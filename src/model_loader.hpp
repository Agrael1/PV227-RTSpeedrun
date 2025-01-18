#pragma once
#include <filesystem>
#include <vector>
#include <DirectXMath.h>
#include <span>

struct aiScene;
namespace w {
class ModelLoader
{
public:
    ModelLoader(std::filesystem::path p);
    ~ModelLoader();

public:
    std::vector<uint16_t> indices;
    std::span<const DirectX::XMFLOAT3> vertices;
    std::span<const DirectX::XMFLOAT3> normals;
    std::span<const DirectX::XMFLOAT3> texcoords;
    const aiScene* scene;
};
} // namespace w