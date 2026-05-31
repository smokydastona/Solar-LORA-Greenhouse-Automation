#pragma once

#include <Arduino.h>

namespace PinMap {
constexpr gpio_num_t I2C_SDA = GPIO_NUM_17;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_18;
constexpr gpio_num_t OLED_RESET = GPIO_NUM_21;
constexpr gpio_num_t OLED_VEXT = GPIO_NUM_36;

constexpr gpio_num_t TEMP_AIR_DHT = GPIO_NUM_16;

constexpr gpio_num_t TEMP_WATER = GPIO_NUM_15;

constexpr gpio_num_t SERVO_TOP_VENT = GPIO_NUM_7;
constexpr gpio_num_t SERVO_BOTTOM_VENT = GPIO_NUM_6;

constexpr gpio_num_t FAN_EXHAUST = GPIO_NUM_5;
constexpr gpio_num_t FAN_INTAKE = GPIO_NUM_42;
constexpr gpio_num_t HEATER_DEFOGGER = GPIO_NUM_41;
constexpr gpio_num_t GROW_LIGHT = GPIO_NUM_40;
constexpr gpio_num_t CIRCULATION_FANS = GPIO_NUM_39;

constexpr gpio_num_t BUTTON_MODE = GPIO_NUM_47;
constexpr gpio_num_t BUTTON_FORCE_OPEN = GPIO_NUM_38;
constexpr gpio_num_t BUTTON_FORCE_CLOSE = GPIO_NUM_33;

constexpr gpio_num_t STATUS_LED = GPIO_NUM_35;
}
