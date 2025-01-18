#pragma once
#include <wisdom/wisdom.hpp>
#include <filesystem>

namespace w {
class Graphics;
class Texture
{
public:
    void Load(w::Graphics& gfx, std::filesystem::path p); // uses staging buffer
public:
    wis::ShaderResource CreateSrv(w::Graphics& gfx);

private:
    wis::Texture texture;
};
} // namespace w