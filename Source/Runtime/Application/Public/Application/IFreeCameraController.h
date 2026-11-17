#pragma once

#include "Application/ICameraController.h"

struct InputActionContext;

struct FreeCameraControllerDesc
{
    WindowDesc*            pWindow = nullptr;
    Vector3                position = {};
    Vector3                LookAt = {};
    CameraMotionParameters motion = {};
    float                  boostMultiplier = 4.0f;
};

class FORGE_API FreeCameraController
{
public:
    FreeCameraController(const FreeCameraControllerDesc& desc);
    ~FreeCameraController();

    FreeCameraController(const FreeCameraController&) = delete;
    FreeCameraController& operator=(const FreeCameraController&) = delete;

    void    Update(float deltaTime, uint32_t Width, uint32_t Height, bool focused);
    Matrix4 GetViewMatrix() const;
    // Right-handed scene pose, with local -Z pointing forward.
    Matrix4 GetWorldMatrix() const;
    Vector3 GetPosition() const;

private:
    void        Stop();
    static bool OnInput(InputActionContext* pInput);

    ICameraController*     pCamera = nullptr;
    CameraMotionParameters motion = {};
    float2                 movement = float2(0.0f);
    float2                 look = float2(0.0f);
    float                  vertical = 0.0f;
    float                  boostMultiplier = 4.0f;
    bool                   captured = false;
    bool                   boost = false;
};
