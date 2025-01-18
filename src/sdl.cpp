#include "sdl.hpp"
#include "consts.hpp"
#include <wisdom/wisdom_platform.hpp>


wis::SwapChain w::Window::CreateSwapchain(wis::Result& result, const wis::Device& device, const wis::CommandQueue& main_queue)
{
    using enum PlatformExtension::Selector;
    if (_platform.current == None) {
        throw w::Exception("Platform is not selected");
    }

    auto [width, height] = PixelSize();
    wis::SwapchainDesc desc{
        .size = { uint32_t(width), uint32_t(height) },
        .format = w::swap_format,
        .buffer_count = w::swap_frames,
        .stereo = false,
        .vsync = true,
        .tearing = false,
    };

    switch (_platform.current) {
#if defined(SDL_PLATFORM_WIN32)
    case Windows: {
        HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        if (hwnd) {
            return static_cast<wis::platform::WindowsExtension*>(_platform.get())
                    ->CreateSwapchain(result, device, main_queue, &desc, hwnd);
        }
    } break;
#elif defined(SDL_PLATFORM_LINUX)
    case X11: {
        Display* xdisplay = (Display*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        ::Window xwindow = (::Window)SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (xdisplay && xwindow) {
            return static_cast<wis::platform::X11Extension*>(_platform.get())
                    ->CreateSwapchain(result, device, main_queue, &desc, xdisplay, xwindow);
        }
    } break;
    case Wayland: {
        struct wl_display* display = (struct wl_display*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, NULL);
        struct wl_surface* surface = (struct wl_surface*)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, NULL);
        if (display && surface) {
            return static_cast<wis::platform::WaylandExtension*>(_platform.get())
                    ->CreateSwapchain(result, device, main_queue, &desc, display, surface);
        }
    } break;
#endif
    }
    throw w::Exception("Failed to create swapchain");
}

void w::Window::PostQuit()
{
    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
}

w::PlatformExtension::PlatformExtension()
{
    current = Selector::None;
    platform = {};
    const char* platform_name = SDL_GetCurrentVideoDriver();
#if defined(SDL_PLATFORM_WIN32)
    platform = std::make_unique<wis::platform::WindowsExtension>();
    current = Selector::Windows;
#elif defined(SDL_PLATFORM_LINUX)
    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0) {
        platform = std::make_unique<wis::platform::X11Extension>();
        current = Selector::X11;
    } else if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "wayland") == 0) {
        platform = std::make_unique<wis::platform::WaylandExtension>();
        current = Selector::Wayland;
    }
#endif
}
