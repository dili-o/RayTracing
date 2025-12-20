#pragma once
// Vendor
#include <filesystem>

class Renderer;

void load_default_scene(Renderer* renderer);
bool load_scene(const std::filesystem::path &scene_name, Renderer* renderer);
