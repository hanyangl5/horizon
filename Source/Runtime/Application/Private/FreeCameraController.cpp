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
    const bool            ended = pInput->mPhase == INPUT_ACTION_PHASE_CANCELED || pInput->mPhase == INPUT_ACTION_PHASE_ENDED;
    switch (pInput->mActionId)
    {
    case Move:
        pController->movement = ended ? float2(0.0f) : pInput->mFloat2;
        break;
    case MoveVertical:
        pController->vertical = ended ? 0.0f : pInput->mFloat;
        break;
    case Look:
        if (pController->captured && !ended)
            pController->look = pInput->mFloat2;
        break;
    case Capture:
        pController->captured = !ended && pInput->mBool;
        setEnableCaptureInput(pController->captured);
        break;
    case Boost:
        pController->boost = !ended && pInput->mBool;
        break;
    case Reset:
        if (pInput->mPhase == INPUT_ACTION_PHASE_STARTED)
        {
            pController->pCamera->resetView();
            pController->stop();
        }
        break;
    case Release:
        if (pInput->mBool)
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
        { .mActionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
          .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .mActionId = Move,
          .mDeviceButtons = { KEYBOARD_BUTTON_D, KEYBOARD_BUTTON_A, KEYBOARD_BUTTON_W, KEYBOARD_BUTTON_S } },
        { .mActionMappingType = INPUT_ACTION_MAPPING_COMPOSITE,
          .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .mActionId = MoveVertical,
          .mDeviceButtons = { KEYBOARD_BUTTON_E, KEYBOARD_BUTTON_Q },
          .mCompositeUseSingleAxis = true },
        { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE,
          .mActionId = Look,
          .mDeviceButtons = { MOUSE_BUTTON_AXIS_X },
          .mNumAxis = 2,
          .mScale = 0.002f,
          .mScaleByDT = true },
        { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_MOUSE, .mActionId = Capture, .mDeviceButtons = { MOUSE_BUTTON_RIGHT } },
        { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .mActionId = Boost,
          .mDeviceButtons = { KEYBOARD_BUTTON_SHIFT_L } },
        { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD, .mActionId = Reset, .mDeviceButtons = { KEYBOARD_BUTTON_R } },
        { .mActionMappingDeviceTarget = INPUT_ACTION_MAPPING_TARGET_KEYBOARD,
          .mActionId = Release,
          .mDeviceButtons = { KEYBOARD_BUTTON_ESCAPE } },
    };
    addActionMappings(mappings, TF_ARRAY_COUNT(mappings), INPUT_ACTION_MAPPING_TARGET_ALL);
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(mappings); ++i)
    {
        const InputActionDesc action = { .mActionId = mappings[i].mActionId, .pFunction = onInput, .pUserData = this };
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
