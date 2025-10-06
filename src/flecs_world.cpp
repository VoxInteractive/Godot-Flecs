#include "flecs_world.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void FlecsWorld::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_age"), &FlecsWorld::get_age);
    ClassDB::bind_method(D_METHOD("set_size", "p_size"), &FlecsWorld::set_size);
    ClassDB::bind_method(D_METHOD("get_size"), &FlecsWorld::get_size);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_size", "get_size");
}

FlecsWorld::FlecsWorld() : FlecsWorld(Vector2i(1600, 900)) {}

FlecsWorld::FlecsWorld(const Vector2i &p_size)
    : size(p_size)
{
    age = 0.0;
}

void FlecsWorld::set_size(const Vector2i &p_size)
{
    size = p_size;
}

Vector2i FlecsWorld::get_size() const
{
    return size;
}

void FlecsWorld::_physics_process(double delta)
{
    age += delta;
}

double FlecsWorld::get_age() const
{
    return age;
}

// void FlecsWorld::set_example(const double p_example)
// {
//     example = p_example;
// }

FlecsWorld::~FlecsWorld()
{
    // Add your cleanup here.
}
