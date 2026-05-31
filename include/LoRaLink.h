#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace GreenhouseLoRa {

constexpr size_t kNodeIdSize = 24;
constexpr size_t kPayloadSize = 192;
constexpr size_t kQueueCapacity = 8;
constexpr size_t kRecentSequenceWindow = 8;

struct TelemetryFrame {
  char nodeId[kNodeIdSize]{};
  char payload[kPayloadSize]{};
  uint32_t sessionId = 0;
  uint32_t sequence = 0;
  uint32_t crc32 = 0;
  uint32_t createdAtMs = 0;
  uint32_t lastAttemptAtMs = 0;
  uint8_t attempts = 0;
  bool requireAck = true;
};

struct LinkStats {
  bool enabled = false;
  bool backendReady = false;
  bool backendConfigured = false;
  uint32_t enqueuedFrames = 0;
  uint32_t sentFrames = 0;
  uint32_t droppedFrames = 0;
  uint32_t failedSendAttempts = 0;
  uint32_t duplicateInboundFrames = 0;
  uint32_t invalidInboundFrames = 0;
  uint32_t lastAcceptedSequence = 0;
  uint32_t lastSequence = 0;
  uint32_t sessionId = 0;
  uint8_t queueDepth = 0;
  int lastRssi = 0;
  float lastSnr = 0.0F;
  bool signalAvailable = false;
};

struct SequenceWindowEntry {
  uint32_t sessionId = 0;
  uint32_t sequence = 0;
};

inline uint32_t crc32Update(uint32_t crc, uint8_t byte) {
  uint32_t next = crc ^ static_cast<uint32_t>(byte);
  for (uint8_t bit = 0; bit < 8U; ++bit) {
    const uint32_t mask = (next & 1U) != 0U ? 0xEDB88320UL : 0U;
    next = (next >> 1U) ^ mask;
  }
  return next;
}

inline uint32_t crc32(const char *payload) {
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t index = 0; payload[index] != '\0'; ++index) {
    crc = crc32Update(crc, static_cast<uint8_t>(payload[index]));
  }
  return crc ^ 0xFFFFFFFFUL;
}

inline uint32_t deriveSessionId(const char *nodeId, uint32_t seed) {
  uint32_t hash = 2166136261UL ^ seed;
  for (size_t index = 0; nodeId[index] != '\0'; ++index) {
    hash ^= static_cast<uint8_t>(nodeId[index]);
    hash *= 16777619UL;
  }
  return hash == 0U ? 1U : hash;
}

class ILoRaTransport {
 public:
  virtual ~ILoRaTransport() = default;
  virtual bool begin() = 0;
  virtual bool ready() const = 0;
  virtual bool send(const TelemetryFrame &frame, LinkStats &stats) = 0;
  virtual void poll(LinkStats &stats) = 0;
};

class DisabledTransport final : public ILoRaTransport {
 public:
  bool begin() override {
    return false;
  }

  bool ready() const override {
    return false;
  }

  bool send(const TelemetryFrame &, LinkStats &stats) override {
    stats.backendReady = false;
    stats.backendConfigured = false;
    stats.signalAvailable = false;
    return false;
  }

  void poll(LinkStats &stats) override {
    stats.backendReady = false;
    stats.backendConfigured = false;
    stats.signalAvailable = false;
  }
};

class TelemetryQueue {
 public:
  bool enqueue(const TelemetryFrame &frame) {
    if (size_ >= kQueueCapacity) {
      return false;
    }
    frames_[tail_] = frame;
    tail_ = static_cast<uint8_t>((tail_ + 1U) % kQueueCapacity);
    ++size_;
    return true;
  }

  TelemetryFrame *front() {
    if (size_ == 0U) {
      return nullptr;
    }
    return &frames_[head_];
  }

  void pop() {
    if (size_ == 0U) {
      return;
    }
    head_ = static_cast<uint8_t>((head_ + 1U) % kQueueCapacity);
    --size_;
  }

  uint8_t size() const {
    return size_;
  }

 private:
  TelemetryFrame frames_[kQueueCapacity]{};
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t size_ = 0;
};

class LinkService {
 public:
  explicit LinkService(ILoRaTransport &transport) : transport_(transport) {}

  void begin(bool enabled, const char *nodeId, uint8_t maxRetries, uint32_t retryBackoffMs, uint32_t sessionSeed = 0U) {
    stats_ = LinkStats{};
    stats_.enabled = enabled;
    maxRetries_ = maxRetries;
    retryBackoffMs_ = retryBackoffMs;
    strncpy(nodeId_, nodeId, sizeof(nodeId_) - 1U);
    nodeId_[sizeof(nodeId_) - 1U] = '\0';
    stats_.sessionId = deriveSessionId(nodeId_, sessionSeed);
    if (!enabled) {
      return;
    }

    stats_.backendConfigured = true;
    stats_.backendReady = transport_.begin();
  }

