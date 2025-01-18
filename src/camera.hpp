#pragma once
#include <DirectXMath.h>
#include <numbers>
#include <cstring>
#include <algorithm>

namespace w {
struct Camera {
public:
    struct CBuffer {
        DirectX::XMFLOAT4X4A inv_view;
        DirectX::XMFLOAT4X4A inv_projection;
    };


private:
    DirectX::XMFLOAT2A _orientation{ 0.733371496f, -3.88009906f }; // experimental values
    DirectX::XMFLOAT3A _position{ 0, 0, 0 };
    DirectX::XMFLOAT4A _rotation{ 0, 0, 0, 1 };

    mutable DirectX::XMFLOAT4X4A _view;
    mutable DirectX::XMFLOAT4X4A _projection;
    mutable CBuffer _cbuf;

    float _radius = 8;
    bool _dirty_buffer = true; // flag to check if buffer has changed
    bool _dirty_view = true; // flag to check if view has changed
public:
    Camera() noexcept
    {
        Rotate(0, 0);
    }

public:
    bool DirtyBuffer() const noexcept
    {
        return _dirty_buffer;
    }
    void SetClean() noexcept
    {
        _dirty_buffer = false;
    }
    void SetDirty() noexcept
    {
        _dirty_buffer = true;
        _dirty_view = true;
    }

    void PutCBuffer(void* out_buffer)
    {
        RecalculateView(); // maybe recalculates view
        std::memcpy(out_buffer, &_cbuf, sizeof(_cbuf));
    }

    void Rotate(float pitch, float yaw) noexcept
    {
        _orientation.x = WrapAngle(_orientation.y + pitch);
        _orientation.y = std::clamp(WrapAngle(_orientation.x + yaw), 0.995f * -std::numbers::pi_v<float> / 2, 0.995f * std::numbers::pi_v<float> / 2);
        SetOrientation();
    }
    void Zoom(float amount)
    {
        _radius = std::clamp(_radius - amount, 1.0f, 10.0f);
        RecalculatePos();
    }
    void ResetOrientation() noexcept
    {
        _orientation = { 0.733371496f, -3.88009906f };
        SetOrientation();
    }
    void ZeroOrientation() noexcept
    {
        _orientation = { 0, 0 };
        SetOrientation();
    }

public:
    void SetPerspective(float fov, float aspect, float anear, float afar) noexcept
    {
        using namespace DirectX;
        XMMATRIX perspective = XMMatrixPerspectiveFovLH(fov, aspect, anear, afar);
        XMMATRIX inv_perspective = XMMatrixInverse(nullptr, perspective);
        XMStoreFloat4x4A(&_projection, perspective);
        XMStoreFloat4x4A(&_cbuf.inv_projection, inv_perspective);
        _dirty_buffer = true; // no need to recalculate view
    }

private:
    void SetOrientation()
    {
        using namespace DirectX;
        XMVECTOR rotation_quat = DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat2A(&_orientation));
        DirectX::XMStoreFloat4A(&_rotation, rotation_quat);
        RecalculatePos();
    }
    static float WrapAngle(float angle) noexcept
    {
        return fmodf(angle + std::numbers::pi_v<float>, std::numbers::pi_v<float> * 2) - std::numbers::pi_v<float>;
    }
    void RecalculateView()
    {
        using namespace DirectX;
        if (_dirty_view) {
            XMVECTOR look_vector = -XMLoadFloat3A(&_position);
            XMMATRIX view = XMMatrixLookToLH(XMLoadFloat3A(&_position), look_vector, g_XMIdentityR1);
            XMMATRIX inv_view = XMMatrixInverse(nullptr, view);

            XMStoreFloat4x4A(&_view, view);
            XMStoreFloat4x4A(&_cbuf.inv_view, inv_view);
            _dirty_view = false;
        }
    }
    void RecalculatePos() noexcept
    {
        using namespace DirectX;
        XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4A(&_rotation));
        XMVECTOR position = XMVector3Transform(-g_XMIdentityR2 * _radius, rotation);
        XMStoreFloat3A(&_position, position);
        SetDirty();
    }
};
} // namespace w