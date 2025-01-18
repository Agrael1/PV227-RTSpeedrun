#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <wisdom/wisdom.hpp>

namespace w {
class Instance
{
public:
    Instance()
    {
        SDL_Init(SDL_INIT_VIDEO);
    }
    ~Instance()
    {
        SDL_Quit();
    }
};

class PlatformExtension
{
public:
    enum class Selector {
        None,
        Windows,
        X11,
        Wayland
    };

public:
    PlatformExtension();

public:
    wis::FactoryExtension* get() noexcept
    {
        return platform.get();
    }

public:
    Selector current = Selector::None;
    std::unique_ptr<wis::FactoryExtension> platform;
};

class Window
{
public:
    Window(const char* title, int width, int height)
    {
        window = SDL_CreateWindow(title, width, height, SDL_WINDOW_RESIZABLE);
    }
    ~Window()
    {
        SDL_DestroyWindow(window);
    }
    SDL_Window* GetWindow() const
    {
        return window;
    }

public:
    wis::FactoryExtension* GetPlatformExtension()
    {
        return _platform.get();
    }
    wis::SwapChain CreateSwapchain(wis::Result& result, const wis::Device& device, const wis::CommandQueue& main_queue);

    void PostQuit();
    std::pair<int, int> PixelSize() const noexcept
    {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        return { w, h };
    }

private:
    SDL_Window* window;
    PlatformExtension _platform;
};

} // namespace w