  bool queueTelemetry(const char *payload, uint32_t now) {
    if (!stats_.enabled) {
      return false;
    }

    TelemetryFrame frame{};
    strncpy(frame.nodeId, nodeId_, sizeof(frame.nodeId) - 1U);
    strncpy(frame.payload, payload, sizeof(frame.payload) - 1U);
    frame.sessionId = stats_.sessionId;
    frame.sequence = ++sequence_;
    frame.crc32 = crc32(frame.payload);
    frame.createdAtMs = now;
    frame.lastAttemptAtMs = 0;
    if (!queue_.enqueue(frame)) {
      ++stats_.droppedFrames;
      stats_.queueDepth = queue_.size();
      return false;
    }

    ++stats_.enqueuedFrames;
    stats_.lastSequence = frame.sequence;
    stats_.queueDepth = queue_.size();
    return true;
  }

  void poll(uint32_t now) {
    if (!stats_.enabled) {
      return;
    }

    transport_.poll(stats_);
    TelemetryFrame *frame = queue_.front();
    if (frame == nullptr) {
      stats_.queueDepth = 0;
      return;
    }
    if (!transport_.ready()) {
      stats_.backendReady = false;
      stats_.queueDepth = queue_.size();
      return;
    }
    if (frame->lastAttemptAtMs > 0U && (now - frame->lastAttemptAtMs) < retryBackoffMs_) {
      stats_.queueDepth = queue_.size();
      return;
    }

    frame->lastAttemptAtMs = now;
    ++frame->attempts;
    if (transport_.send(*frame, stats_)) {
      ++stats_.sentFrames;
      queue_.pop();
    } else {
      ++stats_.failedSendAttempts;
      if (frame->attempts > maxRetries_) {
        ++stats_.droppedFrames;
        queue_.pop();
      }
    }
    stats_.queueDepth = queue_.size();
  }

  bool validateInboundFrame(uint32_t sessionId, uint32_t sequence, uint32_t payloadCrc32, const char *payload) {
    if (!stats_.enabled || sessionId != stats_.sessionId || sequence == 0U || payload == nullptr) {
      ++stats_.invalidInboundFrames;
      return false;
    }
    if (crc32(payload) != payloadCrc32) {
      ++stats_.invalidInboundFrames;
      return false;
    }
    if (seenRecently(sessionId, sequence)) {
      ++stats_.duplicateInboundFrames;
      return false;
    }

    rememberSequence(sessionId, sequence);
    stats_.lastAcceptedSequence = sequence;
    return true;
  }

  const LinkStats &stats() const {
    return stats_;
  }

 private:
  bool seenRecently(uint32_t sessionId, uint32_t sequence) const {
    for (const auto &entry : recentInbound_) {
      if (entry.sessionId == sessionId && entry.sequence == sequence) {
        return true;
      }
    }
    return false;
  }

  void rememberSequence(uint32_t sessionId, uint32_t sequence) {
    recentInbound_[recentInboundIndex_].sessionId = sessionId;
    recentInbound_[recentInboundIndex_].sequence = sequence;
    recentInboundIndex_ = (recentInboundIndex_ + 1U) % kRecentSequenceWindow;
  }

  ILoRaTransport &transport_;
  TelemetryQueue queue_;
  LinkStats stats_{};
  SequenceWindowEntry recentInbound_[kRecentSequenceWindow]{};
  uint8_t recentInboundIndex_ = 0;
  char nodeId_[kNodeIdSize]{};
  uint32_t sequence_ = 0;
  uint8_t maxRetries_ = 0;
  uint32_t retryBackoffMs_ = 0;
};

inline int buildCompactTelemetry(char *buffer,
                                 size_t bufferSize,
                                 const char *nodeId,
                                 const char *mode,
                                 bool safeMode,
                                 int healthScore,
                                 bool airAvailable,
                                 float airTempC,
                                 float humidityPct,
                                 bool batteryAvailable,
                                 float batteryVoltage,
                                 int batteryPercent) {
  return snprintf(buffer,
                  bufferSize,
                  "n=%s;m=%s;s=%u;h=%d;a=%u;t=%.1f;rh=%.0f;b=%u;bv=%.2f;bp=%d",
                  nodeId,
                  mode,
                  safeMode ? 1 : 0,
                  healthScore,
                  airAvailable ? 1 : 0,
                  static_cast<double>(airTempC),
                  static_cast<double>(humidityPct),
                  batteryAvailable ? 1 : 0,
                  static_cast<double>(batteryVoltage),
                  batteryPercent);
}

}  // namespace GreenhouseLoRa