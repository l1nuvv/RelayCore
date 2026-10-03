#include "relaycore/physics/physics_world.h"

#include "relaycore/core/profiling.h"

// Jolt's umbrella header must precede its other headers.
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on

#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace RelayCore::physics
{
    namespace
    {
        struct JoltRuntime {
            JoltRuntime()
            {
                if (JPH::Factory::sInstance)
                    throw std::logic_error("RelayCore must own Jolt type registration");
                JPH::RegisterDefaultAllocator();
                JPH::Factory::sInstance = new JPH::Factory;
                JPH::RegisterTypes();
            }
            ~JoltRuntime()
            {
                JPH::UnregisterTypes();
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
            }
        };

        // Registration outlives every world, including simultaneously staged and active sessions.
        void initialize_jolt()
        {
            static const JoltRuntime runtime;
        }

        class StaticBroadPhase final : public JPH::BroadPhaseLayerInterface {
            auto GetNumBroadPhaseLayers() const -> JPH::uint override
            {
                return 1;
            }
            auto GetBroadPhaseLayer(JPH::ObjectLayer) const -> JPH::BroadPhaseLayer override
            {
                return JPH::BroadPhaseLayer{0};
            }
        };

        class StaticPairFilter final : public JPH::ObjectLayerPairFilter {
            auto ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const -> bool override
            {
                return false;
            }
        };

        class QueryLayers final : public JPH::ObjectLayerFilter {
        public:
            explicit QueryLayers(WorldQueryMask mask) : mask_(static_cast<JPH::ObjectLayer>(mask)) {}
            auto ShouldCollide(JPH::ObjectLayer layer) const -> bool override
            {
                return (mask_ & layer) != 0;
            }

        private:
            JPH::ObjectLayer mask_;
        };

        auto to_jolt(const glm::vec3& value) -> JPH::Vec3
        {
            return {value.x, value.y, value.z};
        }
        auto from_jolt(JPH::Vec3Arg value) -> glm::vec3
        {
            return {value.GetX(), value.GetY(), value.GetZ()};
        }

        void validate_vector(const glm::vec3& value, const char* field)
        {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
                throw std::invalid_argument(std::string{"PhysicsWorld: non-finite "} + field);
        }

        void validate_box(const glm::vec3& centre, const glm::vec3& half_extents)
        {
            validate_vector(centre, "box centre");
            validate_vector(half_extents, "box half extents");
            if (half_extents.x <= 0 || half_extents.y <= 0 || half_extents.z <= 0)
                throw std::invalid_argument("PhysicsWorld: box half extents must be positive");
        }
    } // namespace

    struct PhysicsWorld::Impl {
        struct Metadata {
            WorldObjectId object;
            SurfaceMaterial surface{};
            std::vector<SurfaceMaterial> triangle_surfaces;
        };
        StaticBroadPhase broad_phase;
        JPH::ObjectVsBroadPhaseLayerFilter broad_phase_filter;
        StaticPairFilter pair_filter;
        JPH::PhysicsSystem system;
        std::vector<JPH::BodyID> bodies;
        std::vector<Metadata> metadata;
        bool added = false;

        explicit Impl(std::size_t count)
        {
            if (count > JPH::BodyID::cMaxBodyIndex)
                throw std::invalid_argument("PhysicsWorld: too many fixture bodies");
            system.Init(
                static_cast<JPH::uint>(std::max<std::size_t>(1, count)),
                0,
                1,
                1,
                broad_phase,
                broad_phase_filter,
                pair_filter
            );
            bodies.reserve(count);
            metadata.reserve(count);
        }

        ~Impl()
        {
            auto& interface = system.GetBodyInterface();
            if (added)
                interface.RemoveBodies(bodies.data(), static_cast<int>(bodies.size()));
            interface.DestroyBodies(bodies.data(), static_cast<int>(bodies.size()));
        }

        void add(const JPH::Shape* shape, const glm::vec3& position, Metadata data)
        {
            JPH::BodyCreationSettings settings{
                shape,
                to_jolt(position),
                JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,
                static_cast<JPH::ObjectLayer>(query_layer(data.object.kind))
            };
            settings.mUserData = metadata.size();
            metadata.push_back(std::move(data));
            auto* body = system.GetBodyInterface().CreateBody(settings);
            if (!body)
                throw std::runtime_error("PhysicsWorld: failed to create static fixture body");
            bodies.push_back(body->GetID());
        }

        void activate()
        {
            if (bodies.empty())
                return;
            auto& interface = system.GetBodyInterface();
            const auto count = static_cast<int>(bodies.size());
            auto state = interface.AddBodiesPrepare(bodies.data(), count);
            interface.AddBodiesFinalize(bodies.data(), count, state, JPH::EActivation::DontActivate);
            added = true;
        }

        auto
        hit(const JPH::Body& body,
            const JPH::SubShapeID& sub_shape,
            float distance,
            float fraction,
            const glm::vec3& position,
            const glm::vec3& normal) const -> WorldQueryHit
        {
            const auto& data = metadata.at(static_cast<std::size_t>(body.GetUserData()));
            auto surface = data.surface;
            if (!data.triangle_surfaces.empty())
            {
                const auto& mesh = static_cast<const JPH::MeshShape&>(*body.GetShape());
                surface = data.triangle_surfaces.at(mesh.GetTriangleUserData(sub_shape));
            }
            return {distance, fraction, position, normal, surface, data.object};
        }
    };

    PhysicsWorld::PhysicsWorld(const std::span<const WorldCollider> boxes, const std::span<const PhysicsMesh> meshes)
    {
        initialize_jolt();
        impl_ = std::make_unique<Impl>(boxes.size() + meshes.size());
        for (const auto& box : boxes)
        {
            const auto half_extents = (box.bounds.maxs - box.bounds.mins) * .5F;
            const auto centre = (box.bounds.maxs + box.bounds.mins) * .5F;
            validate_box(centre, half_extents);
            JPH::BoxShapeSettings settings{to_jolt(half_extents), 0};
            const auto shape = settings.Create();
            if (shape.HasError())
                throw std::invalid_argument("PhysicsWorld: box shape: " + std::string{shape.GetError()});
            impl_->add(shape.Get(), centre, {.object = box.object, .surface = box.surface});
        }
        for (const auto& mesh : meshes)
        {
            if (mesh.triangles.empty())
                throw std::invalid_argument("PhysicsWorld: mesh has no triangles");
            if (mesh.triangles.size() > std::numeric_limits<JPH::uint32>::max())
                throw std::invalid_argument("PhysicsWorld: too many mesh triangles");
            JPH::TriangleList triangles;
            Impl::Metadata data{.object = mesh.object};
            triangles.reserve(mesh.triangles.size());
            data.triangle_surfaces.reserve(mesh.triangles.size());
            for (const auto& triangle : mesh.triangles)
            {
                validate_vector(triangle.a, "triangle vertex");
                validate_vector(triangle.b, "triangle vertex");
                validate_vector(triangle.c, "triangle vertex");
                const auto area = glm::cross(triangle.b - triangle.a, triangle.c - triangle.a);
                if (!std::isfinite(glm::dot(area, area)) || glm::dot(area, area) <= 1e-12F)
                    throw std::invalid_argument("PhysicsWorld: degenerate mesh triangle");
                triangles.emplace_back(
                    to_jolt(triangle.a),
                    to_jolt(triangle.b),
                    to_jolt(triangle.c),
                    0,
                    static_cast<JPH::uint32>(data.triangle_surfaces.size())
                );
                data.triangle_surfaces.push_back(triangle.surface);
            }
            JPH::MeshShapeSettings settings{triangles};
            settings.mPerTriangleUserData = true;
            const auto shape = settings.Create();
            if (shape.HasError())
                throw std::invalid_argument("PhysicsWorld: mesh shape: " + std::string{shape.GetError()});
            impl_->add(shape.Get(), {}, std::move(data));
        }
        impl_->activate();
    }

    PhysicsWorld::~PhysicsWorld() = default;

    auto PhysicsWorld::raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const float max_distance,
        const WorldQueryMask mask
    ) const -> std::optional<WorldQueryHit>
    {
        RELAYCORE_PROFILE_FUNCTION();
        validate_vector(origin, "ray origin");
        validate_vector(direction, "ray direction");
        if (!std::isfinite(max_distance) || std::abs(glm::dot(direction, direction) - 1) > .0001F)
            throw std::invalid_argument("PhysicsWorld: ray requires a unit direction and finite distance");
        if (max_distance < 0)
            return {};
        const JPH::RRayCast ray{to_jolt(origin), to_jolt(direction * max_distance)};
        JPH::RayCastResult result;
        if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, result, {}, QueryLayers{mask}))
            return {};
        const JPH::BodyLockRead lock{impl_->system.GetBodyLockInterface(), result.mBodyID};
        if (!lock.Succeeded())
            throw std::logic_error("PhysicsWorld: static ray body disappeared");
        const auto& body = lock.GetBody();
        const auto point = ray.GetPointOnRay(result.mFraction);
        const auto normal = result.mFraction == 0
                                ? glm::vec3{}
                                : from_jolt(body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
        return impl_->hit(
            body,
            result.mSubShapeID2,
            result.mFraction * max_distance,
            result.mFraction,
            from_jolt(point),
            normal
        );
    }

    auto PhysicsWorld::cast_box(
        const glm::vec3& centre,
        const glm::vec3& half_extents,
        const glm::vec3& displacement,
        const WorldQueryMask mask
    ) const -> std::optional<WorldQueryHit>
    {
        RELAYCORE_PROFILE_FUNCTION();
        validate_box(centre, half_extents);
        validate_vector(displacement, "box displacement");
        if (displacement == glm::vec3{})
            return overlap_box(centre, half_extents, mask);
        const JPH::BoxShape shape{to_jolt(half_extents), 0};
        const JPH::RShapeCast
            cast{&shape, JPH::Vec3::sReplicate(1), JPH::RMat44::sTranslation(to_jolt(centre)), to_jolt(displacement)};
        JPH::ShapeCastSettings settings;
        settings.SetBackFaceMode(JPH::EBackFaceMode::CollideWithBackFaces);
        settings.mReturnDeepestPoint = true;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        impl_->system.GetNarrowPhaseQuery()
            .CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, QueryLayers{mask});
        if (!collector.HadHit())
            return {};
        const auto& result = collector.mHit;
        const JPH::BodyLockRead lock{impl_->system.GetBodyLockInterface(), result.mBodyID2};
        if (!lock.Succeeded())
            throw std::logic_error("PhysicsWorld: static cast body disappeared");
        return impl_->hit(
            lock.GetBody(),
            result.mSubShapeID2,
            result.mFraction * glm::length(displacement),
            result.mFraction,
            from_jolt(result.mContactPointOn2),
            from_jolt(-result.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero()))
        );
    }

    auto
    PhysicsWorld::overlap_box(const glm::vec3& centre, const glm::vec3& half_extents, const WorldQueryMask mask) const
        -> std::optional<WorldQueryHit>
    {
        RELAYCORE_PROFILE_FUNCTION();
        validate_box(centre, half_extents);
        const JPH::BoxShape shape{to_jolt(half_extents), 0};
        JPH::CollideShapeSettings settings;
        settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> collector;
        impl_->system.GetNarrowPhaseQuery().CollideShape(
            &shape,
            JPH::Vec3::sReplicate(1),
            JPH::RMat44::sTranslation(to_jolt(centre)),
            settings,
            JPH::RVec3::sZero(),
            collector,
            {},
            QueryLayers{mask}
        );
        if (!collector.HadHit())
            return {};
        const auto& result = collector.mHit;
        const JPH::BodyLockRead lock{impl_->system.GetBodyLockInterface(), result.mBodyID2};
        if (!lock.Succeeded())
            throw std::logic_error("PhysicsWorld: static overlap body disappeared");
        return impl_->hit(
            lock.GetBody(),
            result.mSubShapeID2,
            0,
            0,
            from_jolt(result.mContactPointOn2),
            from_jolt(-result.mPenetrationAxis.NormalizedOr(JPH::Vec3::sZero()))
        );
    }
} // namespace RelayCore::physics
