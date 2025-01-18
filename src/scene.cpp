#include "scene.hpp"
#include "graphics.hpp"
#include <fstream>

std::string LoadShader(std::filesystem::path p)
{
    if constexpr (wis::shader_intermediate == wis::ShaderIntermediate::DXIL) {
        p += u".cso";
    } else {
        p += u".spv";
    }

    if (!std::filesystem::exists(p)) {
        throw w::Exception(wis::format("Shader file not found: {}", p.string()));
    }

    std::ifstream t{ p, std::ios::binary };
    t.seekg(0, std::ios::end);
    size_t size = t.tellg();
    std::string ret;
    ret.resize(size);
    t.seekg(0);
    t.read(ret.data(), size);
    return ret;
}

w::Scene::Scene(w::Graphics& gfx)
    : model(gfx)
{
    // create camera buffer
    wis::Result result = wis::success;
    auto& alloc = gfx.GetAllocator();

    // 2 * 4x4 matrices for view and projection
    constexpr uint32_t buffer_size = wis::detail::aligned_size(sizeof(w::Camera::CBuffer), 256ull) * w::flight_frames;
    camera_buffer = alloc.CreateBuffer(result, buffer_size, wis::BufferUsage::ConstantBuffer, wis::MemoryType::Upload, wis::MemoryFlags::Mapped);
    mapped_cbuffer = camera_buffer.Map<uint8_t>();
}
w::Scene::~Scene()
{
    camera_buffer.Unmap();
}

void w::Scene::Resize(w::Graphics& gfx, uint32_t width, uint32_t height)
{
    using namespace wis; // for flag operators
    wis::Result result = wis::success;
    auto& device = gfx.GetDevice();
    auto& alloc = gfx.GetAllocator();

    // Create UAV texture
    wis::TextureDesc desc{
        .format = w::swap_format,
        .size = { width, height, 1 },
        .usage = wis::TextureUsage::CopySrc | wis::TextureUsage::UnorderedAccess,
    };
    rt_output[0] = alloc.CreateTexture(result, desc);
    rt_output[1] = alloc.CreateTexture(result, desc);

    // Create UAV output
    wis::UnorderedAccessDesc uav_desc{
        .format = w::swap_format,
        .view_type = wis::TextureViewType::Texture2D,
        .subresource_range = { 0, 1, 0, 1 },
    };
    uav_output[0] = device.CreateUnorderedAccessTexture(result, rt_output[0], uav_desc);
    uav_output[1] = device.CreateUnorderedAccessTexture(result, rt_output[1], uav_desc);

    // Write to descriptor storage
    rt_descriptor_storage.WriteRWTexture(0, 0, uav_output[0]);
    rt_descriptor_storage.WriteRWTexture(0, 1, uav_output[1]);

    // update dispatch desc
    dispatch_desc.width = width;
    dispatch_desc.height = height;
    dispatch_desc.depth = 1;

    camera.SetPerspective(60.0f, float(width) / float(height), 0.1f, 100.0f);
}

