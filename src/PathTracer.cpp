#include "PathTracer.hpp"
#include "Core/Event.hpp"
#include "Core/Input.hpp"
#include "Platform/Platform.hpp"

namespace hlx {
bool application_on_event(u16 event_code, void *sender, void *listener,
                          EventContext context) {
  PathTracer *app = (PathTracer *)listener;
  switch (event_code) {
  case SDL_EVENT_QUIT: {
    app->end_application = true;
    return true;
  } break;
  }
  return false;
}

bool application_on_key(u16 event_code, void *sender, void *listener,
                        EventContext context) {
  switch (event_code) {
  case SDL_EVENT_KEY_DOWN: {
    u16 key_code = context.data.u16[0];
    if (key_code == SDL_SCANCODE_ESCAPE) {
      EventContext context{};
      EventSys::fire_event(SDL_EVENT_QUIT, 0, context);
      return true;
    }
  } break;
  }
  return false;
}

void PathTracer::init() {
  PlatformConfiguration config{
      .width = 1280, .height = 720, .name = "Path Tracer"};
  Platform::init(config);
  InputSys::init();
  EventSys::init();
  EventSys::register_event(SDL_EVENT_QUIT, this, application_on_event);
  EventSys::register_event(SDL_EVENT_KEY_DOWN, 0, application_on_key);
  EventSys::register_event(SDL_EVENT_KEY_UP, 0, application_on_key);
  end_application = false;
}

void PathTracer::run() {
  while (!end_application) {
    Platform::handle_os_messages();

    if (!Platform::is_suspended()) {
    }
  }
}

void PathTracer::shutdown() {
  InputSys::shutdown();
  EventSys::unregister_event(SDL_EVENT_QUIT, this, application_on_event);
  EventSys::unregister_event(SDL_EVENT_KEY_DOWN, 0, application_on_key);
  EventSys::unregister_event(SDL_EVENT_KEY_UP, 0, application_on_key);
  EventSys::shutdown();
  Platform::shutdown();
}

} // namespace hlx
