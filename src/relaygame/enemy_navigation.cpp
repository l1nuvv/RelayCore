#include "relaygame/enemy_navigation.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>

namespace RelayGame
{
    namespace
    {
        constexpr float route_arrival_distance = .12F;

        auto clear_segment(RelayCore::physics::WorldQuery world, glm::vec3 from, glm::vec3 to) -> bool
        {
            // These authored routes are flat. Check the entire footprint and floor, not just eye LOS.
            if (std::abs(from.y - to.y) > .08F || glm::distance(from, to) > 32)
                return false;
            const RelayCore::physics::CollisionConfig collision{.radius = .42F, .height = .95F, .step_height = .15F};
            auto position = from;
            const auto count = std::max(1, static_cast<int>(std::ceil(glm::distance(from, to) / .35F)));
            for (int i = 1; i <= count; ++i)
            {
                const auto desired = glm::mix(from, to, static_cast<float>(i) / static_cast<float>(count));
                const auto move =
                    RelayCore::physics::move_and_collide(world, position, desired - position, 1, collision, true);
                if (glm::distance(move.position, desired) > .03F
                    || !RelayCore::physics::probe_ground(world, move.position, collision))
                    return false;
                position = move.position;
            }
            return true;
        }

        void plan_route(
            RelayCore::physics::WorldQuery world,
            RuntimeEnemy& enemy,
            const glm::vec3& goal,
            float target_eye_height
        )
        {
            enemy.route_count = enemy.route_cursor = 0;
            enemy.route_goal = goal;
            enemy.route_replan_seconds = .5F;
            std::array<glm::vec3, 18> points{};
            points[0] = enemy.state.position;
            std::copy(enemy.route_points.begin(), enemy.route_points.end(), points.begin() + 1);
            const auto end = enemy.route_points.size() + 1;
            points[end] = goal;
            const auto count = end + 1;
            std::array<float, 18> distance;
            distance.fill(std::numeric_limits<float>::max());
            std::array<int, 18> previous;
            previous.fill(-1);
            std::array<bool, 18> visited{};
            distance[0] = 0;
            for (std::size_t pass = 0; pass < count; ++pass)
            {
                std::size_t current = count;
                for (std::size_t i = 0; i < count; ++i)
                    if (!visited[i] && distance[i] < std::numeric_limits<float>::max()
                        && (current == count || distance[i] < distance[current]))
                        current = i;
                if (current == count)
                    break;
                visited[current] = true;
                for (std::size_t next = 1; next < count; ++next)
                {
                    const auto cost = distance[current] + glm::distance(points[current], points[next]);
                    if (!visited[next] && cost < distance[next] && clear_segment(world, points[current], points[next]))
                    {
                        distance[next] = cost;
                        previous[next] = static_cast<int>(current);
                    }
                }
            }
            auto destination = end;
            if (enemy.ranged_attack && !enemy.seeking_last_seen)
            {
                float best = std::numeric_limits<float>::max();
                const auto target = goal + glm::vec3{0, target_eye_height, 0};
                for (std::size_t i = 1; i < end; ++i)
                {
                    const auto eye = points[i] + glm::vec3{0, .75F, 0};
                    const auto delta = target - eye;
                    const auto range = glm::length(delta);
                    // Arrival can stop short of the point; keep that entire radius inside firing range.
                    if (range < 3 || range > mobile_shooter_range - route_arrival_distance
                        || distance[i] == std::numeric_limits<float>::max()
                        || world.raycast(eye, delta / range, range).has_value())
                        continue;
                    const auto score = distance[i] + std::abs(range - 7);
                    if (score < best)
                    {
                        best = score;
                        destination = i;
                    }
                }
            }
            if (previous[destination] < 0)
                return;
            while (destination != 0)
            {
                enemy.route[enemy.route_count++] = points[destination];
                destination = static_cast<std::size_t>(previous[destination]);
            }
            std::reverse(enemy.route.begin(), enemy.route.begin() + static_cast<std::ptrdiff_t>(enemy.route_count));
        }
    } // namespace

    auto enemy_route_velocity(
        RelayCore::physics::WorldQuery world,
        RuntimeEnemy& enemy,
        const glm::vec3& goal,
        float target_eye_height,
        float dt
    ) -> glm::vec3
    {
        enemy.route_replan_seconds = std::max(0.0F, enemy.route_replan_seconds - dt);
        if (enemy.route_replan_seconds == 0 || glm::distance(goal, enemy.route_goal) > 1)
            plan_route(world, enemy, goal, target_eye_height);
        while (enemy.route_cursor < enemy.route_count
               && glm::distance(enemy.state.position, enemy.route[enemy.route_cursor]) < route_arrival_distance)
            ++enemy.route_cursor;
        if (enemy.route_cursor == enemy.route_count)
        {
            if (enemy.ranged_attack && enemy.route_count > 0 && !enemy.seeking_last_seen)
            {
                enemy.seeking_last_seen = true;
                enemy.route_replan_seconds = 0;
            }
            return {};
        }
        auto delta = enemy.route[enemy.route_cursor] - enemy.state.position;
        delta.y = 0;
        const auto distance = glm::length(delta);
        return distance > .001F ? delta / distance * std::min(enemy.move_speed, distance / dt) : glm::vec3{};
    }
} // namespace RelayGame
