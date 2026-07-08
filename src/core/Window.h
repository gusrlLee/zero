#ifndef __ZERO_WINDOW_HEADER__
#define __ZERO_WINDOW_HEADER__

#include <SDL3/SDL.h>
#include <iostream>

class Window {
public:
    Window(int width, int height, const std::string &title);
    ~Window();

    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;

    bool shouldClose() const { return m_shouldClose; }
    void close() { m_shouldClose = true; }

    void pollEvents();
    SDL_Window* getNativeHandle() const { return m_window; }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    SDL_Window *m_window;
    int m_width;
    int m_height;
    bool m_shouldClose;
};

#endif