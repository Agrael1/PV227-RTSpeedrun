#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "model_loader.hpp"
#include <vector>
#include <string>

w::ModelLoader::ModelLoader(std::filesystem::path p)
{
    auto path_str = p.string();
    Assimp::Importer imp;
    scene = imp.ReadFile(path_str.c_str(),
                         aiProcess_Triangulate |
                                 aiProcess_JoinIdenticalVertices |
                                 aiProcess_ConvertToLeftHanded |
                                 aiProcess_GenNormals |
                                 aiProcess_CalcTangentSpace);

    scene = imp.GetOrphanedScene();
    vertices = { (DirectX::XMFLOAT3*)scene->mMeshes[0]->mVertices, scene->mMeshes[0]->mNumVertices };
    normals = { (DirectX::XMFLOAT3*)scene->mMeshes[0]->mNormals, scene->mMeshes[0]->mNumVertices };
    texcoords = { (DirectX::XMFLOAT3*)scene->mMeshes[0]->mTextureCoords[0], scene->mMeshes[0]->mNumVertices };
    indices.reserve(scene->mMeshes[0]->mNumFaces * 3);
    for (size_t i = 0; i < scene->mMeshes[0]->mNumFaces; ++i) {
        auto& face = scene->mMeshes[0]->mFaces[i];
        for (size_t j = 0; j < face.mNumIndices; ++j) {
            indices.push_back(face.mIndices[j]);
        }
    }
}

w::ModelLoader::~ModelLoader()
{
    if (scene) {
        delete scene;
    }
}
