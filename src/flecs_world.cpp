
#include "flecs_world.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <algorithm>

using namespace godot;

void FlecsWorld::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_age"), &FlecsWorld::get_age);
    ClassDB::bind_method(D_METHOD("get_size_factor"), &FlecsWorld::get_size_factor);
    ClassDB::bind_method(D_METHOD("set_size_factor", "p_size_factor"), &FlecsWorld::set_size_factor);
    // Make size_factor editable in the Editor with range 0.25 - 4.0
    ADD_PROPERTY(
        PropertyInfo(Variant::FLOAT, "size_factor", PROPERTY_HINT_RANGE, "0.2,4.0,0.2", PROPERTY_USAGE_DEFAULT),
        "set_size_factor",
        "get_size_factor");
}

FlecsWorld::FlecsWorld()
{
    age = 0.0;
    set_size_factor(size_factor);
}

double FlecsWorld::get_age() const
{
    return age;
}

double FlecsWorld::get_size_factor() const
{
    return size_factor;
}

void FlecsWorld::set_size_factor(double p_size_factor)
{
    size_factor = std::clamp(p_size_factor, 0.25, 4.0);
    const Vector2i project_viewport_size = Vector2i(
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width"),
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height"));
    size = Vector2i(
        static_cast<int>(project_viewport_size.x * size_factor),
        static_cast<int>(project_viewport_size.y * size_factor));
    godot::UtilityFunctions::print("FlecsWorld size: ", size);
}

void FlecsWorld::_ready()
{
    // Print world size for verification
    godot::UtilityFunctions::print("FlecsWorld initialised.");
}

void FlecsWorld::_physics_process(double delta)
{
    age += delta;
}

FlecsWorld::~FlecsWorld()
{
    // Add your cleanup here.
}
