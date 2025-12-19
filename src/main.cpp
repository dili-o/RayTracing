#include "Log.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
// Vendor
#include <Vendor/stb_image_write.h>
#include <windows.h>

#define CHANNEL_NUM 3

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

  if (!load_scene(HOME_PATH "/Scenes/CubeWorld.json", renderer)) {
		load_default_scene(renderer);
  }

  u8 *pixels =
      new u8[renderer->image_width * renderer->image_height * CHANNEL_NUM];
  std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();
  renderer->render(pixels);
  auto end = std::chrono::high_resolution_clock::now();

  double seconds = std::chrono::duration<double>(end - start).count();
  std::cout << "Total time: " << seconds << " seconds\n";

  std::string image_string = HOME_PATH;
  image_string += "/image_";
  image_string += useCPU ? "cpu" : "gpu";
  image_string += ".png";
  if (!stbi_write_png(image_string.c_str(), renderer->image_width,
                      renderer->image_height, CHANNEL_NUM, pixels,
                      renderer->image_width * CHANNEL_NUM)) {
    HERROR("Failed to write to file: {}", image_string.c_str());
    return 1;
  }

  // TODO: Don't show the image if any errors occurred in the renderer
  if (renderer->show_image)
    ShellExecuteA(nullptr,              // parent window (none)
                  "open",               // operation
                  image_string.c_str(), // file to open
                  nullptr,              // parameters (none)
                  nullptr,              // default directory
                  SW_SHOW               // show the window
    );
}
