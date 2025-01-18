#pragma once
#include "texture.hpp"
#include <wisdom/wisdom_raytracing.hpp>


namespace w {
class Graphics;
class Model
{
public:
    Model(w::Graphics& gfx);

public:
    void Bind(wis::DescriptorStorage& storage) const;
    wis::AccelerationStructure& GetBLAS()
    {
        return blas;
    }

private:
    w::Texture diffuse;
    w::Texture normal;
    w::Texture specular;
    w::Texture emissive;

    wis::ShaderResource diffuse_srv;
    wis::ShaderResource normal_srv;
    wis::ShaderResource specular_srv;
    wis::ShaderResource emissive_srv;

    wis::AccelerationStructure blas; // shall never be updated
    wis::Buffer instance_buffer;

    wis::Buffer vertex_buffer;
    wis::Buffer normal_buffer;
    wis::Buffer index_buffer;
};
} // namespace w