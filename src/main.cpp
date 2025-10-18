#include "Log.hpp"
#include "Renderer.hpp"
// Vendor
#include <Vendor/stb_image_write.h>
#include <windows.h>

#define CHANNEL_NUM 3

static cstring image_cpu = "image_cpu.png";
static cstring image_gpu = "image_gpu.png";

int main(int argc, cstring *argv) {
  using namespace hlx;
  Logger logger;
  Renderer *renderer;

  bool useCPU = false;
  bool useGPU = false;
  for (i32 i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "-cpu")) {
      useCPU = true;
    } else if (!strcmp(argv[i], "-gpu")) {
      useGPU = true;
    }
  }

  if (useCPU && useGPU) {
    HERROR("Cannot enable both -cpu and -gpu, select only one!");
    return 1;
  } else if (!useCPU && !useGPU) {
    HERROR("Must enable either -cpu or -gpu!");
    return 1;
  }

  if (useCPU) {
    renderer = new RendererCPU();
  } else {
    renderer = new RendererVk();
  }

  // Create world
  MaterialHandle mat_ground =
      renderer->add_lambert_material(Color(0.5f, 0.5f, 0.5f));
  renderer->add_sphere(Point3(0.f, -1000.f, 0.f), 1000.f, mat_ground);

  i32 size = 11;
  for (i32 a = -size; a < size; ++a) {
    for (i32 b = -size; b < size; ++b) {
      real choose_mat = random_real();
      Point3 center(a + 0.9f * random_real(), 0.2f, b + 0.9f * random_real());

      if ((center - Point3(4.f, 0.2f, 0.f)).length() > 0.9f) {
        MaterialHandle sphere_material;

        if (choose_mat < 0.8f) {
          // diffuse
          Color albedo = Color::random() * Color::random();
          sphere_material = renderer->add_lambert_material(albedo);
          renderer->add_sphere(center, 0.2f, sphere_material);
        } else if (choose_mat < 0.95f) {
          // metal
          Color albedo = Color::random(0.5f, 1.f);
          real fuzz = random_real(0.f, 0.5f);
          sphere_material = renderer->add_metal_material(albedo, fuzz);
          renderer->add_sphere(center, 0.2f, sphere_material);
        } else {
          // glass
          sphere_material = renderer->add_dielectric_material(1.5f);
          renderer->add_sphere(center, 0.2f, sphere_material);
        }
      }
    }
  }

  MaterialHandle mat1 = renderer->add_dielectric_material(1.5f);
  renderer->add_sphere(Point3(0.f, 1.f, 0.f), 1.f, mat1);
  MaterialHandle mat2 = renderer->add_lambert_material(Color(0.4f, 0.2f, 0.1f));
  renderer->add_sphere(Point3(-4.f, 1.f, 0.f), 1.f, mat2);
  MaterialHandle mat3 =
      renderer->add_metal_material(Color(0.7f, 0.6f, 0.5f), 0.f);
  renderer->add_sphere(Point3(4.f, 1.f, 0.f), 1.f, mat3);

  // Initialize
  renderer->center = Point3(13.f, 2.f, 3.f);
  renderer->lookat = Point3(0.f, 0.f, 0.f);
  renderer->vup = Vec3(0.f, 1.f, 0.f);
  renderer->defocus_angle = 0.6f;
  renderer->focus_dist = 10.f;
  renderer->init(1280, 16.f / 9.f, 100, 10, 20);

  u8 *pixels =
      new u8[renderer->image_width * renderer->image_height * CHANNEL_NUM];
  std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();
  renderer->render(pixels);
  auto end = std::chrono::high_resolution_clock::now();

  double seconds = std::chrono::duration<double>(end - start).count();
  std::cout << "Total time: " << seconds << " seconds\n";

  if (!stbi_write_png(useCPU ? image_cpu : image_gpu, renderer->image_width,
                      renderer->image_height, CHANNEL_NUM, pixels,
                      renderer->image_width * CHANNEL_NUM)) {
    HERROR("Failed to write to file: {}", "image.png");
    return 1;
  }

  if (renderer->show_image)
    ShellExecuteA(nullptr,                        // parent window (none)
                  "open",                         // operation
                  useCPU ? image_cpu : image_gpu, // file to open
                  nullptr,                        // parameters (none)
                  nullptr,                        // default directory
                  SW_SHOW                         // show the window
    );
}
