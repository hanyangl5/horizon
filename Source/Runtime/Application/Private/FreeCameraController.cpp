#include "Application/IFreeCameraController.h"

#include "Platform/IInput.h"

#include "Core/ILog.h"

namespace
{
enum CameraAction : uint32_t
{
    Move,
    MoveVertical,
    Look,
    Capture,
    Boost,
    Reset,
    Release,
};
}

void FreeCameraController::stop()
{
    movement = float2(0.0f);
    look = float2(0.0f);
    vertical = 0.0f;
    pCamera->moveTo(pCamera->getViewPosition());
}

bool FreeCameraController::onInput(InputActionContext* pInput)
{
    FreeCameraController* pController = (FreeCameraController*)pInput->pUserData;
    const bool            ended = pInput->phase == INPUT_ACTION_PHASE_CANCELED || pInput->phase == INPUT_ACTION_PHASE_ENDED;
    switch (pInput->actionId)
    {
    case Move:
        pController->movement = ended ? float2(0.0f) : pInput->float2Value;
        break;
    case MoveVertical:
        pController->vertical = ended ? 0.0f : pInput->floatValue;
        break;
    case Look:
        if (pController->captured && !ended)
            pController->look = pInput->float2Value;
        break;
    case Capture:
        pController->captured = !ended && pInput->boolValue;
        setEnableCaptureInput(pController->captured);
        break;
    case Boost:
        pController->boost = !ended && pInput->boolValue;
        break;
    case Reset:
        if (pInput->phase == INPUT_ACTION_PHASE_STARTED)
        {
            pController->pCamera->resetView();
            pController->stop();
        }
        break;
    case Release:
        if (pInput->boolValue)
        {
            pController->captured = false;
            setEnableCaptureInput(false);
        }
        break;
    }
    return true;
}

FreeCameraController::FreeCameraController(const FreeCameraControllerDesc& desc)
{
    ASSERT(desc.pWindow);
    pCamera = initFpsCameraController(desc.position, desc.lookAt);
    motion = desc.motion;
    boostMultiplier = desc.boostMultiplier;
    ASSERT(pCamera);

    InputSystemDesc inputDesc = { .pWindow = desc.pWindow };
    const bool      inputInitialized = initInputSystem(&inputDesc);
    ASSERT(inputInitialized);

    ActionMappingDesc mappings[] = {
        { .actionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
          .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .actionId = Move,
          .deviceButtons = { KEYBOARD_BUTTON_D, KEYBOARD_BUTTON_A, KEYBOARD_BUTTON_W, KEYBOARD_BUTTON_S } },
        { .actionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
          .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .actionId = MoveVertical,
          .deviceButtons = { KEYBOARD_BUTTON_E, KEYBOARD_BUTTON_Q },
          .compositeUseSingleAxis = true },
        { .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE,
          .actionId = Look,
          .deviceButtons = { MOUSE_BUTTON_AXIS_X },
          .numAxis = 2,
          .scale = 0.002f,
          .scaleByDT = true },
        { .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE, .actionId = Capture, .deviceButtons = { MOUSE_BUTTON_RIGHT } },
        { .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .actionId = Boost,
          .deviceButtons = { KEYBOARD_BUTTON_SHIFT_L } },
        { .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD, .actionId = Reset, .deviceButtons = { KEYBOARD_BUTTON_R } },
        { .actionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .actionId = Release,
          .deviceButtons = { KEYBOARD_BUTTON_ESCAPE } },
    };
    addActionMappings(mappings, TF_ARRAY_COUNT(mappings), INPUT_ACTION_MAPPING_TARGET_ALL);
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(mappings); ++i)
    {
        const InputActionDesc action = { .actionId = mappings[i].actionId, .pFunction = onInput, .pUserData = this };
        addInputAction(&action);
    }
}

FreeCameraController::~FreeCameraController()
{
    setEnableCaptureInput(false);
    exitInputSystem();
    exitCameraController(pCamera);
}

void FreeCameraController::update(float deltaTime, uint32_t width, uint32_t height, bool focused)
{
    look = float2(0.0f);
    updateInputSystem(deltaTime, width, height);
    if (!focused)
    {
        captured = false;
        boost = false;
        setEnableCaptureInput(false);
        stop();
    }

    CameraMotionParameters currentMotion = motion;
    if (boost)
        currentMotion.movementSpeed *= boostMultiplier;
    pCamera->setMotionParameters(currentMotion);
    pCamera->onMove(float2(-movement.x, movement.y));
    pCamera->onMoveY(vertical);
    pCamera->onRotate(float2(-look.x, look.y));
    pCamera->update(TF_MIN(deltaTime, 0.1f));

    vec2 rotation = pCamera->getRotationXY();
    rotation.setX(TF_MAX(-1.553343f, TF_MIN(1.553343f, (float)rotation.getX())));
    pCamera->setViewRotationXY(rotation);
}

mat4 FreeCameraController::getViewMatrix() const { return pCamera->getViewMatrix(); }

vec3 FreeCameraController::getPosition() const { return pCamera->getViewPosition(); }
