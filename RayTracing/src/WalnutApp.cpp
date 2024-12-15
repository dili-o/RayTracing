#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Timer.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

#include <glm/gtc/type_ptr.hpp>

typedef uint32_t u32;

using namespace Walnut;
using glm::vec2;
using glm::vec3;
using glm::ivec3;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.f, 0.1f, 100.f)
	{
		{
			Material material1 = { {1.f, 1.f, 0.f}, 0.0f };
			Material material2 = { {1.f, 0.f, 0.f}, 0.1f };
			m_Scene.Materials.push_back(material1);
			m_Scene.Materials.push_back(material2);
		}

		{
			Sphere sphere;
			sphere.Origin = glm::vec3(0.f, 1.f, 0.f);
			sphere.Radius = 1.f;
			sphere.MaterialIndex = 0;
			m_Scene.Spheres.push_back(sphere);
		}
		{
			Sphere sphere;
			sphere.Origin = glm::vec3(0.f, -100.f, 0.f);
			sphere.Radius = 100.f;
			sphere.MaterialIndex = 1;
			m_Scene.Spheres.push_back(sphere);
		}
	}

	virtual void OnUpdate(float ts) override
	{
		if (m_Camera.OnUpdate(ts))
			m_Renderer.ResetFrameIndex();
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Settings");
		ImGui::Text("Last render time: %.3fms", m_LastRenderTime);
		ImGui::Text("Viewport\n%d , %d", m_ViewportWidth, m_ViewportHeight);

		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);
		if (ImGui::Button("Reset"))
		{
			m_Renderer.ResetFrameIndex();
		}


		ImGui::End();

		ImGui::Begin("Scene");
		for (size_t i = 0; i < m_Scene.Spheres.size(); ++i) 
		{
			ImGui::PushID((int)i);

			ImGui::DragFloat3("Position", glm::value_ptr(m_Scene.Spheres[i].Origin), 0.1f);
			ImGui::DragFloat("Radius", &m_Scene.Spheres[i].Radius, 0.1f, 0.3f);
			ImGui::DragInt("Material Index", (int*)& m_Scene.Spheres[i].MaterialIndex, 0, (int)m_Scene.Materials.size() - 1);
			
			ImGui::Separator();

			ImGui::PopID();
		}
		for (size_t i = 0; i < m_Scene.Materials.size(); ++i)
		{
			ImGui::PushID((int)i);

			ImGui::ColorEdit3("Albedo", glm::value_ptr(m_Scene.Materials[i].Albedo));
			ImGui::DragFloat("Roughness", &m_Scene.Materials[i].Roughness, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Metallic", &m_Scene.Materials[i].Metallic, 0.05f, 0.0f, 1.0f);
			ImGui::PopID();
		}

		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::Begin("Viewport");
		{
			m_ViewportWidth = (u32)ImGui::GetContentRegionAvail().x;
			m_ViewportHeight = (u32)ImGui::GetContentRegionAvail().y;

			std::shared_ptr<Image> image = m_Renderer.GetFinalImage();
			if (image.get())
				ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight() }
			, ImVec2(0, 1), ImVec2(1, 0));

		}
		ImGui::End();
		ImGui::PopStyleVar();

		Render();

	}

	void Render()
	{
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);
		

		m_LastRenderTime = timer.ElapsedMillis();
	}
private:
	u32 m_ViewportWidth = 0, m_ViewportHeight = 0;
	float m_LastRenderTime = 0.f;
	Renderer m_Renderer;
	Camera m_Camera;
	Scene m_Scene;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "RayTracing";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}