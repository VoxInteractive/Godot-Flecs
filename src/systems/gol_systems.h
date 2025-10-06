#ifndef GOL_SYSTEMS_H
#define GOL_SYSTEMS_H

#include <flecs/distr/flecs.h>
#include <vector>

// Shared component declarations for rendering and systems
struct Cell
{
    int x;
    int y;
};
struct Alive
{
};

// Register Game of Life systems and optionally initialize the grid
void register_gol_systems(flecs::world &world);
void init_gol_grid(flecs::world &world, int width, int height, int seed = 42, float alive_probability = 0.2f);

// Collect positions of alive cells; optionally compute max x/y encountered
struct CellPos
{
    int x;
    int y;
};
void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x = nullptr, int *out_max_y = nullptr);

#endif // GOL_SYSTEMS_H
