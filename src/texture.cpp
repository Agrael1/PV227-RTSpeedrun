#include "texture.hpp"
#include "graphics.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb.h"

void w::Texture::Load(w::Graphics& gfx, std::filesystem::path p)
{
    const wis::ResourceAllocator& alloc = gfx.GetAllocator();
    const wis::Device& device = gfx.GetDevice();
    const wis::CommandQueue& queue = gfx.GetMainQueue();

    int width, height, channels;
    auto* idata = stbi_load(p.string().c_str(), &width, &height, &channels, 4);

    if (!idata) {
        return;
    }

    wis::Size2D size{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

    using namespace wis;
    wis::TextureDesc tdesc{
        .format = wis::DataFormat::RGBA8Unorm,
        .size = { size.width, size.height, 1 },
        .usage = wis::TextureUsage::ShaderResource | wis::TextureUsage::CopyDst,
    };
    auto [res, tex] = alloc.CreateTexture(tdesc);
    texture = std::move(tex);

    auto [res4, buf] = alloc.CreateUploadBuffer(size.width * size.height * 4);
    std::copy(idata, idata + size.width * size.height * 4, buf.Map<uint8_t>());
    buf.Unmap();
    stbi_image_free(idata);

    auto [res2, cl] = device.CreateCommandList(wis::QueueType::Graphics);
    std::ignore = cl.Reset();
    cl.TextureBarrier(
            {
                    .sync_before = wis::BarrierSync::None,
                    .sync_after = wis::BarrierSync::Copy,
                    .access_before = wis::ResourceAccess::NoAccess,
                    .access_after = wis::ResourceAccess::CopyDest,
                    .state_before = wis::TextureState::Undefined,
                    .state_after = wis::TextureState::CopyDest,
            },
            texture);

    wis::BufferTextureCopyRegion region{
        .texture{
                .size = { size.width, size.height, 1 },
                .format = wis::DataFormat::RGBA8Unorm,
        }
    };

    cl.CopyBufferToTexture(buf, texture, &region, 1);
    cl.TextureBarrier(
            {
                    .sync_before = wis::BarrierSync::Copy,
                    .sync_after = wis::BarrierSync::None,
                    .access_before = wis::ResourceAccess::CopyDest,
                    .access_after = wis::ResourceAccess::NoAccess,
                    .state_before = wis::TextureState::CopyDest,
                    .state_after = wis::TextureState::ShaderResource,
            },
            texture);

    cl.Close();
    wis::CommandListView lists[] = { cl };
    auto [res3, f] = device.CreateFence();
    queue.ExecuteCommandLists(lists, 1);
    std::ignore = queue.SignalQueue(f, 1);
    std::ignore = f.Wait(1);
}

wis::ShaderResource w::Texture::CreateSrv(w::Graphics& gfx)
{
    wis::Result res = wis::success;
    wis::ShaderResourceDesc srv_desc{
        .format = wis::DataFormat::RGBA8Unorm,
        .view_type = wis::TextureViewType::Texture2D,
        .subresource_range = { 0, 1, 0, 1 },
    };
    return gfx.GetDevice().CreateShaderResource(res, texture, srv_desc);
}
