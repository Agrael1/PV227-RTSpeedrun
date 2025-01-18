#include "graphics.hpp"
#include <wisdom/wisdom_debug.hpp>
#include <iostream>

w::Swapchain::Swapchain(const wis::Device& device, wis::SwapChain xswap, uint32_t width, uint32_t height, wis::DataFormat format, bool stereo)
    : swap(std::move(xswap)), width(width), height(height), stereo(stereo), format(format)
{
    wis::Result result = wis::success;
    fence = device.CreateFence(result, 0);
    textures = swap.GetBufferSpan();

    wis::RenderTargetDesc rt_desc{
        .format = format,
        .layout = stereo ? wis::TextureLayout::Texture2DArray : wis::TextureLayout::Texture2D,
        .layer_count = stereo ? 2u : 1u, // stereo uses multiview extension
    };
    for (size_t i = 0; i < render_targets.size(); i++) {
        render_targets[i] = device.CreateRenderTarget(result, textures[i], rt_desc);
    }
}

bool w::Swapchain::Present(const wis::CommandQueue& main_queue)
{
    auto res = swap.Present();
    if (res.status != wis::Status::Ok) {
        return false;
    }

    CheckResult(main_queue.SignalQueue(fence, fence_value));

    frame_index = swap.GetCurrentIndex() % w::flight_frames;
    CheckResult(fence.Wait(fence_values[frame_index]));

    fence_values[frame_index] = ++fence_value;
    return true;
}

void w::Swapchain::Resize(const wis::Device& device, uint32_t width, uint32_t height)
{
    this->height = height;
    this->width = width;
    wis::Result result = swap.Resize(width, height);
    CheckResult(result);
    textures = swap.GetBufferSpan();

    wis::RenderTargetDesc rt_desc{
        .format = format,
        .layout = stereo ? wis::TextureLayout::Texture2DArray : wis::TextureLayout::Texture2D,
        .layer_count = stereo ? 2u : 1u, // stereo uses multiview extension
    };
    for (size_t i = 0; i < render_targets.size(); i++) {
        render_targets[i] = device.CreateRenderTarget(result, textures[i], rt_desc);
        CheckResult(result);
    }
}

void w::Graphics::DebugCallback(wis::Severity severity, const char* message, void* user_data)
{
    auto stream = reinterpret_cast<std::ostream*>(user_data);
    *stream << message << "\n";
}

wis::Device w::Graphics::InitDevice(wis::FactoryExtension* platform_ext)
{
    std::array<wis::DeviceExtension*, 1> device_exts = { &raytracing };

    wis::Factory factory = DefaultFactory(platform_ext);
    wis::Result result = wis::success;
    for (size_t i = 0;; i++) {
        auto adapter = factory.GetAdapter(result, i);
        if (result.status != wis::Status::Ok) {
            break;
        }

        wis::AdapterDesc desc;
        result = adapter.GetDesc(&desc);
        std::cout << "Adapter: " << desc.description.data() << "\n";

        wis::Device device = wis::CreateDevice(result, std::move(adapter), device_exts.data(), device_exts.size());
        if (result.status == wis::Status::Ok) {
            return device;
        }
    }
    throw Exception("No suitable adapter found");
}

wis::Factory w::Graphics::DefaultFactory(wis::FactoryExtension* platform_ext)
{
    using namespace wis;
    wis::Result res;

#ifndef NDEBUG
    wis::DebugExtension debug_ext; // no need to store it, it will be destroyed when it goes out of scope
#endif // !NDEBUG

    wis::FactoryExtension* xfactory_exts[] = {
#ifndef NDEBUG
        &debug_ext,
#endif // !NDEBUG
        platform_ext
    };
    wis::Factory factory = wis::CreateFactory(res, true, xfactory_exts, std::size(xfactory_exts));
#ifndef NDEBUG
    info = debug_ext.CreateDebugMessenger(res, &DebugCallback, &std::cout);
#endif // !NDEBUG
    return factory;
}

void w::Graphics::InitMainQueue(wis::Device& device)
{
    wis::Result result = wis::success;
    main_queue = device.CreateCommandQueue(result, wis::QueueType::Graphics);
    fence = device.CreateFence(result, 0);
    allocator = device.CreateAllocator(result);
}
