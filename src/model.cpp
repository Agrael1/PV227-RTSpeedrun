#include "model.hpp"
#include "model_loader.hpp"
#include "graphics.hpp"

w::Model::Model(w::Graphics& gfx)
{
    using namespace wis;
    wis::Result res = wis::success;
    w::ModelLoader mesh("assets/SnowmanOBJ.obj");
    const wis::ResourceAllocator& alloc = gfx.GetAllocator();
    auto& device = gfx.GetDevice();
    auto& queue = gfx.GetMainQueue();

    auto cmd_list = device.CreateCommandList(res, wis::QueueType::Graphics);
    auto fence = device.CreateFence(res);

    index_buffer = alloc.CreateBuffer(res, mesh.indices.size() * sizeof(uint16_t), wis::BufferUsage::IndexBuffer | wis::BufferUsage::CopyDst | wis::BufferUsage::AccelerationStructureInput);
    vertex_buffer = alloc.CreateBuffer(res, mesh.vertices.size() * sizeof(DirectX::XMFLOAT3), wis::BufferUsage::VertexBuffer | wis::BufferUsage::CopyDst | wis::BufferUsage::AccelerationStructureInput);
    normal_buffer = alloc.CreateBuffer(res, mesh.normals.size() * sizeof(DirectX::XMFLOAT3), wis::BufferUsage::VertexBuffer | wis::BufferUsage::CopyDst);

    if (res.status != wis::success.status) {
        throw std::runtime_error("Failed to create buffers");
    }

    // allocate staging buffer
    uint64_t max_size = mesh.indices.size() * sizeof(uint16_t) + mesh.vertices.size() * sizeof(DirectX::XMFLOAT3) + mesh.normals.size() * sizeof(DirectX::XMFLOAT3);

    wis::Buffer staging = alloc.CreateUploadBuffer(res, max_size);
    if (res.status != wis::success.status) {
        throw std::runtime_error("Failed to create staging buffer");
    }

    // copy data to staging buffer
    uint16_t* indices = staging.Map<uint16_t>();
    std::copy_n(mesh.indices.data(), mesh.indices.size(), indices);

    DirectX::XMFLOAT3* vertices = (DirectX::XMFLOAT3*)(indices + mesh.indices.size());
    // scale vertices
    DirectX::XMVECTOR scale = DirectX::XMVectorSet(0.01f, -0.01f, 0.01f, 1);
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&mesh.vertices[i]);
        v = DirectX::XMVectorMultiply(v, scale);
        DirectX::XMStoreFloat3(&vertices[i], v);
    }

    DirectX::XMFLOAT3* normals = (DirectX::XMFLOAT3*)(vertices + mesh.vertices.size());
    std::copy_n(mesh.normals.data(), mesh.normals.size(), normals);

    staging.Unmap();
    cmd_list.CopyBuffer(staging, index_buffer, { .size_bytes = mesh.indices.size() * sizeof(uint16_t) });
    cmd_list.CopyBuffer(staging, vertex_buffer, { .src_offset = mesh.indices.size() * sizeof(uint16_t), .size_bytes = mesh.vertices.size() * sizeof(DirectX::XMFLOAT3) });
    cmd_list.CopyBuffer(staging, normal_buffer, { .src_offset = mesh.indices.size() * sizeof(uint16_t) + mesh.vertices.size() * sizeof(DirectX::XMFLOAT3), .size_bytes = mesh.normals.size() * sizeof(DirectX::XMFLOAT3) });

    // slap barriers
    // clang-format off
    wis::BufferBarrier2 barriers[]{
        { .barrier = {
                  .sync_before = wis::BarrierSync::Copy,
                  .sync_after = wis::BarrierSync::BuildRTAS,
                  .access_before = wis::ResourceAccess::CopyDest,
                  .access_after = wis::ResourceAccess::Common
          },
          .buffer = index_buffer },
        { .barrier = {
                  .sync_before = wis::BarrierSync::Copy,
                  .sync_after = wis::BarrierSync::BuildRTAS,
                  .access_before = wis::ResourceAccess::CopyDest,
                  .access_after = wis::ResourceAccess::Common
          },
          .buffer = vertex_buffer },
        { .barrier = {
                  .sync_before = wis::BarrierSync::Copy,
                  .sync_after = wis::BarrierSync::BuildRTAS,
                  .access_before = wis::ResourceAccess::CopyDest,
                  .access_after = wis::ResourceAccess::Common
          },
          .buffer = normal_buffer },
    };
    // clang-format on
    cmd_list.BufferBarriers(barriers, std::size(barriers));

    // could have loaded it on separate thread but this is fine for now
    diffuse.Load(gfx, "assets/Snowman_C.png");
    normal.Load(gfx, "assets/Snowman_NM.png");
    specular.Load(gfx, "assets/Snowman_S.png");
    emissive.Load(gfx, "assets/Snowman_Emessive.png");

    diffuse_srv = diffuse.CreateSrv(gfx);
    normal_srv = normal.CreateSrv(gfx);
    specular_srv = specular.CreateSrv(gfx);
    emissive_srv = emissive.CreateSrv(gfx);

    // create blas
    auto& rt = gfx.GetRaytracing();

    AcceleratedGeometryInput blas_input{
        .geometry_type = ASGeometryType::Triangles,
        .flags = ASGeometryFlags::Opaque,
        .vertex_or_aabb_buffer_address = vertex_buffer.GetGPUAddress(),
        .vertex_or_aabb_buffer_stride = sizeof(DirectX::XMFLOAT3),
        .index_buffer_address = index_buffer.GetGPUAddress(),
        .vertex_count = uint32_t(mesh.vertices.size()),
        .triangle_or_aabb_count = uint32_t(mesh.indices.size() / 3),
        .vertex_format = wis::DataFormat::RGB32Float,
        .index_format = wis::IndexType::UInt16,
    };
    auto geometry_desc = wis::CreateGeometryDesc(blas_input);

    wis::BottomLevelASBuildDesc blas_desc{
        .flags = wis::AccelerationStructureFlags::PreferFastTrace,
        .geometry_count = 1,
        .geometry_array = &geometry_desc,
    };
    auto alloc_info = rt.GetBottomLevelASSize(blas_desc);
    auto scratch = alloc.CreateBuffer(res, alloc_info.scratch_size, wis::BufferUsage::StorageBuffer);
    instance_buffer = alloc.CreateBuffer(res, alloc_info.result_size, wis::BufferUsage::AccelerationStructureBuffer);

    blas = rt.CreateAccelerationStructure(res, instance_buffer, 0, alloc_info.result_size, wis::ASLevel::Bottom);

    // build blas
    rt.BuildBottomLevelAS(cmd_list, blas_desc, blas, scratch.GetGPUAddress());

    cmd_list.Close();
    wis::CommandListView lists[] = { cmd_list };
    queue.ExecuteCommandLists(lists, 1);
    res = queue.SignalQueue(fence, 1);
    res = fence.Wait(1);
}

void w::Model::Bind(wis::DescriptorStorage& storage) const
{
    // bind textures to 2nd set
    storage.WriteTexture(2, 0, diffuse_srv);
    storage.WriteTexture(2, 1, normal_srv);
    storage.WriteTexture(2, 2, specular_srv);
    storage.WriteTexture(2, 3, emissive_srv);
}
