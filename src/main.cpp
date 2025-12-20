#include "Log.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"
// Vendor
#include <Vendor/stb_image_write.h>
#include <windows.h>
#include <filesystem>

#define CHANNEL_NUM 3

int main(int argc, cstring *argv) {
  using namespace hlx;
  Logger logger;
  Renderer *renderer = nullptr;

  i32 arg_idx = 1;
  std::filesystem::path scene_path;
  std::filesystem::path image_path;
  if (argc > 1) {
		// Check if the first argument is the scene
    if (argv[arg_idx][0] != '-') {
			std::filesystem::path input_path = argv[1];
			scene_path = std::filesystem::absolute(input_path);
      ++arg_idx;
    }
    // Check for other arguments
    for (; arg_idx < argc; ++arg_idx) {
      cstring arg = argv[arg_idx];
      if (arg[0] == '-') {
        if (!strcmp(arg + 1, "cpu")) {
          if (renderer) {
            HWARN("Renderer type specified more than once in argument! Only used -cpu or -gpu once");
          } else {
						renderer = new RendererCPU();
          }
        } else if (!strcmp(arg + 1, "gpu")) {
          if (renderer) {
            HWARN("Renderer type specified more than once in argument! Only used -cpu or -gpu once");
          } else {
						renderer = new RendererVk();
          }
        } else if (!strcmp(arg + 1, "o")) {
          ++arg_idx;
          if (arg_idx >= argc) {
            HWARN("No output image file passed!");
          }
          else {
						std::filesystem::path output_path = argv[arg_idx];
						image_path = std::filesystem::absolute(output_path);
          }
        }
      }
    }
  }

  if (!renderer) {
    renderer = new RendererCPU();
  }

  if (!scene_path.empty()) {
		if (!load_scene(scene_path, renderer)) {
			load_default_scene(renderer);
		}
  } else {
		load_default_scene(renderer);
  }

  if (image_path.empty()) {
    image_path = std::string(std::filesystem::current_path().string() + "/image.png");
  }

  u8 *pixels =
      new u8[renderer->image_width * renderer->image_height * CHANNEL_NUM];
  std::chrono::steady_clock::time_point start;
  start = std::chrono::high_resolution_clock::now();
  renderer->render(pixels);
  auto end = std::chrono::high_resolution_clock::now();

  f64 seconds = std::chrono::duration<f64>(end - start).count();
  std::cout << "Total time: " << seconds << " seconds\n";

  if (image_path.extension() != ".png") {
    std::string old_ext = image_path.extension().string();
    HWARN("Image extension type: [{}] not supported.", old_ext.c_str());
    image_path = std::string(std::filesystem::current_path().string() + "/image.png");
  } 

	if (!stbi_write_png(image_path.string().c_str(), renderer->image_width,
											renderer->image_height, CHANNEL_NUM, pixels,
											renderer->image_width * CHANNEL_NUM)) {
		HERROR("Failed to write to file: {}", image_path.string().c_str());
		return 1;
	} 
  HTRACE("Image saved to: {}", image_path.string().c_str());

  // TODO: Don't show the image if any errors occurred in the renderer
  if (renderer->show_image)
    ShellExecuteA(nullptr,                      // parent window (none)
                  "open",                       // operation
                  image_path.string().c_str(),  // file to open
                  nullptr,                      // parameters (none)
                  nullptr,                      // default directory
                  SW_SHOW                       // show the window
    );
}
