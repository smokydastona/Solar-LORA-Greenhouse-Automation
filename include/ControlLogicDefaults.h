#pragma once

#include "ControlLogic.h"

namespace GreenhouseDefaults {

constexpr GreenhouseLogic::ClimateConfig CLIMATE{
    28.0F,
    23.0F,
    29.0F,
    24.0F,
    82.0F,
    72.0F,
    5.0F,
    8.0F,
    30.0F,
    10.0F,
    8U * 60U,
    16U * 60U,
    12000.0F,
};

constexpr GreenhouseLogic::SystemConfig CONTROL_SYSTEM{
    true,
    true,
    true,
};

}  // namespace GreenhouseDefaults