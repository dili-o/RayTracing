#pragma once
#include <vector>
#include <glm/glm.hpp>

struct Material
{
	glm::vec3 Albedo{ 1.0f };
	float Roughness = 1.0f;
	float Metallic = 0.0f;
};

struct Sphere
{
	glm::vec3 Origin{ 0.0f };
	float Radius = 0.5f;

	u32 MaterialIndex = 0;
};


struct Scene
{
	std::vector<Sphere> Spheres;
	std::vector<Material> Materials;
};
