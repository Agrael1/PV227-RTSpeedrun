#include "app.hpp"

uint32_t w::App::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            swapchain.Throttle();
            return 0;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
            swapchain.Resize(gfx.GetDevice(), event.window.data1, event.window.data2);
            scene.Resize(gfx, event.window.data1, event.window.data2);

            aux_cmd_list.Reset();
            scene.TransitionTextures(gfx, aux_cmd_list);
            aux_cmd_list.Close();

            wis::CommandListView lists[] = { aux_cmd_list };
            gfx.GetMainQueue().ExecuteCommandLists(lists, std::size(lists));
            gfx.WaitForGpu();
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            OnKeyPressed(event);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            OnMouseMove(event);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            OnWheel(event);
            break;
        }
    }
    return 1;
}

w::Swapchain w::App::CreateSwapchain()
{
    wis::Result result = wis::success;
    auto [w, h] = window.PixelSize();

    return w::Swapchain{ gfx.GetDevice(),
                         window.CreateSwapchain(result, gfx.GetDevice(), gfx.GetMainQueue()),
                         uint32_t(w), uint32_t(h) };
}