void w::Scene::CreatePipelines(w::Graphics& gfx)
{
    wis::Result result = wis::success;
    auto& rt = gfx.GetRaytracing();
    auto& device = gfx.GetDevice();

    // Create Sampler
    wis::SamplerDesc sample_desc{
        .min_filter = wis::Filter::Linear,
        .mag_filter = wis::Filter::Linear,
        .mip_filter = wis::Filter::Linear,
        .anisotropic = false,
        .max_anisotropy = 16,
        .address_u = wis::AddressMode::Repeat,
        .address_v = wis::AddressMode::Repeat,
        .address_w = wis::AddressMode::Repeat,
        .min_lod = 0,
        .max_lod = 1.0f,
        .mip_lod_bias = 0.0f,
        .comparison_op = wis::Compare::None,
    };
    sampler = device.CreateSampler(result, sample_desc);

    wis::DescriptorBindingDesc bindings[] = {
        { .binding_type = wis::DescriptorType::RWTexture, .binding_space = 1, .binding_count = w::flight_frames }, // output textures
        { .binding_type = wis::DescriptorType::AccelerationStructure, .binding_space = 2, .binding_count = 1 }, // TLAS
        { .binding_type = wis::DescriptorType::Texture, .binding_space = 3, .binding_count = 4 }, // textures for model
        { .binding_type = wis::DescriptorType::Sampler, .binding_space = 4, .binding_count = 1 }, // sampler for textures
    };
    wis::PushDescriptor push_descriptors[] = {
        { .stage = wis::ShaderStages::All, .type = wis::DescriptorType::ConstantBuffer }
    };
    wis::PushConstant push_constants[] = {
        { .stage = wis::ShaderStages::All, .size_bytes = sizeof(uint32_t), .bind_register = 1u }
    };
    rt_descriptor_storage = device.CreateDescriptorStorage(result, bindings, std::size(bindings));
    rt_root_signature = device.CreateRootSignature(result, push_constants, std::size(push_constants), push_descriptors, std::size(push_descriptors), bindings, std::size(bindings));

    LoadShaders(gfx);
    wis::ShaderView shaders = lib_shader;
    wis::ShaderExport exports[]{
        { .entry_point = "RayGeneration", .shader_type = wis::RaytracingShaderType::Raygen, .shader_array_index = 0 },
        { .entry_point = "Miss", .shader_type = wis::RaytracingShaderType::Miss, .shader_array_index = 0 },
        { .entry_point = "ClosestHit", .shader_type = wis::RaytracingShaderType::ClosestHit, .shader_array_index = 0 },
    };
    wis::HitGroupDesc hit_groups[]{
        { .type = wis::HitGroupType::Triangles, .closest_hit_export_index = 2 },
    };

    // create raytracing pipeline
    wis::RaytracingPipelineDesc rt_desc{
        .root_signature = rt_root_signature,
        .shaders = &shaders,
        .shader_count = 1,
        .exports = exports,
        .export_count = std::size(exports),
        .max_recursion_depth = 1,
        .max_payload_size = 24,
        .max_attribute_size = 8,
    };
    rt_pipeline = rt.CreateRaytracingPipeline(result, rt_desc);

    // dump SBT into a single array and regret it later
    wis::ShaderBindingTableInfo sbt_info = rt.GetShaderBindingTableInfo();
    const uint8_t* shader_ident = rt_pipeline.GetShaderIdentifiers();

    uint32_t raygen_size = wis::detail::aligned_size(sbt_info.entry_size, sbt_info.table_start_alignment);
    uint32_t miss_size = wis::detail::aligned_size(sbt_info.entry_size, sbt_info.table_start_alignment);
    uint32_t hit_group_size = wis::detail::aligned_size(sbt_info.entry_size, sbt_info.table_start_alignment);

    sbt = gfx.GetAllocator().CreateBuffer(result, raygen_size + miss_size + hit_group_size, wis::BufferUsage::ShaderBindingTable, wis::MemoryType::Upload, wis::MemoryFlags::Mapped);

    // write SBT
    uint8_t* mapped = sbt.Map<uint8_t>();
    auto addr = sbt.GetGPUAddress();

    // raygen
    dispatch_desc.ray_gen_shader_table_address = addr;
    dispatch_desc.ray_gen_shader_table_size = sbt_info.entry_size;
    std::memcpy(mapped, shader_ident, sbt_info.entry_size);
    mapped += raygen_size;

    // miss
    dispatch_desc.miss_shader_table_address = addr + raygen_size;
    dispatch_desc.miss_shader_table_size = sbt_info.entry_size;
    dispatch_desc.miss_shader_table_stride = sbt_info.entry_size;
    std::memcpy(mapped, shader_ident + sbt_info.entry_size, sbt_info.entry_size);
    mapped += miss_size;

    // hit group
    dispatch_desc.hit_group_table_address = addr + raygen_size + miss_size;
    dispatch_desc.hit_group_table_size = sbt_info.entry_size;
    dispatch_desc.hit_group_table_stride = sbt_info.entry_size;
    std::memcpy(mapped, shader_ident + sbt_info.entry_size * 2, sbt_info.entry_size);
}

void w::Scene::TransitionTextures(w::Graphics& gfx, wis::CommandList& cmd)
{
    std::ignore = cmd.Reset();
    // Transition UAV texture to UAV state
    cmd.TextureBarrier({ .sync_before = wis::BarrierSync::None,
                         .sync_after = wis::BarrierSync::None,
                         .access_before = wis::ResourceAccess::NoAccess,
                         .access_after = wis::ResourceAccess::NoAccess,
                         .state_before = wis::TextureState::Undefined,
                         .state_after = wis::TextureState::UnorderedAccess },
                       rt_output[0]);
    cmd.TextureBarrier({ .sync_before = wis::BarrierSync::None,
                         .sync_after = wis::BarrierSync::None,
                         .access_before = wis::ResourceAccess::NoAccess,
                         .access_after = wis::ResourceAccess::NoAccess,
                         .state_before = wis::TextureState::Undefined,
                         .state_after = wis::TextureState::UnorderedAccess },
                       rt_output[1]);
}

void w::Scene::Bind(w::Graphics& gfx)
{
    auto& rt = gfx.GetRaytracing();
    // Bind TLAS
    rt.WriteAccelerationStructure(rt_descriptor_storage, 1, 0, tlas);
    // Bind model textures
    model.Bind(rt_descriptor_storage);
    // Bind sampler
    rt_descriptor_storage.WriteSampler(3, 0, sampler);
}

void w::Scene::Draw(w::Graphics& gfx, wis::CommandList& cmd_list, uint32_t frame_index)
{
    // Update camera buffer
    uint32_t offset = frame_index * wis::detail::aligned_size(sizeof(w::Camera::CBuffer), 256ull);
    camera.PutCBuffer(mapped_cbuffer + offset);

    // Dispatch rays
    auto& rt = gfx.GetRaytracing();

    rt.SetPipelineState(cmd_list, rt_pipeline);
    cmd_list.SetComputeRootSignature(rt_root_signature);
    cmd_list.SetComputePushConstants(&frame_index, 1, 0);

    // push camera data
    rt.PushDescriptor(cmd_list, wis::DescriptorType::ConstantBuffer, 0, camera_buffer, offset);
    rt.SetDescriptorStorage(cmd_list, rt_descriptor_storage);
    rt.DispatchRays(cmd_list, dispatch_desc);
}

