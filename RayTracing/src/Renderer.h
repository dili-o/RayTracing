#pragma once

#include "Walnut/Image.h"

typedef uint32_t u32;

#include <memory>
#include <glm/glm.hpp>

#include "Camera.h"
#include "Scene.h"

struct Ray;

class Renderer
{
public:
	struct Settings
	{
		bool Accumulate = true;
	};
public:
	Renderer() = default;

	void OnResize(u32 width, u32 height);
	void Render(const Scene& scene, const Camera& camera);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

	void ResetFrameIndex() { m_FrameIndex = 1; }
	Settings& GetSettings() { return m_Settings; }

private:
	struct HitPayload
	{
		float HitDistance;
		glm::vec3 WorldNormal;
		glm::vec3 WorldPosition;

		u32 ObjectIndex;
	};

	glm::vec4 PerPixel(u32 x, u32 y);

	HitPayload TraceRay(const Ray& ray);
	HitPayload ClosestHit(const Ray& ray, float hitDistance, u32 objectIndex);
	HitPayload Miss(const Ray& ray);

private:
	std::shared_ptr<Walnut::Image> m_FinalImage;

	std::vector<u32> m_ImageHorizontalIter, m_ImageVerticalIter;

	const Camera* m_ActiveCamera = nullptr;
	const Scene* m_ActiveScene = nullptr;

	u32* m_ImageData = nullptr;
	glm::vec4* m_AccumulationData = nullptr;

	Settings m_Settings;
	u32 m_FrameIndex = 1;
};
