#include "overlay_state.h"

namespace {
constexpr const char* kVideoNotReady = "Video not ready";
constexpr const char* kMetadataNotStarted = "Metadata not started";
constexpr const char* kNoMetadata = "No metadata";
constexpr const char* kMetadataNoObjects = "Metadata, no objects";
constexpr const char* kIncompleteObjectMetadata = "Incomplete object metadata";
constexpr const char* kOverlayVisible = "Overlay visible";
constexpr const char* kOverlayCleared = "Overlay cleared";
}

void set_overlay_reason(OverlayRuntimeState& state,
                        const std::string& reason,
                        std::chrono::steady_clock::time_point now) {
    if (state.reason != reason) {
        state.reason = reason;
        state.reason_since = now;
    } else if (state.reason_since == std::chrono::steady_clock::time_point{}) {
        state.reason_since = now;
    }
}

std::string overlay_reason_from_parse(ParseStatus status,
                                      const std::string& message,
                                      int object_count,
                                      bool has_video_analytics) {
    if (object_count > 0) {
        return kOverlayVisible;
    }

    if (!has_video_analytics) {
        return kMetadataNoObjects;
    }

    if (status == ParseStatus::NoObjects || message == "metadata-without-objects") {
        return kMetadataNoObjects;
    }

    if (status == ParseStatus::MalformedPayload || message == "continuation-without-xml-start") {
        return kIncompleteObjectMetadata;
    }

    return kOverlayCleared;
}

std::string derive_overlay_reason_for_ui(bool has_video_frame,
                                         bool metadata_enabled,
                                         bool metadata_started,
                                         bool has_raw_metadata,
                                         bool metadata_is_fresh,
                                         int visible_overlay_count,
                                         const std::string& runtime_reason) {
    if (!has_video_frame) {
        return kVideoNotReady;
    }
    if (!metadata_enabled || !metadata_started) {
        return kMetadataNotStarted;
    }
    if (visible_overlay_count > 0 && metadata_is_fresh) {
        return kOverlayVisible;
    }
    if (!has_raw_metadata) {
        return kNoMetadata;
    }
    if (!metadata_is_fresh && runtime_reason == kOverlayVisible) {
        return kOverlayCleared;
    }
    if (!runtime_reason.empty()) {
        return runtime_reason;
    }
    return kNoMetadata;
}
