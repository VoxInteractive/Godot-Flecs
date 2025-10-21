#ifndef GOL_SYSTEMS_H
#define GOL_SYSTEMS_H

#include <flecs.h>
#include <vector>
#include <godot_cpp/classes/random_number_generator.hpp>

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
void register_game_of_life_systems(flecs::world &world);
void init_game_of_life_grid(flecs::world &world, int width, int height, int seed = 42, float alive_probability = 0.2f);

// Collect positions of alive cells; optionally compute max x/y encountered
struct CellPos
{
    int x;
    int y;
};
void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x = nullptr, int *out_max_y = nullptr);

// Optimized simulation buffer accessors (row-major, size = width*height):
// Returns non-owning pointer to current alive buffer (0 or 1 per cell).
const uint8_t *game_of_life_alive_data();
int game_of_life_width();
int game_of_life_height();

#endif // GOL_SYSTEMS_H
