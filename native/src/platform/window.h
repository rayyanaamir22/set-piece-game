// SDL3 window + OpenGL 4.1 core context.
#pragma once
#include <SDL3/SDL.h>

namespace platform {

class Window {
public:
    Window(const char* title, int w, int h);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    SDL_Window* handle() const { return win_; }
    SDL_GLContext context() const { return gl_; }
    void swap() { SDL_GL_SwapWindow(win_); }
    void setVsync(bool on);
    // Framebuffer size in pixels (differs from window size on HiDPI).
    void drawableSize(int& w, int& h) const { SDL_GetWindowSizeInPixels(win_, &w, &h); }
    // Refresh rate of the display the window is on, 0 if unknown.
    double refreshRate() const;

private:
    SDL_Window* win_ = nullptr;
    SDL_GLContext gl_ = nullptr;
};

}  // namespace platform
