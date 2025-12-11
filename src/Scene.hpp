#pragma once
// Vendor
#include <string>

class Renderer;

void load_default_scene(Renderer* renderer);
bool load_scene(std::string scene_name, Renderer* renderer);
