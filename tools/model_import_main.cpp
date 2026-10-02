#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "model_import.h"

int main(const int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: relaycore_model_import source.glb output.rcmodel [--metallic-roughness]\n";
        return 2;
    }
    try
    {
        if (std::filesystem::path{argv[2]}.extension() != ".rcmodel")
            throw std::runtime_error("Output must use the .rcmodel extension");
        if (argc == 4 && std::string_view{argv[3]} != "--metallic-roughness")
            throw std::runtime_error("Unknown material option: " + std::string{argv[3]});
        const auto model = RelayCore::tools::import_glb(argv[1], argc == 4);
        RelayCore::render::write_model_asset(argv[2], model);
        std::cout << "Imported " << model.meshes.size() << " meshes, " << model.materials.size()
                  << " opaque materials, " << model.parts.size() << " parts\n";
        return 0;
    } catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
