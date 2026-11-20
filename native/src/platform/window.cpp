#include "platform/window.h"

#include <glad/gl.h>

#include <stdexcept>
#include <string>

namespace platform {

Window::Window(const char* title, int w, int h) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS))
        throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // required on macOS
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    win_ = SDL_CreateWindow(title, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win_) throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());

    gl_ = SDL_GL_CreateContext(win_);
    if (!gl_) throw std::runtime_error(std::string("SDL_GL_CreateContext: ") + SDL_GetError());
    SDL_GL_MakeCurrent(win_, gl_);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
        throw std::runtime_error("gladLoadGL failed");
    setVsync(true);
}

Window::~Window() {
    if (gl_) SDL_GL_DestroyContext(gl_);
    if (win_) SDL_DestroyWindow(win_);
    SDL_Quit();
}

void Window::setVsync(bool on) { SDL_GL_SetSwapInterval(on ? 1 : 0); }

double Window::refreshRate() const {
    const SDL_DisplayID d = SDL_GetDisplayForWindow(win_);
    const SDL_DisplayMode* m = d ? SDL_GetCurrentDisplayMode(d) : nullptr;
    return m ? static_cast<double>(m->refresh_rate) : 0.0;
}

}  // namespace platform
