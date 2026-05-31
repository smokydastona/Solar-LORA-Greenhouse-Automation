#pragma once

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ControlLogic.h"

namespace GreenhouseRemote {

constexpr size_t kTopicSize = 128;

enum class ModeCommand : uint8_t {
  invalid = 0,
  automatic,
  open,
  closed,
};

enum class CommandStatus : uint8_t {
  none = 0,
  accepted,
  rejectedInvalidPayload,
  rejectedTopic,
  rejectedSafeMode,
};

struct CommandAudit {
  bool available = false;
  ModeCommand requested = ModeCommand::invalid;
  GreenhouseLogic::ControlMode applied = GreenhouseLogic::ControlMode::automatic;
  CommandStatus status = CommandStatus::none;
  uint32_t receivedAtMs = 0;
};

inline const char *modeCommandLabel(ModeCommand command) {
  switch (command) {
    case ModeCommand::automatic:
      return "AUTO";
    case ModeCommand::open:
      return "OPEN";
    case ModeCommand::closed:
      return "CLOSED";
    case ModeCommand::invalid:
      break;
  }
  return "INVALID";
}

inline const char *commandStatusLabel(CommandStatus status) {
  switch (status) {
    case CommandStatus::none:
      return "NONE";
    case CommandStatus::accepted:
      return "ACCEPTED";
    case CommandStatus::rejectedInvalidPayload:
      return "REJECTED_INVALID_PAYLOAD";
    case CommandStatus::rejectedTopic:
      return "REJECTED_TOPIC";
    case CommandStatus::rejectedSafeMode:
      return "REJECTED_SAFE_MODE";
  }
  return "NONE";
}

inline const char *controlModeLabel(GreenhouseLogic::ControlMode mode) {
  switch (mode) {
    case GreenhouseLogic::ControlMode::automatic:
      return "AUTO";
    case GreenhouseLogic::ControlMode::forceOpen:
      return "OPEN";
    case GreenhouseLogic::ControlMode::forceClosed:
      return "CLOSED";
  }
  return "AUTO";
}

inline GreenhouseLogic::ControlMode toControlMode(ModeCommand command) {
  switch (command) {
    case ModeCommand::open:
      return GreenhouseLogic::ControlMode::forceOpen;
    case ModeCommand::closed:
      return GreenhouseLogic::ControlMode::forceClosed;
    case ModeCommand::automatic:
    case ModeCommand::invalid:
      break;
  }
  return GreenhouseLogic::ControlMode::automatic;
}

inline void buildModeCommandTopic(char *buffer, size_t bufferSize, const char *baseTopic) {
  snprintf(buffer, bufferSize, "%s/command/mode/set", baseTopic);
}

inline void buildModeStateTopic(char *buffer, size_t bufferSize, const char *baseTopic) {
  snprintf(buffer, bufferSize, "%s/command/mode/state", baseTopic);
}

inline void buildModeResultTopic(char *buffer, size_t bufferSize, const char *baseTopic) {
  snprintf(buffer, bufferSize, "%s/command/mode/result", baseTopic);
}

inline bool topicMatches(const char *actual, const char *expected) {
  return actual != nullptr && expected != nullptr && strcmp(actual, expected) == 0;
}

inline ModeCommand parseModeCommand(const char *payload) {
  if (payload == nullptr) {
    return ModeCommand::invalid;
  }

  char normalized[16]{};
  size_t length = 0;
  while (payload[length] != '\0' && length < (sizeof(normalized) - 1U)) {
    const char current = payload[length];
    if (!isspace(static_cast<unsigned char>(current))) {
      normalized[length] = static_cast<char>(toupper(static_cast<unsigned char>(current)));
    } else {
      normalized[length] = current;
    }
    ++length;
  }
  normalized[length] = '\0';

  while (length > 0U && isspace(static_cast<unsigned char>(normalized[length - 1U]))) {
    normalized[length - 1U] = '\0';
    --length;
  }

  const char *trimmed = normalized;
  while (*trimmed != '\0' && isspace(static_cast<unsigned char>(*trimmed))) {
    ++trimmed;
  }

  if (strcmp(trimmed, "AUTO") == 0) {
    return ModeCommand::automatic;
  }
  if (strcmp(trimmed, "OPEN") == 0) {
    return ModeCommand::open;
  }
  if (strcmp(trimmed, "CLOSED") == 0) {
    return ModeCommand::closed;
  }
  return ModeCommand::invalid;
}

inline CommandAudit evaluateModeCommand(bool safeMode, const char *topic, const char *expectedTopic, const char *payload, uint32_t now) {
  CommandAudit audit{};
  audit.available = true;
  audit.receivedAtMs = now;

  if (!topicMatches(topic, expectedTopic)) {
    audit.status = CommandStatus::rejectedTopic;
    return audit;
  }

  audit.requested = parseModeCommand(payload);
  if (audit.requested == ModeCommand::invalid) {
    audit.status = CommandStatus::rejectedInvalidPayload;
    return audit;
  }

  audit.applied = toControlMode(audit.requested);
  if (safeMode) {
    audit.status = CommandStatus::rejectedSafeMode;
    return audit;
  }

  audit.status = CommandStatus::accepted;
  return audit;
}

}  // namespace GreenhouseRemote