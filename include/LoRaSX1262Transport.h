#pragma once

#include "LoRaLink.h"

#ifdef ARDUINO

#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>

#include "PinMap.h"

namespace GreenhouseLoRa {

class SX1262Transport final : public ILoRaTransport {
 public:
  SX1262Transport(float frequencyMHz,
                  float bandwidthKHz,
                  uint8_t spreadingFactor,
                  uint8_t codingRate,
                  uint8_t syncWord,
                  int8_t outputPowerDbm,
                  uint16_t preambleLength,
                  uint32_t ackTimeoutMs)
      : module_(PinMap::LORA_NSS,
                PinMap::LORA_DIO1,
                PinMap::LORA_RESET,
                PinMap::LORA_BUSY,
                spi_),
        radio_(&module_),
        frequencyMHz_(frequencyMHz),
        bandwidthKHz_(bandwidthKHz),
        spreadingFactor_(spreadingFactor),
        codingRate_(codingRate),
        syncWord_(syncWord),
        outputPowerDbm_(outputPowerDbm),
        preambleLength_(preambleLength),
        ackTimeoutMs_(ackTimeoutMs) {}

  bool begin() override {
    spi_.begin(PinMap::LORA_SCK, PinMap::LORA_MISO, PinMap::LORA_MOSI, PinMap::LORA_NSS);
    const int16_t state = radio_.begin(frequencyMHz_,
                                       bandwidthKHz_,
                                       spreadingFactor_,
                                       codingRate_,
                                       syncWord_,
                                       outputPowerDbm_,
                                       preambleLength_);
    lastErrorCode_ = state;
    if (state != RADIOLIB_ERR_NONE) {
      ready_ = false;
      return false;
    }

    radio_.setDio2AsRfSwitch(true);
    ready_ = true;
    return true;
  }

  bool ready() const override {
    return ready_;
  }

  bool send(const TelemetryFrame &frame, LinkStats &stats) override {
    if (!ready_) {
      stats.backendReady = false;
      stats.lastErrorCode = lastErrorCode_;
      return false;
    }

    char packet[kWirePacketSize]{};
    buildWireTelemetryPacket(packet, sizeof(packet), frame);

    const uint32_t now = millis();
    const int16_t txState = radio_.transmit(packet);
    lastErrorCode_ = txState;
    stats.lastErrorCode = txState;
    stats.lastTransmitAtMs = now;
    if (txState != RADIOLIB_ERR_NONE) {
      ready_ = false;
      stats.backendReady = false;
      return false;
    }

    if (!frame.requireAck) {
      stats.backendReady = true;
      stats.signalAvailable = true;
      return true;
    }

    String ack;
    const int16_t ackState = radio_.receive(ack, ackTimeoutMs_);
    lastErrorCode_ = ackState;
    stats.lastErrorCode = ackState;
    if (ackState != RADIOLIB_ERR_NONE) {
      ++stats.ackTimeouts;
      stats.backendReady = true;
      stats.signalAvailable = false;
      return false;
    }

    uint32_t sessionId = 0;
    uint32_t sequence = 0;
    uint32_t payloadCrc32 = 0;
    if (!parseAckPacket(ack.c_str(), sessionId, sequence, payloadCrc32) ||
        sessionId != frame.sessionId ||
        sequence != frame.sequence ||
        payloadCrc32 != frame.crc32) {
      ++stats.invalidInboundFrames;
      stats.backendReady = true;
      stats.signalAvailable = true;
      stats.lastRssi = static_cast<int>(radio_.getRSSI());
      stats.lastSnr = radio_.getSNR();
      return false;
    }

    ++stats.acknowledgedFrames;
    stats.backendReady = true;
    stats.signalAvailable = true;
    stats.lastAckAtMs = millis();
    stats.lastRssi = static_cast<int>(radio_.getRSSI());
    stats.lastSnr = radio_.getSNR();
    return true;
  }

  void poll(LinkStats &stats) override {
    stats.backendConfigured = true;
    stats.backendReady = ready_;
    stats.lastErrorCode = lastErrorCode_;
  }

 private:
  SPIClass spi_;
  Module module_;
  SX1262 radio_;
  float frequencyMHz_ = 0.0F;
  float bandwidthKHz_ = 0.0F;
  uint8_t spreadingFactor_ = 0;
  uint8_t codingRate_ = 0;
  uint8_t syncWord_ = 0;
  int8_t outputPowerDbm_ = 0;
  uint16_t preambleLength_ = 0;
  uint32_t ackTimeoutMs_ = 0;
  bool ready_ = false;
  int16_t lastErrorCode_ = 0;
};

}  // namespace GreenhouseLoRa

#endif