#pragma once

#include <string>
#include <vector>

#include "metadata_types.h"

const char* parse_status_label(ParseStatus status);
std::string summarize_objects(const std::vector<DetectedObject>& objects);
MetadataParseResult parse_onvif_xml(const std::string& xml);
bool contains_video_analytics_frame(const std::string& xml);
bool contains_object_blocks(const std::string& xml);
