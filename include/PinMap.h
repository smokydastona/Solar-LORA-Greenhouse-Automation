#pragma once

#include <Arduino.h>

namespace PinMap {
constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;

constexpr gpio_num_t TEMP_WATER = GPIO_NUM_10;

constexpr gpio_num_t SERVO_TOP_VENT = GPIO_NUM_3;
constexpr gpio_num_t SERVO_BOTTOM_VENT = GPIO_NUM_4;

constexpr gpio_num_t FAN_EXHAUST = GPIO_NUM_5;
constexpr gpio_num_t FAN_INTAKE = GPIO_NUM_6;
constexpr gpio_num_t HEATER_DEFOGGER = GPIO_NUM_7;
constexpr gpio_num_t GROW_LIGHT = GPIO_NUM_11;
constexpr gpio_num_t CIRCULATION_FANS = GPIO_NUM_12;

constexpr gpio_num_t BUTTON_MODE = GPIO_NUM_13;
constexpr gpio_num_t BUTTON_FORCE_OPEN = GPIO_NUM_14;
constexpr gpio_num_t BUTTON_FORCE_CLOSE = GPIO_NUM_15;

constexpr gpio_num_t STATUS_LED = GPIO_NUM_48;
}
