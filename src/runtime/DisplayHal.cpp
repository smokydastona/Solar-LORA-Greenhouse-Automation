#include "runtime/DisplayHal.h"

#include <Arduino.h>

#include "PinMap.h"
#include "Settings.h"

namespace GreenhouseHal {

bool initDisplay(Adafruit_SSD1306 &display) {
  pinMode(PinMap::OLED_VEXT, OUTPUT);
  digitalWrite(PinMap::OLED_VEXT, LOW);
  delay(50);

  pinMode(PinMap::OLED_RESET, OUTPUT);
  digitalWrite(PinMap::OLED_RESET, LOW);
  delay(20);
  digitalWrite(PinMap::OLED_RESET, HIGH);
  delay(20);

  const bool displayReady = display.begin(SSD1306_SWITCHCAPVCC, Settings::OLED_ADDRESS);
  if (displayReady) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.display();
  }
  return displayReady;
}

void showBootBanner(Adafruit_SSD1306 &display,
                    esp_reset_reason_t lastResetReason,
                    const char *firmwareVersion,
                    const char *firmwareCodename,
                    const char *(*resetReasonLabel)(esp_reset_reason_t)) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Mini Greenhouse\n");
  display.printf("FW %s\n", firmwareVersion);
  display.printf("%s\n", firmwareCodename);
  display.printf("Reset:%s", resetReasonLabel(lastResetReason));
  display.display();
  delay(1200);
}

}  // namespace GreenhouseHal