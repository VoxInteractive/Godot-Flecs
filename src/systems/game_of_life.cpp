#include "game_of_life.h"
#include <cstdlib>
#include <unordered_map>
// Godot utility functions for debug prints
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Temporary component to accumulate neighbor counts each tick
struct NeighborCount
{
    int n;
};

static inline int neighbor_offsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

// Simple global index from (x,y) -> entity for fast neighbor lookup
static std::unordered_map<long long, flecs::entity> g_cell_index;
static int g_grid_w = 0, g_grid_h = 0;
static inline long long key_xy(int x, int y)
{
    return (static_cast<long long>(y) << 32) | (static_cast<unsigned int>(x));
}

void register_gol_systems(flecs::world &world)
{
    // Ensure components are registered (names align with FlecsScript)
    world.component<Cell>()
        .member<int>("x")
        .member<int>("y");
    world.component<Alive>();
    world.component<NeighborCount>().member<int>("n");

    // System: reset neighbor counts to 0 for all cells that have it
    world.system<NeighborCount>("ResetNeighborCounts")
        .kind(flecs::OnUpdate)
        .each([](NeighborCount &nc)
              { nc.n = 0; });

    // For all alive cells, increment neighbor count of the 8 neighbors via index
    world.system<const Cell>("AccumulateNeighborsIndexed")
        .kind(flecs::OnUpdate)
        .with<Alive>()
        .each([](flecs::entity /*alive_e*/, const Cell &alive_cell)
              {
            int cx = alive_cell.x;
            int cy = alive_cell.y;
            for (int i = 0; i < 8; ++i) {
                int nx = cx + neighbor_offsets[i][0];
                int ny = cy + neighbor_offsets[i][1];
                if (nx < 0 || ny < 0 || nx >= g_grid_w || ny >= g_grid_h) continue;
                auto it = g_cell_index.find(key_xy(nx, ny));
                if (it != g_cell_index.end()) {
                    flecs::entity neigh = it->second;
                    auto &nc = neigh.get_mut<NeighborCount>();
                    nc.n += 1;
                    neigh.modified<NeighborCount>();
                }
            } });

    // Apply Life rules
    world.system<Cell, NeighborCount>("ApplyLifeRules")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, Cell &, NeighborCount &nc)
              {
            bool is_alive = e.has<Alive>();
            int n = nc.n;
            if (is_alive) {
                if (n < 2 || n > 3) {
                    e.remove<Alive>();
                }
            } else {
                if (n == 3) {
                    e.add<Alive>();
                }
            } });
}

void init_gol_grid(flecs::world &world, int width, int height, int seed, float alive_probability)
{
    g_cell_index.clear();
    g_grid_w = width;
    g_grid_h = height;

    world.defer_begin();
    std::srand(seed);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto e = world.entity()
                         .set<Cell>({x, y})
                         .set<NeighborCount>({0});
            float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            if (r < alive_probability)
            {
                e.add<Alive>();
            }
            g_cell_index.emplace(key_xy(x, y), e);
        }
    }
    world.defer_end();

    // Ensure at least a small visible pattern exists for visual verification
    auto force_alive = [&](int fx, int fy)
    {
        auto it = g_cell_index.find(key_xy(fx, fy));
        if (it != g_cell_index.end())
        {
            it->second.add<Alive>();
        }
    };
    force_alive(1, 1);
    force_alive(2, 1);
    force_alive(3, 1);
}

void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x, int *out_max_y)
{
    out.clear();
    int max_x = 0, max_y = 0;
    auto q = world.query_builder<const Cell>().with<Alive>().cached().build();
    q.each([&](flecs::entity, const Cell &c)
           {
        out.push_back({c.x, c.y});
        if (c.x > max_x) max_x = c.x;
        if (c.y > max_y) max_y = c.y; });
    // Diagnostic: print how many alive cells were collected and sample the first few
    int found = (int)out.size();
    (void)found; // silently ignore diagnostic count in release
    if (out_max_x)
        *out_max_x = max_x;
    if (out_max_y)
        *out_max_y = max_y;
}
