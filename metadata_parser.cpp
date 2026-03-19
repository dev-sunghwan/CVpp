#include "metadata_parser.h"

#include <regex>
#include <sstream>

const char* parse_status_label(ParseStatus status) {
    switch (status) {
        case ParseStatus::Success:
            return "success";
        case ParseStatus::UnknownPattern:
            return "unknown-pattern";
        case ParseStatus::NoObjects:
            return "no-objects";
        case ParseStatus::MalformedPayload:
        default:
            return "malformed-payload";
    }
}

std::string summarize_objects(const std::vector<DetectedObject>& objects) {
    if (objects.empty()) {
        return "objects=0";
    }

    std::ostringstream summary;
    summary << "objects=" << objects.size() << " ";
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& obj = objects[i];
        if (i > 0) {
            summary << "; ";
        }
        summary << "id=" << obj.id
                << ",type=" << obj.type
                << ",score=" << static_cast<int>(obj.likelihood * 100) << "%";
    }
    return summary.str();
}

bool contains_video_analytics_frame(const std::string& xml) {
    return xml.find("<tt:VideoAnalytics") != std::string::npos &&
           xml.find("<tt:Frame") != std::string::npos;
}

bool contains_object_blocks(const std::string& xml) {
    return xml.find("<tt:Object ObjectId=") != std::string::npos;
}

MetadataParseResult parse_onvif_xml(const std::string& xml) {
    MetadataParseResult result;

    const std::string start_tag = "<tt:Object ObjectId=";
    const std::string end_tag = "</tt:Object>";

    if (xml.find("<?xml") == std::string::npos) {
        result.status = ParseStatus::MalformedPayload;
        result.message = "XML declaration not found";
        return result;
    }

    std::regex id_re("ObjectId=\"(\\d+)\"");
    std::regex bbox_re("<tt:BoundingBox left=\"([0-9.-]+)\" top=\"([0-9.-]+)\" right=\"([0-9.-]+)\" bottom=\"([0-9.-]+)\"");
    std::regex class_type_re("<tt:Class>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:Class>");
    std::regex candidate_re("<tt:ClassCandidate>[\\s\\S]*?<tt:Type>([^<]+)</tt:Type>[\\s\\S]*?<tt:Likelihood>([0-9.-]+)</tt:Likelihood>[\\s\\S]*?</tt:ClassCandidate>");
    std::regex vehicle_re("<tt:VehicleInfo>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:VehicleInfo>");
    std::regex human_re("<tt:HumanInfo>[\\s\\S]*?<tt:Type Likelihood=\"([0-9.-]+)\">([^<]+)</tt:Type>[\\s\\S]*?</tt:HumanInfo>");

    bool found_object_block = false;
    bool found_unknown_pattern = false;
    size_t pos = 0;
    while ((pos = xml.find(start_tag, pos)) != std::string::npos) {
        size_t end = xml.find(end_tag, pos);
        if (end == std::string::npos) {
            result.status = ParseStatus::MalformedPayload;
            result.message = "Object block did not close cleanly";
            return result;
        }

        found_object_block = true;
        end += end_tag.size();
        std::string block = xml.substr(pos, end - pos);
        pos = end;

        DetectedObject obj;
        bool has_any_detail = false;
        std::smatch id_m;
        if (std::regex_search(block, id_m, id_re)) {
            obj.id = std::stoi(id_m[1].str());
        }

        std::smatch bbox_m;
        if (std::regex_search(block, bbox_m, bbox_re)) {
            obj.left = std::stof(bbox_m[1].str());
            obj.top = std::stof(bbox_m[2].str());
            obj.right = std::stof(bbox_m[3].str());
            obj.bottom = std::stof(bbox_m[4].str());
        }

        std::smatch detail_m;
        if (std::regex_search(block, detail_m, vehicle_re) || std::regex_search(block, detail_m, human_re)) {
            obj.likelihood = std::stof(detail_m[1].str());
            obj.type = detail_m[2].str();
            has_any_detail = true;
        } else {
            std::smatch class_m;
            if (std::regex_search(block, class_m, class_type_re)) {
                obj.likelihood = std::stof(class_m[1].str());
                obj.type = class_m[2].str();
                has_any_detail = true;
            } else {
                std::smatch candidate_m;
                if (std::regex_search(block, candidate_m, candidate_re)) {
                    obj.type = candidate_m[1].str();
                    obj.likelihood = std::stof(candidate_m[2].str());
                    has_any_detail = true;
                }
            }
        }

        if (!has_any_detail) {
            found_unknown_pattern = true;
        }

        result.objects.push_back(obj);
    }

    if (!found_object_block) {
        result.status = ParseStatus::NoObjects;
        result.message = "No <tt:Object> blocks found";
        return result;
    }

    if (result.objects.empty()) {
        result.status = ParseStatus::NoObjects;
        result.message = "No objects parsed";
        return result;
    }

    result.status = found_unknown_pattern ? ParseStatus::UnknownPattern : ParseStatus::Success;
    result.message = found_unknown_pattern ? "Objects parsed but some class patterns were unknown" : "Objects parsed successfully";
    return result;
}
