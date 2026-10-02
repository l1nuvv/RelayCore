#include "model_import.h"

#include <cmath>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/matrix.hpp>
#include <map>
#include <numeric>
#include <stdexcept>
#include <wincodec.h>
#include <wrl/client.h>

namespace RelayCore::tools
{
    namespace
    {
        void require(const bool condition, const std::string& message)
        {
            if (!condition)
                throw std::runtime_error(message);
        }

        void require_com(const HRESULT result, const char* operation)
        {
            require(SUCCEEDED(result), std::string(operation) + ": HRESULT " + std::to_string(result));
        }

        auto buffer_bytes(const fastgltf::Buffer& buffer) -> std::span<const std::byte>
        {
            return std::visit(
                [](const auto& source) -> std::span<const std::byte> {
                    if constexpr (requires { source.bytes; })
                        return {source.bytes.data(), source.bytes.size()};
                    else
                        throw std::runtime_error("GLB requires embedded buffers; external buffer URI is unsupported");
                },
                buffer.data
            );
        }

        auto view_bytes(const fastgltf::Asset& asset, const std::size_t index) -> std::span<const std::byte>
        {
            require(index < asset.bufferViews.size(), "missing GLB buffer view");
            const auto& view = asset.bufferViews[index];
            require(view.bufferIndex < asset.buffers.size(), "missing GLB buffer");
            const auto bytes = buffer_bytes(asset.buffers[view.bufferIndex]);
            require(
                view.byteOffset <= bytes.size() && view.byteLength <= bytes.size() - view.byteOffset,
                "GLB buffer view exceeds embedded buffer"
            );
            return bytes.subspan(view.byteOffset, view.byteLength);
        }

        void check_accessor(
            const fastgltf::Asset& asset,
            const std::size_t index,
            const fastgltf::AccessorType type,
            const bool indices = false
        )
        {
            require(index < asset.accessors.size(), "missing GLB accessor");
            const auto& accessor = asset.accessors[index];
            require(
                accessor.type == type && accessor.count > 0 && accessor.count <= 6000000,
                "GLB accessor has invalid type/count"
            );
            require(
                !accessor.sparse && accessor.bufferViewIndex.has_value(),
                "sparse/implicit GLB accessors are unsupported"
            );
            require(
                !accessor.normalized,
                "normalized GLB attributes are unsupported; export floating-point attributes"
            );
            require(
                indices ? accessor.componentType == fastgltf::ComponentType::UnsignedByte
                              || accessor.componentType == fastgltf::ComponentType::UnsignedShort
                              || accessor.componentType == fastgltf::ComponentType::UnsignedInt
                        : accessor.componentType == fastgltf::ComponentType::Float,
                "GLB accessor requires float attributes or unsigned integer indices"
            );
            const auto bytes = view_bytes(asset, *accessor.bufferViewIndex);
            const auto& view = asset.bufferViews[*accessor.bufferViewIndex];
            const auto element = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
            const auto stride = view.byteStride.value_or(element);
            require(
                stride >= element && accessor.byteOffset <= bytes.size()
                    && element <= bytes.size() - accessor.byteOffset
                    && accessor.count - 1 <= (bytes.size() - accessor.byteOffset - element) / stride,
                "GLB accessor exceeds buffer view"
            );
        }

