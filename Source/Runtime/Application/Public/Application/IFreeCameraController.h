#pragma once

#include "Application/ICameraController.h"

struct InputActionContext;

struct FreeCameraControllerDesc
{
    WindowDesc*            pWindow = nullptr;
    vec3                   position = {};
    vec3                   lookAt = {};
    CameraMotionParameters motion = {};
    float                  boostMultiplier = 4.0f;
};

class FORGE_API FreeCameraController
{
public:
    explicit FreeCameraController(const FreeCameraControllerDesc& desc);
    ~FreeCameraController();

    FreeCameraController(const FreeCameraController&) = delete;
    FreeCameraController& operator=(const FreeCameraController&) = delete;

    void update(float deltaTime, uint32_t width, uint32_t height, bool focused);
    mat4 getViewMatrix() const;
    vec3 getPosition() const;

private:
    void        stop();
    static bool onInput(InputActionContext* pInput);

    ICameraController*     pCamera = nullptr;
    CameraMotionParameters motion = {};
    float2                 movement = float2(0.0f);
    float2                 look = float2(0.0f);
    float                  vertical = 0.0f;
    float                  boostMultiplier = 4.0f;
    bool                   captured = false;
    bool                   boost = false;
};
