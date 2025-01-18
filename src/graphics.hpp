#pragma once
#include "consts.hpp"
#include <wisdom/wisdom_raytracing.hpp>

namespace w {
class Swapchain
{
public:
    Swapchain() = default;
    Swapchain(const wis::Device& device, wis::SwapChain xswap, uint32_t width, uint32_t height, wis::DataFormat format = w::swap_format, bool stereo = false);
    ~Swapchain()
    {
        if (swap)
            Throttle();
    }

public:
    void Throttle() noexcept
    {
        CheckResult(fence.Wait(fence_values[frame_index] - 1));
    }
    bool Present(const wis::CommandQueue& main_queue);
    void Resize(const wis::Device& device, uint32_t width, uint32_t height);
    uint32_t CurrentFrame() const
    {
        return swap.GetCurrentIndex();
    }
    const wis::SwapChain& GetSwapChain() const
    {
        return swap;
    }
    uint32_t GetWidth() const
    {
        return width;
    }
    uint32_t GetHeight() const
    {
        return height;
    }

    std::span<const wis::Texture> GetTextures() const
    {
        return textures;
    }
    const wis::Texture& GetTexture(size_t i) const
    {
        return textures[i];
    }
    const wis::RenderTarget& GetRenderTarget(size_t i) const
    {
        return render_targets[i];
    }

private:
    wis::SwapChain swap;
    wis::Fence fence;
    uint64_t fence_value = 1;
    uint64_t frame_index = 0;
    std::array<uint64_t, w::flight_frames> fence_values{ 1, 0 };

    std::span<const wis::Texture> textures;
    std::array<wis::RenderTarget, w::swap_frames> render_targets;

    wis::DataFormat format = w::swap_format;
    uint32_t width = 0;
    uint32_t height = 0;
    bool stereo = false;
};

class Graphics
{
    static void DebugCallback(wis::Severity severity, const char* message, void* user_data);

public:
    Graphics(wis::FactoryExtension* platform_ext)
        : device(InitDevice(platform_ext))
    {
        InitMainQueue(device);
    }

public:
    void WaitForGpu()
    {
        CheckResult(main_queue.SignalQueue(fence, fence_value));
        CheckResult(fence.Wait(fence_value));
        fence_value++;
    }

public:
    const wis::Device& GetDevice() const
    {
        return device;
    }
    const wis::CommandQueue& GetMainQueue() const
    {
        return main_queue;
    }
    const wis::ResourceAllocator& GetAllocator() const
    {
        return allocator;
    }
    wis::Raytracing& GetRaytracing()
    {
        return raytracing;
    }

private:
    wis::Device InitDevice(wis::FactoryExtension* platform_ext);
    wis::Factory DefaultFactory(wis::FactoryExtension* platform_ext);
    void InitMainQueue(wis::Device& device);

public:
    wis::Raytracing raytracing;
#ifndef NDEBUG
    wis::DebugMessenger info;
#endif

    wis::Device device;
    wis::CommandQueue main_queue;

    wis::ResourceAllocator allocator;

    wis::Fence fence; // for wait for gpu
    uint64_t fence_value = 1;
};
} // namespace w