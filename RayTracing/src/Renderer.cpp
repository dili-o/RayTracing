#include "Renderer.h"

#include <execution>

#include "Walnut/Random.h"

#include "Ray.h"


namespace Utils {
	static u32 ConvertToRGBA(const glm::vec4& color)
	{
		
		uint8_t r = (uint8_t)(color.r * 255.999f);
		uint8_t g = (uint8_t)(color.g * 255.999f);
		uint8_t b = (uint8_t)(color.b * 255.999f);
		uint8_t a = (uint8_t)(color.a * 255.999f);

		return (u32)a << 24 | (u32) b << 16 | (u32)g << 8 | (u32)r;
	}
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{	
	m_ActiveCamera = &camera;
	m_ActiveScene = &scene;

	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));
#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImageVerticalIter.begin(), m_ImageVerticalIter.end(),
		[this](u32 y) 
		{
#if 0
			std::for_each(std::execution::par, m_ImageHorizontalIter.begin(), m_ImageHorizontalIter.end(),
			[this, y](u32 x)
				{
					glm::vec4 color = PerPixel(x, y);

					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.f), glm::vec4(1.f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
				});
#endif
			for (u32 x = 0; x < m_FinalImage->GetWidth(); ++x)
			{	
				glm::vec4 color = PerPixel(x, y);

				m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

				glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
				accumulatedColor /= (float)m_FrameIndex;

				accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.f), glm::vec4(1.f));
				m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
			}
		});

#else
	for (u32 y = 0; y < m_FinalImage->GetHeight(); ++y)
	{
		for (u32 x = 0; x < m_FinalImage->GetWidth(); ++x)
		{
			glm::vec4 color = PerPixel(x, y);

			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.f), glm::vec4(1.f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif // MT
	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		++m_FrameIndex;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(u32 x, u32 y)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];
	

	glm::vec3 finalColor(0.f);
	float multiplier = 1.0f;
	u32 bounces = 2;
	for (u32 i = 0; i < bounces; ++i) 
	{
		HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.f)
		{
			glm::vec3 skyColor(0.6f, 0.7f, 0.9f);
			finalColor += skyColor * multiplier;
			break;
		}


		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));
		float cosAngle = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f);

		const Sphere& closestSphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[closestSphere.MaterialIndex];
		glm::vec3 sphereColour = material.Albedo;
		sphereColour *= cosAngle;
		finalColor += sphereColour * multiplier;

		multiplier *= 0.5f;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;
		ray.Direction = glm::reflect(ray.Direction,
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}


	return glm::vec4(finalColor, 1.f);
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	u32 closestSphere = UINT32_MAX;
	float hitDistance = FLT_MAX;
	const glm::vec3& rayDirection = ray.Direction;

	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); ++i)
	{
		const Sphere& sphere = m_ActiveScene->Spheres[i];
		const glm::vec3 rayOrigin = ray.Origin - sphere.Origin;

		// Quadratic discriminant
		float a = glm::dot(rayDirection, rayDirection);
		float b = 2.f * glm::dot(rayOrigin, rayDirection);
		float c = glm::dot(rayOrigin, rayOrigin) - sphere.Radius * sphere.Radius;

		float discriminant = (b * b) - (4.f * a * c);

		if (discriminant < 0)
			continue;

		// Quadratic equation (-b +- sqrt(discriminant)) / 2a
		float t1 = (-b - sqrt(discriminant)) / (2.f * a);

		if (t1 > 0.f && t1 < hitDistance)
		{
			closestSphere = (u32)i;
			hitDistance = t1;
		}
	}

	if (closestSphere == UINT32_MAX)
		return Miss(ray);

	return ClosestHit(ray, hitDistance, closestSphere);
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, u32 objectIndex)
{
	HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	const glm::vec3 rayOrigin = ray.Origin - closestSphere.Origin;
	payload.WorldPosition = rayOrigin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	payload.WorldPosition += closestSphere.Origin;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	HitPayload payload;
	payload.HitDistance = -1.f;
	return payload;
}

void Renderer::OnResize(u32 width, u32 height)
{
	if (m_FinalImage)
	{
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;
		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new u32[width * height];
	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIter.resize(width);
	m_ImageVerticalIter.resize(height);

	for (u32 i = 0; i < width; ++i)
		m_ImageHorizontalIter[i] = i;
	for (u32 i = 0; i < height; ++i)
		m_ImageVerticalIter[i] = i;
}