        auto decode_image(const std::span<const std::byte> bytes) -> render::CpuRgbaTextureData
        {
            using Microsoft::WRL::ComPtr;
            require(!bytes.empty() && bytes.size() <= 256 * 1024 * 1024, "invalid embedded image size");
            ComPtr<IWICImagingFactory> factory;
            require_com(
                CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)),
                "Create WIC image decoder"
            );
            ComPtr<IWICStream> stream;
            require_com(factory->CreateStream(&stream), "Create image stream");
            require_com(
                stream->InitializeFromMemory(
                    reinterpret_cast<BYTE*>(const_cast<std::byte*>(bytes.data())),
                    static_cast<DWORD>(bytes.size())
                ),
                "Read embedded image"
            );
            ComPtr<IWICBitmapDecoder> decoder;
            require_com(
                factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder),
                "Decode embedded PNG/JPEG"
            );
            GUID format;
            require_com(decoder->GetContainerFormat(&format), "Read image format");
            require(
                format == GUID_ContainerFormatPng || format == GUID_ContainerFormatJpeg,
                "model images must be embedded PNG/JPEG"
            );
            UINT frames = 0;
            require_com(decoder->GetFrameCount(&frames), "Read image frame count");
            require(frames == 1, "base-color image must have one frame");
            ComPtr<IWICBitmapFrameDecode> frame;
            require_com(decoder->GetFrame(0, &frame), "Read image frame");
            UINT width = 0, height = 0;
            require_com(frame->GetSize(&width, &height), "Read image dimensions");
            require(
                width > 0 && width <= 4096 && height > 0 && height <= 4096,
                "base-color dimensions must be 1..4096"
            );
            ComPtr<IWICFormatConverter> converter;
            require_com(factory->CreateFormatConverter(&converter), "Create RGBA converter");
            require_com(
                converter->Initialize(
                    frame.Get(),
                    GUID_WICPixelFormat32bppRGBA,
                    WICBitmapDitherTypeNone,
                    nullptr,
                    0,
                    WICBitmapPaletteTypeCustom
                ),
                "Convert image to RGBA"
            );
            render::CpuRgbaTextureData result{
                static_cast<int>(width),
                static_cast<int>(height),
                std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)
            };
            require_com(
                converter
                    ->CopyPixels(nullptr, width * 4, static_cast<UINT>(result.pixels.size()), result.pixels.data()),
                "Read RGBA pixels"
            );
            return result;
        }

        auto import_texture(const fastgltf::Asset& asset, const fastgltf::TextureInfo& info)
            -> render::CpuRgbaTextureData
        {
            require(info.texCoordIndex == 0 && !info.transform, "model requires TEXCOORD_0 without texture transform");
            require(info.textureIndex < asset.textures.size(), "missing model texture");
            const auto& texture = asset.textures[info.textureIndex];
            require(texture.imageIndex && *texture.imageIndex < asset.images.size(), "missing model image");
            if (texture.samplerIndex)
            {
                require(*texture.samplerIndex < asset.samplers.size(), "missing texture sampler");
                const auto& sampler = asset.samplers[*texture.samplerIndex];
                require(
                    sampler.wrapS == fastgltf::Wrap::Repeat && sampler.wrapT == fastgltf::Wrap::Repeat
                        && (!sampler.magFilter || *sampler.magFilter == fastgltf::Filter::Linear)
                        && (!sampler.minFilter || *sampler.minFilter == fastgltf::Filter::Linear
                            || *sampler.minFilter == fastgltf::Filter::LinearMipMapLinear),
                    "model sampler requires repeat and linear/trilinear filtering"
                );
            }
            const auto* image = std::get_if<fastgltf::sources::BufferView>(&asset.images[*texture.imageIndex].data);
            require(image != nullptr, "GLB model images must be embedded buffer views");
            return decode_image(view_bytes(asset, image->bufferViewIndex));
        }

        auto
        import_material(const fastgltf::Asset& asset, const fastgltf::Material& material, const bool metallic_roughness)
            -> render::ModelMaterial
        {
            require(
                material.alphaMode == fastgltf::AlphaMode::Opaque && material.pbrData.baseColorFactor[3] == 1,
                "only opaque base-color materials are supported"
            );
            require(!material.doubleSided, "double-sided materials are unsupported; enable backface culling on export");
            require(
                !material.occlusionTexture && !material.emissiveTexture && !material.pbrData.metallicRoughnessTexture
                    && material.emissiveFactor == fastgltf::math::nvec3(0),
                "occlusion/emissive/metallic-roughness maps are outside the normal-map importer"
            );
            render::ModelMaterial result{{1, 1, {255, 255, 255, 255}}};
            if (material.pbrData.baseColorTexture)
            {
                result.base_color = import_texture(asset, *material.pbrData.baseColorTexture);
            }
            if (material.normalTexture)
            {
                result.normal = import_texture(asset, *material.normalTexture);
                result.normal_scale = material.normalTexture->scale;
                require(std::isfinite(result.normal_scale), "normal texture scale must be finite");
            }
            if (metallic_roughness)
                result.metallic_roughness =
                    render::MetallicRoughness{material.pbrData.roughnessFactor, material.pbrData.metallicFactor};
            for (int channel = 0; channel < 3; ++channel)
                require(
                    std::isfinite(material.pbrData.baseColorFactor[channel])
                        && material.pbrData.baseColorFactor[channel] >= 0
                        && material.pbrData.baseColorFactor[channel] <= 1,
                    "base-color factor must be finite and in [0, 1]"
                );
            // glTF factors are linear; the native product stores opaque visual-sRGB texels.
            for (std::size_t i = 0; i < result.base_color.pixels.size(); i += 4)
            {
                require(result.base_color.pixels[i + 3] == 255, "base-color image must be opaque");
                for (int channel = 0; channel < 3; ++channel)
                {
                    const auto srgb = result.base_color.pixels[i + channel] / 255.0F;
                    auto linear = srgb <= .04045F ? srgb / 12.92F : std::pow((srgb + .055F) / 1.055F, 2.4F);
                    linear *= material.pbrData.baseColorFactor[channel];
                    const auto encoded =
                        linear <= .0031308F ? linear * 12.92F : 1.055F * std::pow(linear, 1.0F / 2.4F) - .055F;
                    result.base_color.pixels[i + channel] = static_cast<std::uint8_t>(std::lround(encoded * 255));
                }
            }
            return result;
        }
    } // namespace

    auto import_glb(const std::filesystem::path& path, const bool metallic_roughness) -> render::ModelAssetData
    {
        try
        {
            require(path.extension() == ".glb", "model importer expects a static .glb source");
            require(std::filesystem::file_size(path) <= 256 * 1024 * 1024, "GLB exceeds 256 MiB");
            auto data = fastgltf::GltfDataBuffer::FromPath(path);
            require(data.error() == fastgltf::Error::None, std::string(fastgltf::getErrorMessage(data.error())));
            fastgltf::Parser parser;
            auto parsed = parser.loadGltfBinary(data.get(), path.parent_path(), fastgltf::Options::None);
            require(parsed.error() == fastgltf::Error::None, std::string(fastgltf::getErrorMessage(parsed.error())));
            auto& asset = parsed.get();
            const auto validity = fastgltf::validate(asset);
            require(validity == fastgltf::Error::None, std::string(fastgltf::getErrorMessage(validity)));
            require(asset.extensionsUsed.empty(), "GLB extensions are unsupported; export core glTF 2.0");
            require(
                asset.animations.empty() && asset.skins.empty(),
                "static model importer does not accept animation/skins"
            );
            require(asset.defaultScene && *asset.defaultScene < asset.scenes.size(), "GLB requires a default scene");
            struct ComLifetime {
                ComLifetime()
                {
                    require_com(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Initialize image decoding");
                }
                ~ComLifetime()
                {
                    CoUninitialize();
                }
            } com;
            render::ModelAssetData result;
            std::map<std::size_t, std::uint32_t> materials;
            std::map<std::pair<std::size_t, std::size_t>, std::uint32_t> meshes;
            std::map<std::uint32_t, std::uint32_t> mirrored_meshes;
            std::vector<bool> visited(asset.nodes.size());
            const auto visit =
                [&](const std::size_t index, const fastgltf::math::fmat4x4& parent, const std::size_t depth, auto& self
                ) -> void {
                require(index < asset.nodes.size() && depth <= 256, "GLB node reference/depth is invalid");
                require(!visited[index], "GLB scene has cyclic or multiply-parented nodes");
                visited[index] = true;
                const auto& node = asset.nodes[index];
                require(!node.skinIndex && node.weights.empty(), "static model node has skin/morph weights");
                const auto matrix = fastgltf::getTransformMatrix(node, parent);
                glm::mat4 transform;
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                        transform[c][r] = matrix[c][r];
                if (node.meshIndex)
                {
                    require(*node.meshIndex < asset.meshes.size(), "missing node mesh");
                    const auto& source = asset.meshes[*node.meshIndex];
                    for (std::size_t p = 0; p < source.primitives.size(); ++p)
                    {
                        const auto& primitive = source.primitives[p];
                        require(
                            primitive.type == fastgltf::PrimitiveType::Triangles && primitive.targets.empty(),
                            "static model requires triangles without morph targets"
                        );
                        const auto key = std::pair{*node.meshIndex, p};
                        if (!meshes.contains(key))
                        {
                            render::MeshAsset mesh;
                            const auto positions = primitive.findAttribute("POSITION"),
                                       normals = primitive.findAttribute("NORMAL");
                            require(
                                positions != primitive.attributes.end() && normals != primitive.attributes.end(),
                                "mesh primitive requires POSITION and NORMAL"
                            );
                            check_accessor(asset, positions->accessorIndex, fastgltf::AccessorType::Vec3);
                            check_accessor(asset, normals->accessorIndex, fastgltf::AccessorType::Vec3);
                            const auto count = asset.accessors[positions->accessorIndex].count;
                            require(
                                count <= 2000000 && asset.accessors[normals->accessorIndex].count == count,
                                "POSITION/NORMAL counts differ or exceed model limits"
                            );
                            mesh.geometry.vertices.resize(count);
                            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                                asset,
                                asset.accessors[positions->accessorIndex],
                                [&](const glm::vec3 value, const std::size_t i) {
                                    mesh.geometry.vertices[i].position = value;
                                    mesh.geometry.vertices[i].color = glm::vec3{1};
                                }
                            );
                            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                                asset,
                                asset.accessors[normals->accessorIndex],
                                [&](const glm::vec3 value, const std::size_t i) {
                                    mesh.geometry.vertices[i].normal = value;
                                }
                            );
                            const auto uv = primitive.findAttribute("TEXCOORD_0");
                            if (uv != primitive.attributes.end())
                            {
                                check_accessor(asset, uv->accessorIndex, fastgltf::AccessorType::Vec2);
                                require(asset.accessors[uv->accessorIndex].count == count, "UV/POSITION counts differ");
                                fastgltf::iterateAccessorWithIndex<glm::vec2>(
                                    asset,
                                    asset.accessors[uv->accessorIndex],
                                    [&](const glm::vec2 value, const std::size_t i) {
                                        mesh.geometry.vertices[i].uv = value;
                                    }
                                );
                            }
                            const auto tangent = primitive.findAttribute("TANGENT");
                            if (tangent != primitive.attributes.end())
                            {
                                check_accessor(asset, tangent->accessorIndex, fastgltf::AccessorType::Vec4);
                                require(
                                    asset.accessors[tangent->accessorIndex].count == count,
                                    "TANGENT/POSITION counts differ"
                                );
                                mesh.has_tangents = true;
                                fastgltf::iterateAccessorWithIndex<glm::vec4>(
                                    asset,
                                    asset.accessors[tangent->accessorIndex],
                                    [&](const glm::vec4 value, const std::size_t i) {
                                        mesh.geometry.vertices[i].tangent = value;
                                    }
                                );
                            }
                            for (const auto& attribute : primitive.attributes)
                                require(
                                    attribute.name == "POSITION" || attribute.name == "NORMAL"
                                        || attribute.name == "TEXCOORD_0" || attribute.name == "TANGENT",
                                    "unsupported mesh attribute " + std::string(attribute.name)
                                );
                            if (primitive.indicesAccessor)
                            {
                                check_accessor(asset, *primitive.indicesAccessor, fastgltf::AccessorType::Scalar, true);
                                fastgltf::iterateAccessor<std::uint32_t>(
                                    asset,
                                    asset.accessors[*primitive.indicesAccessor],
                                    [&](const std::uint32_t value) {
                                        mesh.geometry.indices.push_back(value);
                                    }
                                );
                            } else
                            {
                                mesh.geometry.indices.resize(count);
                                std::iota(mesh.geometry.indices.begin(), mesh.geometry.indices.end(), 0u);
                            }
                            require(
                                !mesh.geometry.indices.empty() && mesh.geometry.indices.size() % 3 == 0,
                                "primitive triangle index count must be divisible by three"
                            );
                            meshes.emplace(key, static_cast<std::uint32_t>(result.meshes.size()));
                            result.meshes.push_back(std::move(mesh));
                        }
                        const auto material = primitive.materialIndex.value_or(asset.materials.size());
                        require(material <= asset.materials.size(), "missing primitive material");
                        if (!materials.contains(material))
                        {
                            materials.emplace(material, static_cast<std::uint32_t>(result.materials.size()));
                            if (material == asset.materials.size())
                                result.materials.push_back(
                                    import_material(asset, fastgltf::Material{}, metallic_roughness)
                                );
                            else
                                result.materials.push_back(
                                    import_material(asset, asset.materials[material], metallic_roughness)
                                );
                        }
                        if (material < asset.materials.size()
                            && (asset.materials[material].pbrData.baseColorTexture
                                || asset.materials[material].normalTexture))
                            require(
                                primitive.findAttribute("TEXCOORD_0") != primitive.attributes.end(),
                                "textured primitive requires TEXCOORD_0"
                            );
                        require(
                            !result.materials[materials.at(material)].normal
                                || result.meshes[meshes.at(key)].has_tangents,
                            "normal-mapped primitive requires exported TANGENT (enable tangents in Blender)"
                        );
                        render::ModelPart part{meshes.at(key), materials.at(material), transform};
                        if (glm::determinant(glm::mat3{transform}) < 0)
                        {
                            // Keep renderer winding fixed while preserving mirrored instance geometry.
                            if (!mirrored_meshes.contains(part.mesh))
                            {
                                auto mesh = result.meshes[part.mesh];
                                for (auto& vertex : mesh.geometry.vertices)
                                {
                                    vertex.position.x = -vertex.position.x;
                                    vertex.normal.x = -vertex.normal.x;
                                    vertex.tangent.x = -vertex.tangent.x;
                                    vertex.tangent.w = -vertex.tangent.w;
                                }
                                for (std::size_t i = 0; i < mesh.geometry.indices.size(); i += 3)
                                    std::swap(mesh.geometry.indices[i + 1], mesh.geometry.indices[i + 2]);
                                mirrored_meshes.emplace(part.mesh, static_cast<std::uint32_t>(result.meshes.size()));
                                result.meshes.push_back(std::move(mesh));
                            }
                            part.mesh = mirrored_meshes.at(part.mesh);
                            part.transform[0] *= -1;
                        }
                        result.parts.push_back(part);
                    }
                }
                for (const auto child : node.children)
                    self(child, matrix, depth + 1, self);
            };
            for (const auto node : asset.scenes[*asset.defaultScene].nodeIndices)
                visit(node, fastgltf::math::fmat4x4(1), 0, visit);
            render::validate_model_asset(result);
            return result;
        } catch (const std::exception& error)
        {
            throw std::runtime_error(path.string() + ": " + error.what());
        }
    }
} // namespace RelayCore::tools
