#pragma once
#include "model.hpp"
#include "consts.hpp"
#include "camera.hpp"
#include <bitset>

namespace w {

class Scene
{
public:
    Scene(w::Graphics& gfx);
    ~Scene();

public:
    void Resize(w::Graphics& gfx, uint32_t width, uint32_t height);
    void CreatePipelines(w::Graphics& gfx);
    void CreateTLAS(w::Graphics& gfx, wis::CommandList& cmd_list);
    void TransitionTextures(w::Graphics& gfx, wis::CommandList& cmd_list);
    void Bind(w::Graphics& gfx);

    void Draw(w::Graphics& gfx, wis::CommandList& cmd_list, uint32_t frame_index);
    void CopyToOutput(wis::CommandList& cmd_list, uint32_t frame_index, const wis::Texture& out_texture);

private:
    void LoadShaders(w::Graphics& gfx);

private:
    w::Model model; // snowman
    wis::Sampler sampler;

    wis::Texture rt_output[w::flight_frames];
    wis::UnorderedAccessTexture uav_output[w::flight_frames];

    wis::RootSignature rt_root_signature;
    wis::RaytracingPipeline rt_pipeline;
    wis::DescriptorStorage rt_descriptor_storage;

    // shaders
    wis::Shader lib_shader; // for raytracing

    // tlas
    wis::AccelerationStructure tlas{};
    wis::Buffer as_storage;
    wis::Buffer instance_buffer;

    // sbt
    wis::Buffer sbt;
    wis::RaytracingDispatchDesc dispatch_desc{};

    // camera
    w::Camera camera;
    wis::Buffer camera_buffer;
    uint8_t* mapped_cbuffer = nullptr;
    std::bitset<w::flight_frames> camera_dirty;
};
} // namespace w