#ifndef FLECS_REGISTER_TYPES_H
#define FLECS_REGISTER_TYPES_H

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_flecs_module(ModuleInitializationLevel p_level);
void uninitialize_flecs_module(ModuleInitializationLevel p_level);

#endif // FLECS_REGISTER_TYPES_H
