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

void FreeCameraController::Stop()
{
    movement = float2(0.0f);
    look = float2(0.0f);
    vertical = 0.0f;
    pCamera->MoveTo(pCamera->GetViewPosition());
}

bool FreeCameraController::OnInput(InputActionContext* pInput)
{
    FreeCameraController* pController = (FreeCameraController*)pInput->UserData;
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
        SetEnableCaptureInput(pController->captured);
        break;
    case Boost:
        pController->boost = !ended && pInput->boolValue;
        break;
    case Reset:
        if (pInput->phase == INPUT_ACTION_PHASE_STARTED)
        {
            pController->pCamera->ResetView();
            pController->Stop();
        }
        break;
    case Release:
        if (pInput->boolValue)
        {
            pController->captured = false;
            SetEnableCaptureInput(false);
        }
        break;
    }
    return true;
}

FreeCameraController::FreeCameraController(const FreeCameraControllerDesc& desc)
{
    ASSERT(desc.pWindow);
    pCamera = InitFpsCameraController(desc.position, desc.LookAt);
    motion = desc.motion;
    boostMultiplier = desc.boostMultiplier;
    ASSERT(pCamera);

    InputSystemDesc inputDesc = { .pWindow = desc.pWindow };
    const bool      inputInitialized = InitInputSystem(&inputDesc);
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
    AddActionMappings(mappings, TF_ARRAY_COUNT(mappings), INPUT_ACTION_MAPPING_TARGET_ALL);
    for (uint32_t i = 0; i < TF_ARRAY_COUNT(mappings); ++i)
    {
        const InputActionDesc action = { .actionId = mappings[i].actionId, .pFunction = OnInput, .UserData = this };
        AddInputAction(&action);
    }
}

FreeCameraController::~FreeCameraController()
{
    SetEnableCaptureInput(false);
    ExitInputSystem();
    ExitCameraController(pCamera);
}

void FreeCameraController::Update(float deltaTime, uint32_t Width, uint32_t Height, bool focused)
{
    look = float2(0.0f);
    UpdateInputSystem(deltaTime, Width, Height);
    if (!focused)
    {
        captured = false;
        boost = false;
        SetEnableCaptureInput(false);
        Stop();
    }

    CameraMotionParameters currentMotion = motion;
    if (boost)
        currentMotion.movementSpeed *= boostMultiplier;
    pCamera->SetMotionParameters(currentMotion);
    pCamera->OnMove(float2(-movement.x, movement.y));
    pCamera->OnMoveY(vertical);
    pCamera->OnRotate(float2(-look.x, look.y));
    pCamera->Update(TF_MIN(deltaTime, 0.1f));

    Vector2 rotation = pCamera->GetRotationXY();
    rotation.setX(TF_MAX(-1.553343f, TF_MIN(1.553343f, (float)rotation.getX())));
    pCamera->SetViewRotationXY(rotation);
}

Matrix4 FreeCameraController::GetViewMatrix() const { return pCamera->GetViewMatrix(); }
Matrix4 FreeCameraController::GetWorldMatrix() const
{
    return orthoInverse(Matrix4::scale(Vector3(-1.0f, 1.0f, -1.0f)) * pCamera->GetViewMatrix());
}

Vector3 FreeCameraController::GetPosition() const { return pCamera->GetViewPosition(); }

void FreeCameraController::SetPose(const Vector3& Position, const Vector3& Forward)
{
    pCamera->MoveTo(Position);
    pCamera->LookAt(Position + Forward);
}
