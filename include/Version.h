#pragma once

namespace GreenhouseVersion {

#ifndef GH_BUILD_VERSION
#define GH_BUILD_VERSION "0.1.0"
#endif

#ifndef GH_BUILD_LABEL
#define GH_BUILD_LABEL "manual"
#endif

constexpr const char *kFirmwareSeries = "0.1";
constexpr const char *kFirmwareVersion = GH_BUILD_VERSION;
constexpr const char *kFirmwareBuildLabel = GH_BUILD_LABEL;
constexpr const char *kFirmwareCodename = "Foundation Patch";

}  // namespace GreenhouseVersion