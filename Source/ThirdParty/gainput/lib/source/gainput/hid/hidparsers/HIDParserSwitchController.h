#pragma once

#include "Core/IConfig.h"

struct HIDDeviceInfo;
struct HIDController;

bool HIDIsSupportedSwitchController(HIDDeviceInfo* devInfo);
int HIDOpenSwitchController(HIDDeviceInfo* devInfo, HIDController* controller, uint8_t playerNum);
