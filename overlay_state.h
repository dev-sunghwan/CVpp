#pragma once

#include <chrono>
#include <string>

#include "metadata_types.h"

struct OverlayRuntimeState {
    std::string reason = "No metadata";
    std::chrono::steady_clock::time_point reason_since{};
};

void set_overlay_reason(OverlayRuntimeState& state,
                        const std::string& reason,
                        std::chrono::steady_clock::time_point now);

std::string overlay_reason_from_parse(ParseStatus status,
                                      const std::string& message,
                                      int object_count,
                                      bool has_video_analytics);

std::string derive_overlay_reason_for_ui(bool has_video_frame,
                                         bool metadata_enabled,
                                         bool metadata_started,
                                         bool has_raw_metadata,
                                         bool metadata_is_fresh,
                                         int visible_overlay_count,
                                         const std::string& runtime_reason);
