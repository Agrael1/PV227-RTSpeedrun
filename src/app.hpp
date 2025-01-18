#pragma once
#include "sdl.hpp"
#include "graphics.hpp"
#include "scene.hpp"

namespace w {
class App
{
public:
    App()
        : window("Window", 800, 600)
        , gfx(window.GetPlatformExtension())
        , swapchain(CreateSwapchain())
        , scene(gfx)
    {
        wis::Result res = wis::success;
        for (auto& cmd_list : cmd_lists) {
            cmd_list = gfx.GetDevice().CreateCommandList(res, wis::QueueType::Graphics);
        }
        aux_cmd_list = gfx.GetDevice().CreateCommandList(res, wis::QueueType::Graphics);

        scene.CreatePipelines(gfx);
        scene.Resize(gfx, 800, 600);
        scene.CreateTLAS(gfx, aux_cmd_list); // reset the command list (local buffers)
        scene.TransitionTextures(gfx, aux_cmd_list);
        aux_cmd_list.Close();

        wis::CommandListView lists[] = { aux_cmd_list };
        gfx.GetMainQueue().ExecuteCommandLists(lists, std::size(lists));
        gfx.WaitForGpu();

        scene.Bind(gfx);
    }

public:
    int Run()
    {
        while (ProcessEvents()) {
            Frame();
        }

        return 0;
    }
    void Frame()
    {
        auto frame_index = swapchain.CurrentFrame();
        auto& cmd_list = cmd_lists[frame_index];
        auto& main_queue = gfx.GetMainQueue();
        cmd_list.Reset();

        scene.Draw(gfx, cmd_list, frame_index);
        scene.CopyToOutput(cmd_list, frame_index, swapchain.GetTexture(frame_index));

        cmd_list.Close();
        wis::CommandListView cmd_list_view{ cmd_list };
        main_queue.ExecuteCommandLists(&cmd_list_view, 1);
        swapchain.Present(main_queue);
        gfx.WaitForGpu();
    }

private:
    uint32_t ProcessEvents();
    void OnKeyPressed(const SDL_Event& event)
    {
        switch (event.key.key) {
        case SDLK_ESCAPE:
            window.PostQuit();
            break;
        }
    }
    void OnMouseMove(const SDL_Event& event)
    {
        if (event.motion.state & SDL_BUTTON_LMASK) {
            scene.RotateCamera(float(event.motion.xrel), float(event.motion.yrel));
        }
    }
    void OnWheel(const SDL_Event& event)
    {
        scene.ZoomCamera(float(event.wheel.y));
    }

private:
    w::Swapchain CreateSwapchain();

private:
    w::Window window;
    w::Graphics gfx;
    w::Swapchain swapchain;
    w::Scene scene;

    wis::CommandList cmd_lists[w::flight_frames];
    wis::CommandList aux_cmd_list; // for transitions and initializations
};
} // namespace w