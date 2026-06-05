#pragma once

#include <Adafruit_SSD1306.h>
#include <esp_system.h>

namespace GreenhouseHal {

bool initDisplay(Adafruit_SSD1306 &display);
void setDisplayPower(Adafruit_SSD1306 &display, bool enabled);
void setDisplayContrast(Adafruit_SSD1306 &display, uint8_t contrast);
void showBootBanner(Adafruit_SSD1306 &display,
                    esp_reset_reason_t lastResetReason,
                    const char *firmwareVersion,
                    const char *firmwareCodename,
                    const char *(*resetReasonLabel)(esp_reset_reason_t));

}  // namespace GreenhouseHal