void w::Scene::CopyToOutput(wis::CommandList& cmd_list, uint32_t frame_index, const wis::Texture& out_texture)
{
    // clang-format off
    wis::TextureBarrier2 before[]{
        { .barrier = {
                  .sync_before = wis::BarrierSync::Compute,
                  .sync_after = wis::BarrierSync::Copy,
                  .access_before = wis::ResourceAccess::UnorderedAccess,
                  .access_after = wis::ResourceAccess::CopySource,
                  .state_before = wis::TextureState::UnorderedAccess,
                  .state_after = wis::TextureState::CopySource },
          .texture =  rt_output[frame_index]},
        { .barrier = {
                  .sync_before = wis::BarrierSync::None,
                  .sync_after = wis::BarrierSync::Copy,
                  .access_before = wis::ResourceAccess::NoAccess,
                  .access_after = wis::ResourceAccess::CopyDest,
                  .state_before = wis::TextureState::Present,
                  .state_after = wis::TextureState::CopyDest }, 
          .texture = out_texture }
    };
    wis::TextureBarrier2 after[]{
        { .barrier = {
                    .sync_before = wis::BarrierSync::Copy,
                    .sync_after = wis::BarrierSync::None,
                    .access_before = wis::ResourceAccess::CopySource,
                    .access_after = wis::ResourceAccess::NoAccess,
                    .state_before = wis::TextureState::CopySource,
                    .state_after = wis::TextureState::UnorderedAccess },
          .texture = rt_output[frame_index] },
        { .barrier = {
                .sync_before = wis::BarrierSync::Copy,
                .sync_after = wis::BarrierSync::None,
                .access_before = wis::ResourceAccess::CopyDest,
                .access_after = wis::ResourceAccess::NoAccess,
                .state_before = wis::TextureState::CopyDest,
                .state_after = wis::TextureState::Present },
          .texture = out_texture }
    };

    // clang-format on
    cmd_list.TextureBarriers(before, std::size(before));
    wis::TextureCopyRegion region{
        .src = {
                .size = { dispatch_desc.width, dispatch_desc.height, 1 },
                .format = w::swap_format,
        },
        .dst = {
                .size = { dispatch_desc.width, dispatch_desc.height, 1 },
                .format = w::swap_format,
        },
    };
    cmd_list.CopyTexture(rt_output[frame_index], out_texture, &region, 1);
    cmd_list.TextureBarriers(after, std::size(after));
}

void w::Scene::LoadShaders(w::Graphics& gfx)
{
    wis::Result result = wis::success;
    auto& device = gfx.GetDevice();

    auto buf = LoadShader("shaders/raytracing.lib");
    lib_shader = device.CreateShader(result, buf.data(), uint32_t(buf.size()));
}

void w::Scene::CreateTLAS(w::Graphics& gfx, wis::CommandList& cmd_list)
{
    wis::Result result = wis::success;
    auto& rt = gfx.GetRaytracing();
    auto& device = gfx.GetDevice();
    auto& alloc = gfx.GetAllocator();

    // initialize acceleration structure instance
    instance_buffer = alloc.CreateBuffer(result, sizeof(wis::AccelerationInstance), wis::BufferUsage::AccelerationStructureInput, wis::MemoryType::Upload, wis::MemoryFlags::Mapped);
    instance_buffer.Map<wis::AccelerationInstance>()[0] = {
        .transform = {
                { 1.0f, 0.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f, 0.0f },
        },
        .instance_id = 0,
        .mask = 0xFF,
        .instance_offset = 0,
        .flags = uint32_t(wis::ASInstanceFlags::TriangleCullDisable),
        .acceleration_structure_handle = rt.GetAccelerationStructureDeviceAddress(model.GetBLAS()),
    };
    instance_buffer.Unmap();

    // Create acceleration structure
    wis::TopLevelASBuildDesc build_desc{
        .flags = wis::AccelerationStructureFlags::PreferFastTrace,
        .instance_count = 1,
        .gpu_address = instance_buffer.GetGPUAddress(),
        .indirect = false,
        .update = false, // will be changed later
    };

    auto as_size = rt.GetTopLevelASSize(build_desc);

    as_storage = alloc.CreateBuffer(result, as_size.result_size, wis::BufferUsage::AccelerationStructureBuffer);
    tlas = rt.CreateAccelerationStructure(result, as_storage, 0, as_size.result_size, wis::ASLevel::Top);
    wis::Buffer scratch = alloc.CreateBuffer(result, as_size.scratch_size, wis::BufferUsage::StorageBuffer);

    // Build TLAS
    rt.BuildTopLevelAS(cmd_list, build_desc, tlas, scratch.GetGPUAddress());
    cmd_list.Close();
    wis::CommandListView lists[] = { cmd_list };
    gfx.GetMainQueue().ExecuteCommandLists(lists, 1);
    gfx.WaitForGpu();

    std::ignore = cmd_list.Reset();
}
