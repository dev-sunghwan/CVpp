#include "qt_shell_window.h"

#include <chrono>
#include <map>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <QCloseEvent>
#include <QComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifdef signals
#undef signals
#endif

#include <gst/gst.h>
#include <opencv2/opencv.hpp>

#include "metadata_rtsp_session.h"
#include "session_logger.h"
#include "shared_app_state.h"
#include "video_rtsp_session.h"

namespace {
constexpr int kFreshMetadataWindowMs = 1500;
constexpr int kVideoStartupRetryHintSeconds = 10;
constexpr int kMetadataStartupHoldMs = 1500;

struct SummaryFields {
    std::string status = "unknown";
    std::string message;
    int objects = 0;
    QStringList fragments;
};

struct RuntimeSnapshot {
    cv::Mat frame;
    std::vector<DetectedObject> overlay_objects;
    bool has_video_frame = false;
    bool metadata_is_fresh = false;
    int total_raw_metadata_samples = 0;
    int last_parsed_object_count = 0;
    int total_detection_events = 0;
    int total_parsed_payloads = 0;
    std::chrono::steady_clock::time_point last_raw_metadata_seen{};
    std::string last_parse_status_text = "No metadata parsed yet";
    std::string overlay_reason = "No metadata";
    std::chrono::steady_clock::time_point overlay_reason_since{};
    ParserHealthCounts parser_health_counts;
    std::map<std::string, int> detections_by_type;
    std::unordered_map<std::string, std::unordered_set<int>> unique_id_sets_by_type;
    std::map<std::string, int> unique_ids_by_type;
    std::vector<std::string> recent_summaries;
};

struct ReadinessViewModel {
    QString runtime;
    QString video;
    QString metadata;
    QString parser;
    QString badge_text;
    QString badge_background;
    QString badge_foreground = "#ecf3f9";
    QString connection_summary;
};

void configureTwoColumnTable(QTableWidget* table, const QString& left_header, const QString& right_header) {
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({left_header, right_header});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
}

void configureThreeColumnTable(QTableWidget* table,
                               const QString& left_header,
                               const QString& middle_header,
                               const QString& right_header) {
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({left_header, middle_header, right_header});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
}

void setCompactTableHeight(QTableWidget* table, int visible_rows) {
    constexpr int kRowHeight = 24;
    constexpr int kPadding = 10;
    table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    const int height = table->horizontalHeader()->height() + (visible_rows * kRowHeight) + kPadding;
    table->setMinimumHeight(height);
    table->setMaximumHeight(height);
}

QTableWidgetItem* makeItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void setTableRow(QTableWidget* table, int row, const QString& name, const QString& value) {
    if (!table->item(row, 0)) {
        table->setItem(row, 0, makeItem(name));
    } else {
        table->item(row, 0)->setText(name);
    }

    if (!table->item(row, 1)) {
        table->setItem(row, 1, makeItem(value));
    } else {
        table->item(row, 1)->setText(value);
    }
}

void setTripleTableRow(QTableWidget* table, int row, const QString& name, const QString& detections, const QString& unique_ids) {
    if (!table->item(row, 0)) {
        table->setItem(row, 0, makeItem(name));
    } else {
        table->item(row, 0)->setText(name);
    }

    if (!table->item(row, 1)) {
        table->setItem(row, 1, makeItem(detections));
    } else {
        table->item(row, 1)->setText(detections);
    }

    if (!table->item(row, 2)) {
        table->setItem(row, 2, makeItem(unique_ids));
    } else {
        table->item(row, 2)->setText(unique_ids);
    }
}

std::map<std::string, int> toSortedMap(const std::unordered_map<std::string, int>& source) {
    return std::map<std::string, int>(source.begin(), source.end());
}

std::map<std::string, int> toSortedUniqueCountMap(const std::unordered_map<std::string, std::unordered_set<int>>& source) {
    std::map<std::string, int> result;
    for (const auto& entry : source) {
        result[entry.first] = static_cast<int>(entry.second.size());
    }
    return result;
}

int familyDetectionCount(const std::map<std::string, int>& detections, const std::vector<std::string>& family_types) {
    int total = 0;
    for (const auto& type : family_types) {
        auto it = detections.find(type);
        if (it != detections.end()) {
            total += it->second;
        }
    }
    return total;
}

int familyUniqueCount(const std::unordered_map<std::string, std::unordered_set<int>>& source,
                      const std::vector<std::string>& family_types) {
    std::unordered_set<int> ids;
    for (const auto& type : family_types) {
        auto it = source.find(type);
        if (it != source.end()) {
            ids.insert(it->second.begin(), it->second.end());
        }
    }
    return static_cast<int>(ids.size());
}

bool parseRtspDefaults(const std::string& url, QString& ip, QString& user, QString& password, QString& profile) {
    const std::string prefix = "rtsp://";
    if (url.rfind(prefix, 0) != 0) {
        return false;
    }

    const std::string remainder = url.substr(prefix.size());
    const size_t at_pos = remainder.find('@');
    const size_t slash_pos = remainder.find('/');
    if (at_pos == std::string::npos || slash_pos == std::string::npos || at_pos >= slash_pos) {
        return false;
    }

    const std::string credentials = remainder.substr(0, at_pos);
    const std::string host = remainder.substr(at_pos + 1, slash_pos - at_pos - 1);
    const std::string path = remainder.substr(slash_pos + 1);

    const size_t colon_pos = credentials.find(':');
    if (colon_pos != std::string::npos) {
        user = QString::fromStdString(credentials.substr(0, colon_pos));
        password = QString::fromStdString(credentials.substr(colon_pos + 1));
    } else {
        user = QString::fromStdString(credentials);
        password.clear();
    }
    ip = QString::fromStdString(host);

    const std::string profile_prefix = "profile";
    if (path.rfind(profile_prefix, 0) == 0) {
        size_t end = profile_prefix.size();
        while (end < path.size() && std::isdigit(static_cast<unsigned char>(path[end]))) {
            ++end;
        }
        profile = QString::fromStdString(path.substr(profile_prefix.size(), end - profile_prefix.size()));
    }

    return true;
}

std::string buildRtspUrl(const QString& ip, const QString& user, const QString& password, const QString& profile) {
    return "rtsp://" + user.toStdString() + ":" + password.toStdString() + "@" + ip.toStdString() +
           "/profile" + profile.toStdString() + "/media.smp";
}

SummaryFields parseSummaryFields(const std::string& line) {
    static const std::regex kStatusRe(R"(status=([^ ]+))");
    static const std::regex kMessageRe("message=\\\"([^\\\"]+)\\\"");
    static const std::regex kObjectsRe(R"(objects=(\d+))");
    static const std::regex kObjectRe(R"(id=(\d+),type=([A-Za-z]+),score=(\d+)%)");

    SummaryFields fields;
    std::smatch match;
    if (std::regex_search(line, match, kStatusRe) && match.size() > 1) {
        fields.status = match[1].str();
    }
    if (std::regex_search(line, match, kMessageRe) && match.size() > 1) {
        fields.message = match[1].str();
    }
    if (std::regex_search(line, match, kObjectsRe) && match.size() > 1) {
        fields.objects = std::stoi(match[1].str());
    }

    auto begin = std::sregex_iterator(line.begin(), line.end(), kObjectRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end && fields.fragments.size() < 3; ++it) {
        const auto& object_match = *it;
        fields.fragments << QString("%1 #%2 (%3%)")
                                 .arg(QString::fromStdString(object_match[2].str()))
                                 .arg(QString::fromStdString(object_match[1].str()))
                                 .arg(QString::fromStdString(object_match[3].str()));
    }

    return fields;
}

QString parserHealthCategoryLabel(const std::string& status, const std::string& message) {
    if (message == "metadata-without-objects" || status == "no-objects") {
        return "metadata without objects";
    }
    if (message == "continuation-without-xml-start" || message == "XML start marker not found") {
        return "continuation chunk";
    }
    if (message == "truncated-object-fragment") {
        return "fragmented object payload";
    }
    if (status == "unknown-pattern" || message.find("unknown-patterns") != std::string::npos) {
        return "unknown object pattern";
    }
    if (message.rfind("recovered-continuation", 0) == 0) {
        return "recovered continuation";
    }
    if (!message.empty()) {
        return "clean object payload";
    }
    return "waiting for first payload";
}

QString formatSummaryLine(const std::string& line) {
    const SummaryFields fields = parseSummaryFields(line);
    QString summary = QString("%1 | %2 | objects=%3")
                          .arg(QString::fromStdString(fields.status))
                          .arg(fields.message.empty() ? QString("no-note") : QString::fromStdString(fields.message))
                          .arg(fields.objects);
    if (!fields.fragments.isEmpty()) {
        summary += " | " + fields.fragments.join(", ");
    }
    return summary;
}

RuntimeSnapshot captureRuntimeSnapshot(SharedAppState& state) {
    RuntimeSnapshot snapshot;
    {
        std::lock_guard<std::mutex> lock(state.frame_mutex);
        if (!state.current_frame.empty()) {
            snapshot.frame = state.current_frame.clone();
            snapshot.has_video_frame = true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(state.meta_mutex);
        const auto now = std::chrono::steady_clock::now();
        snapshot.metadata_is_fresh =
            state.has_metadata_update &&
            (std::chrono::duration_cast<std::chrono::milliseconds>(now - state.last_metadata_update).count() <= kFreshMetadataWindowMs);
        snapshot.total_raw_metadata_samples = state.total_raw_metadata_samples;
        snapshot.last_parsed_object_count = state.last_parsed_object_count;
        snapshot.total_detection_events = state.total_detection_events;
        snapshot.total_parsed_payloads = state.total_parsed_payloads;
        snapshot.last_raw_metadata_seen = state.last_raw_metadata_seen;
        snapshot.last_parse_status_text = state.last_parse_status_text;
        snapshot.overlay_reason = state.overlay_state.reason;
        snapshot.overlay_reason_since = state.overlay_state.reason_since;
        snapshot.parser_health_counts = state.parser_health_counts;
        snapshot.detections_by_type = toSortedMap(state.detections_by_type);
        snapshot.unique_id_sets_by_type = state.unique_ids_by_type;
        snapshot.unique_ids_by_type = toSortedUniqueCountMap(state.unique_ids_by_type);
        snapshot.recent_summaries = state.recent_parsed_summaries;

        if (snapshot.metadata_is_fresh) {
            snapshot.overlay_objects = state.current_objects;
        }
    }

    return snapshot;
}

QString buildOverlayStateLabel(const RuntimeSnapshot& snapshot,
                               bool metadata_enabled,
                               bool metadata_started) {
    return QString::fromStdString(
        derive_overlay_reason_for_ui(snapshot.has_video_frame,
                                     metadata_enabled,
                                     metadata_started,
                                     snapshot.total_raw_metadata_samples > 0,
                                     snapshot.metadata_is_fresh,
                                     static_cast<int>(snapshot.overlay_objects.size()),
                                     snapshot.overlay_reason));
}
ReadinessViewModel buildReadinessViewModel(const RuntimeSnapshot& snapshot,
                                           bool runtime_active,
                                           bool metadata_enabled,
                                           bool metadata_started,
                                           std::chrono::steady_clock::time_point runtime_started_at,
                                           std::chrono::steady_clock::time_point video_ready_since) {
    ReadinessViewModel model;
    if (!runtime_active) {
        model.runtime = "idle";
        model.video = "not started";
        model.metadata = metadata_enabled ? "not started" : "disabled";
        model.parser = "waiting for first payload";
        model.badge_text = "Idle";
        model.badge_background = "#3a4652";
        model.connection_summary = "Not connected.";
        return model;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool video_retry_window = runtime_started_at != std::chrono::steady_clock::time_point{} &&
        (now - runtime_started_at >= std::chrono::seconds(kVideoStartupRetryHintSeconds));
    const bool video_settled = video_ready_since != std::chrono::steady_clock::time_point{} &&
        (now - video_ready_since >= std::chrono::milliseconds(kMetadataStartupHoldMs));
    model.video = snapshot.has_video_frame ? "ready" : (video_retry_window ? "retrying" : "starting");

    if (!metadata_enabled) {
        model.metadata = "disabled";
    } else if (!snapshot.has_video_frame && !metadata_started) {
        model.metadata = "waiting for video baseline";
    } else if (snapshot.has_video_frame && !metadata_started) {
        model.metadata = video_settled ? "starting session" : "holding for video baseline";
    } else if (snapshot.total_raw_metadata_samples > 0) {
        model.metadata = snapshot.metadata_is_fresh ? "receiving" : "receiving / stale";
    } else {
        model.metadata = "waiting for first payload";
    }

    if (!snapshot.recent_summaries.empty()) {
        const SummaryFields fields = parseSummaryFields(snapshot.recent_summaries.back());
        model.parser = parserHealthCategoryLabel(fields.status, fields.message);
    } else if (snapshot.total_raw_metadata_samples > 0) {
        model.parser = "waiting for parsed summary";
    } else {
        model.parser = "waiting for first payload";
    }

    if (snapshot.has_video_frame && (!metadata_enabled || snapshot.total_raw_metadata_samples > 0)) {
        model.runtime = "live";
        model.badge_text = metadata_enabled ? "Live" : "Video ready";
        model.badge_background = metadata_enabled ? "#166534" : "#0e7490";
        model.connection_summary = metadata_enabled
            ? QString::fromStdString("Video and metadata active. Last parser status: " + snapshot.last_parse_status_text)
            : "Video is active. Metadata is disabled for this session.";
    } else if (snapshot.has_video_frame) {
        model.runtime = metadata_enabled ? "awaiting metadata" : "video ready";
        model.badge_text = metadata_enabled ? "Awaiting metadata" : "Video ready";
        model.badge_background = "#0e7490";
        model.connection_summary = metadata_enabled
            ? "Video is active. Waiting for the first metadata payload."
            : "Video is active.";
    } else if (snapshot.total_raw_metadata_samples > 0) {
        model.runtime = "metadata only";
        model.badge_text = "Metadata only";
        model.badge_background = "#b45309";
        model.connection_summary = QString::fromStdString(
            "Metadata is active, but video has not started yet. Last parser status: " + snapshot.last_parse_status_text);
    } else {
        model.runtime = video_retry_window ? "retrying video" : "starting video";
        model.badge_text = video_retry_window ? "Retrying" : "Starting video";
        model.badge_background = "#8a6d1f";
        model.connection_summary = "Video session started. Waiting for first frame before metadata startup.";
    }

    return model;
}

void initializeReadinessTable(QTableWidget* table) {
    if (!table) {
        return;
    }
    table->setRowCount(4);
    setTableRow(table, 0, "runtime", "idle");
    setTableRow(table, 1, "video", "not started");
    setTableRow(table, 2, "metadata", "not started");
    setTableRow(table, 3, "parser", "waiting for first payload");
}

void updateReadinessTable(QTableWidget* table, const ReadinessViewModel& model) {
    if (!table) {
        return;
    }
    setTableRow(table, 0, "runtime", model.runtime);
    setTableRow(table, 1, "video", model.video);
    setTableRow(table, 2, "metadata", model.metadata);
    setTableRow(table, 3, "parser", model.parser);
}

void initializeEvidenceTable(QTableWidget* table) {
    if (!table) {
        return;
    }
    table->setRowCount(6);
    setTableRow(table, 0, "raw", "not connected");
    setTableRow(table, 1, "parsed", "0");
    setTableRow(table, 2, "overlay", "0");
    setTableRow(table, 3, "overlay state", "Not connected");
    setTableRow(table, 4, "age", "n/a");
    setTableRow(table, 5, "fresh", "no");
}

void updateEvidenceTable(QTableWidget* table, const RuntimeSnapshot& snapshot, const QString& overlay_state_label) {
    if (!table) {
        return;
    }

    const auto age_ms = snapshot.total_raw_metadata_samples > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - snapshot.last_raw_metadata_seen).count()
        : -1LL;

    setTableRow(table, 0, "raw", snapshot.total_raw_metadata_samples > 0 ? "seen" : "not-seen");
    setTableRow(table, 1, "parsed", QString::number(snapshot.last_parsed_object_count));
    setTableRow(table, 2, "overlay", QString::number(static_cast<int>(snapshot.overlay_objects.size())));
    setTableRow(table, 3, "overlay state", overlay_state_label);
    setTableRow(table, 4, "age", age_ms >= 0 ? QString::number(age_ms) + " ms" : "n/a");
    setTableRow(table, 5, "fresh", snapshot.metadata_is_fresh ? "yes" : "no");
}

void initializeParserHealthTable(QTableWidget* table) {
    if (!table) {
        return;
    }
    table->setRowCount(6);
    setTableRow(table, 0, "clean object payload", "0");
    setTableRow(table, 1, "recovered continuation", "0");
    setTableRow(table, 2, "fragmented object payload", "0");
    setTableRow(table, 3, "continuation chunk", "0");
    setTableRow(table, 4, "metadata without objects", "0");
    setTableRow(table, 5, "unknown object pattern", "0");
}

void updateParserHealthTable(QTableWidget* table, const ParserHealthCounts& counts) {
    if (!table) {
        return;
    }
    setTableRow(table, 0, "clean object payload", QString::number(counts.clean_object_payloads));
    setTableRow(table, 1, "recovered continuation", QString::number(counts.recovered_continuations));
    setTableRow(table, 2, "fragmented object payload", QString::number(counts.fragmented_object_payloads));
    setTableRow(table, 3, "continuation chunk", QString::number(counts.continuation_chunks));
    setTableRow(table, 4, "metadata without objects", QString::number(counts.metadata_without_objects));
    setTableRow(table, 5, "unknown object pattern", QString::number(counts.unknown_object_patterns));
}

void updateMetricsTable(QTableWidget* table, const RuntimeSnapshot& snapshot) {
    if (!table) {
        return;
    }

    std::map<std::string, int> combined_types = snapshot.detections_by_type;
    for (const auto& entry : snapshot.unique_ids_by_type) {
        if (!combined_types.count(entry.first)) {
            combined_types[entry.first] = 0;
        }
    }

    const std::vector<std::string> vehicle_family_types = {"Vehicle", "Car", "Bus", "Truck", "Motorcycle"};
    const int vehicle_family_detections = familyDetectionCount(snapshot.detections_by_type, vehicle_family_types);
    const int vehicle_family_unique = familyUniqueCount(snapshot.unique_id_sets_by_type, vehicle_family_types);

    table->setRowCount(static_cast<int>(combined_types.size()) + 2);
    int row = 0;
    setTripleTableRow(table, row++, "Payloads / events",
                      QString::number(snapshot.total_parsed_payloads),
                      QString::number(snapshot.total_detection_events));
    setTripleTableRow(table, row++, "Vehicle family",
                      QString::number(vehicle_family_detections),
                      QString::number(vehicle_family_unique));
    for (const auto& entry : combined_types) {
        const int unique_count = snapshot.unique_ids_by_type.count(entry.first) ? snapshot.unique_ids_by_type.at(entry.first) : 0;
        QString label = QString::fromStdString(entry.first);
        if (entry.first == "Vehicle") {
            label += " (general)";
        }
        setTripleTableRow(table,
                          row++,
                          label,
                          QString::number(entry.second),
                          QString::number(unique_count));
    }
}

void updateRecentMetadataList(QListWidget* list, const std::vector<std::string>& recent_summaries) {
    if (!list) {
        return;
    }

    list->clear();
    if (recent_summaries.empty()) {
        list->addItem("No parsed metadata yet.");
        return;
    }

    for (const auto& line : recent_summaries) {
        list->addItem(formatSummaryLine(line));
    }
}

void drawOverlay(cv::Mat& frame, const std::vector<DetectedObject>& objects) {
    const int fw = frame.cols;
    const int fh = frame.rows;

    for (const auto& obj : objects) {
        auto clampI = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
        int x1 = clampI(static_cast<int>(obj.left), 0, fw - 1);
        int y1 = clampI(static_cast<int>(obj.top), 0, fh - 1);
        int x2 = clampI(static_cast<int>(obj.right), 0, fw - 1);
        int y2 = clampI(static_cast<int>(obj.bottom), 0, fh - 1);

        cv::rectangle(frame, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);
        const std::string label =
            obj.type + " #" + std::to_string(obj.id) + " " + std::to_string(static_cast<int>(obj.likelihood * 100)) + "%";
        int baseline = 0;
        const double font_scale = 0.58;
        const int thickness = 1;
        const cv::Size text_size =
            cv::getTextSize(label, cv::FONT_HERSHEY_DUPLEX, font_scale, thickness, &baseline);

        int label_x = x1;
        int label_y = (y1 - 5 > text_size.height + 5) ? (y1 - 5) : (text_size.height + baseline + 6);
        const int max_label_x = std::max(0, fw - text_size.width - 8);
        const int min_label_y = text_size.height + baseline + 6;
        const int max_label_y = std::max(min_label_y, fh - 2);
        label_x = clampI(label_x, 0, max_label_x);
        label_y = clampI(label_y, min_label_y, max_label_y);

        cv::rectangle(frame,
                      cv::Point(label_x, label_y - text_size.height - baseline - 6),
                      cv::Point(label_x + text_size.width + 8, label_y + baseline + 2),
                      cv::Scalar(230, 255, 230),
                      cv::FILLED);
        cv::putText(frame,
                    label,
                    cv::Point(label_x + 4, label_y - 2),
                    cv::FONT_HERSHEY_DUPLEX,
                    font_scale,
                    cv::Scalar(18, 28, 18),
                    thickness);
    }
}

void renderFrame(QLabel* target, const RuntimeSnapshot& snapshot) {
    if (!target) {
        return;
    }

    if (!snapshot.has_video_frame) {
        target->setPixmap(QPixmap());
        target->setText("Waiting for video...");
        return;
    }

    cv::Mat frame_copy = snapshot.frame.clone();
    if (!snapshot.overlay_objects.empty()) {
        drawOverlay(frame_copy, snapshot.overlay_objects);
    }

    cv::cvtColor(frame_copy, frame_copy, cv::COLOR_BGR2RGB);
    QImage image(frame_copy.data, frame_copy.cols, frame_copy.rows, static_cast<int>(frame_copy.step), QImage::Format_RGB888);
    const QPixmap pixmap = QPixmap::fromImage(image.copy());
    target->setPixmap(pixmap.scaled(target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    target->setText(QString());
}
}

QtShellWindow::QtShellWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("CV++ Qt Verification Shell");
    resize(1480, 920);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(12);

    auto* title = new QLabel("CV++ Qt Verification Shell");
    title->setStyleSheet("font-size: 24px; font-weight: 600; color: #e9f0f6;");

    auto* subtitle = new QLabel("Qt verification is now the primary operator surface. This slice focuses on evidence readability and session clarity.");
    subtitle->setStyleSheet("font-size: 13px; color: #8ea0b3;");

    root->addWidget(title);
    root->addWidget(subtitle);
    root->addWidget(buildConnectionPanel());
    root->addWidget(buildVerificationPanel(), 1);

    setCentralWidget(central);
    setStyleSheet(
        "QMainWindow, QWidget { background-color: #11161b; color: #d9e1e8; }"
        "QGroupBox { border: 1px solid #273341; border-radius: 8px; margin-top: 10px; font-weight: 600; color: #e6edf3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QLineEdit, QComboBox, QListWidget, QTableWidget { background-color: #182029; color: #edf3f8; border: 1px solid #2f3d4b; border-radius: 6px; }"
        "QLineEdit, QComboBox { padding: 6px 8px; }"
        "QPushButton { background-color: #2b6cb0; color: white; border: none; border-radius: 6px; padding: 8px 14px; font-weight: 600; }"
        "QPushButton:disabled { background-color: #44515c; color: #a6b0b9; }"
        "QHeaderView::section { background-color: #212b36; color: #dce6ee; border: none; padding: 6px; font-weight: 600; }"
        "QTableWidget { gridline-color: #263240; }"
        "QListWidget::item { padding: 4px 6px; }");

    poll_timer_ = new QTimer(this);
    connect(poll_timer_, &QTimer::timeout, this, [this]() { updateRuntime(); });

    gst_init(nullptr, nullptr);
    loadDefaultsFromConfig();
}

QtShellWindow::~QtShellWindow() {
    stopRuntime();
}

void QtShellWindow::closeEvent(QCloseEvent* event) {
    stopRuntime();
    QMainWindow::closeEvent(event);
}

QWidget* QtShellWindow::buildConnectionPanel() {
    auto* panel = new QWidget(this);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* controls_box = new QGroupBox("Session Controls", panel);
    controls_box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* controls_layout = new QVBoxLayout(controls_box);
    controls_layout->setContentsMargins(10, 10, 10, 10);
    controls_layout->setSpacing(8);

    auto* top_row = new QHBoxLayout();
    top_row->setSpacing(8);

    auto addField = [&](const QString& label_text, QWidget* widget, int width) {
        auto* column = new QVBoxLayout();
        column->setSpacing(3);
        auto* label = new QLabel(label_text, controls_box);
        label->setStyleSheet("font-size: 11px; color: #8ea0b3; font-weight: 600;");
        widget->setParent(controls_box);
        widget->setMinimumWidth(width);
        widget->setMaximumWidth(width);
        column->addWidget(label);
        column->addWidget(widget);
        top_row->addLayout(column);
    };

    ip_edit_ = new QLineEdit(controls_box);
    username_edit_ = new QLineEdit(controls_box);
    password_edit_ = new QLineEdit(controls_box);
    password_edit_->setEchoMode(QLineEdit::Password);
    profile_combo_ = new QComboBox(controls_box);
    profile_combo_->addItems({"2", "4", "10"});
    profile_combo_->setCurrentText("2");

    addField("IP", ip_edit_, 170);
    addField("User", username_edit_, 120);
    addField("Password", password_edit_, 120);
    addField("Profile", profile_combo_, 82);

    top_row->addSpacing(4);

    status_badge_ = new QLabel("Idle", controls_box);
    status_badge_->setAlignment(Qt::AlignCenter);
    status_badge_->setMinimumWidth(120);
    status_badge_->setMaximumWidth(150);

    connect_button_ = new QPushButton("Connect", controls_box);
    connect_button_->setMinimumWidth(88);
    connect_button_->setMaximumWidth(96);
    disconnect_button_ = new QPushButton("Disconnect", controls_box);
    disconnect_button_->setEnabled(false);
    disconnect_button_->setMinimumWidth(96);
    disconnect_button_->setMaximumWidth(104);

    auto* status_column = new QVBoxLayout();
    status_column->setSpacing(3);
    auto* status_label = new QLabel("State", controls_box);
    status_label->setStyleSheet("font-size: 11px; color: #8ea0b3; font-weight: 600;");
    status_column->addWidget(status_label);
    status_column->addWidget(status_badge_);
    top_row->addLayout(status_column);

    auto* button_column = new QVBoxLayout();
    button_column->setSpacing(6);
    button_column->addWidget(connect_button_);
    button_column->addWidget(disconnect_button_);
    top_row->addLayout(button_column);
    top_row->addStretch(1);

    connection_summary_ = new QLabel("Not connected.", controls_box);
    connection_summary_->setWordWrap(true);
    connection_summary_->setMinimumHeight(48);
    connection_summary_->setStyleSheet("color: #c7d1da; background-color: #182029; border: 1px solid #2f3d4b; border-radius: 6px; padding: 8px;");

    connect(connect_button_, &QPushButton::clicked, this, [this]() { startRuntime(); });
    connect(disconnect_button_, &QPushButton::clicked, this, [this]() { stopRuntime(); });

    controls_layout->addLayout(top_row);
    controls_layout->addWidget(connection_summary_);

    layout->addWidget(controls_box, 3);
    layout->addWidget(buildOperatorStatePanel(panel), 2);
    setStatusBadge("Idle", "#3a4652", "#f0f4f8");
    return panel;
}

QWidget* QtShellWindow::buildOperatorStatePanel(QWidget* parent) {
    auto* operator_state_box = new QGroupBox("Operator State", parent);
    operator_state_box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    auto* operator_state_layout = new QHBoxLayout(operator_state_box);
    operator_state_layout->setContentsMargins(10, 10, 10, 10);
    operator_state_layout->setSpacing(8);

    readiness_table_ = new QTableWidget(4, 2, operator_state_box);
    configureTwoColumnTable(readiness_table_, "Stage", "State");
    initializeReadinessTable(readiness_table_);
    setCompactTableHeight(readiness_table_, 4);
    operator_state_layout->addWidget(readiness_table_, 1);

    parser_health_table_ = new QTableWidget(6, 2, operator_state_box);
    configureTwoColumnTable(parser_health_table_, "Parser Health", "Count");
    initializeParserHealthTable(parser_health_table_);
    setCompactTableHeight(parser_health_table_, 6);
    operator_state_layout->addWidget(parser_health_table_, 1);

    return operator_state_box;
}

QWidget* QtShellWindow::buildVerificationPanel() {
    auto* splitter = new QSplitter(Qt::Horizontal);

    auto* video_box = new QGroupBox("Live Verification View");
    auto* video_layout = new QVBoxLayout(video_box);
    stream_placeholder_ = new QLabel("Not connected yet.");
    stream_placeholder_->setAlignment(Qt::AlignCenter);
    stream_placeholder_->setMinimumSize(900, 640);
    stream_placeholder_->setStyleSheet(
        "background: #11161b;"
        "border: 1px solid #273341;"
        "border-radius: 8px;"
        "font-size: 20px;"
        "color: #d8dee5;");
    video_layout->addWidget(stream_placeholder_, 1);

    auto* right_panel = new QWidget();
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(0, 0, 0, 0);
    right_layout->setSpacing(10);

    auto* summary_row = new QHBoxLayout();
    summary_row->setSpacing(10);

    auto* evidence_box = new QGroupBox("Evidence");
    auto* evidence_layout = new QVBoxLayout(evidence_box);
    evidence_layout->setContentsMargins(10, 10, 10, 10);
    evidence_table_ = new QTableWidget(6, 2, evidence_box);
    configureTwoColumnTable(evidence_table_, "Field", "Value");
    initializeEvidenceTable(evidence_table_);
    setCompactTableHeight(evidence_table_, 6);
    evidence_layout->addWidget(evidence_table_);
    summary_row->addWidget(evidence_box, 1);

    auto* metrics_box = new QGroupBox("Session Metrics");
    auto* metrics_layout = new QVBoxLayout(metrics_box);
    metrics_layout->setContentsMargins(10, 10, 10, 10);
    metrics_table_ = new QTableWidget(0, 3, metrics_box);
    configureThreeColumnTable(metrics_table_, "Type", "Detections", "Unique IDs");
    metrics_table_->setMinimumHeight(180);
    metrics_layout->addWidget(metrics_table_);
    summary_row->addWidget(metrics_box, 2);

    auto* metadata_box = new QGroupBox("Recent Metadata");
    auto* metadata_layout = new QVBoxLayout(metadata_box);
    metadata_layout->setContentsMargins(10, 10, 10, 10);
    recent_metadata_list_ = new QListWidget(metadata_box);
    recent_metadata_list_->addItem("No parsed metadata yet.");
    recent_metadata_list_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    recent_metadata_list_->setMinimumHeight(170);
    recent_metadata_list_->setMaximumHeight(240);
    metadata_layout->addWidget(recent_metadata_list_);

    right_layout->addLayout(summary_row);
    right_layout->addWidget(metadata_box);
    right_layout->addStretch(1);

    splitter->addWidget(video_box);
    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({960, 500});

    return splitter;
}

void QtShellWindow::loadDefaultsFromConfig() {
    std::string error_message;
    AppConfig loaded;
    if (!load_config("config.toml", loaded, error_message)) {
        ip_edit_->setText("192.168.4.225");
        username_edit_->setText("admin");
        profile_combo_->setCurrentText("2");
        return;
    }

    config_ = loaded;

    QString ip;
    QString user;
    QString password;
    QString profile = "2";
    if (parseRtspDefaults(config_.rtsp_url, ip, user, password, profile)) {
        ip_edit_->setText(ip);
        username_edit_->setText(user);
        password_edit_->setText(password);
    } else {
        ip_edit_->setText("192.168.4.225");
        username_edit_->setText("admin");
    }
    profile_combo_->setCurrentText("2");
}

void QtShellWindow::startRuntime() {
    stopRuntime();

    if (ip_edit_->text().trimmed().isEmpty() ||
        username_edit_->text().trimmed().isEmpty() ||
        password_edit_->text().isEmpty()) {
        QMessageBox::warning(this, "CV++", "IP address, username, and password are required.");
        return;
    }

    std::string error_message;
    AppConfig loaded_defaults;
    if (load_config("config.toml", loaded_defaults, error_message)) {
        config_ = loaded_defaults;
    } else {
        config_ = AppConfig{};
    }

    config_.rtsp_url = buildRtspUrl(ip_edit_->text().trimmed(),
                                    username_edit_->text().trimmed(),
                                    password_edit_->text(),
                                    profile_combo_->currentText());

    logger_ = std::make_unique<SessionLogger>();
    if (!logger_->initialize(config_, error_message)) {
        QMessageBox::critical(this, "CV++", QString::fromStdString("Failed to initialize logging: " + error_message));
        logger_.reset();
        return;
    }

    shared_state_ = std::make_unique<SharedAppState>();
    video_session_ = std::make_unique<VideoRtspSession>(config_, *logger_, *shared_state_);
    metadata_session_ = std::make_unique<MetadataRtspSession>(config_, *logger_, *shared_state_);

    if (!video_session_->start()) {
        QMessageBox::critical(this, "CV++", "Failed to start video session.");
        stopRuntime();
        return;
    }

    metadata_started_ = false;
    runtime_active_ = true;
    runtime_started_at_ = std::chrono::steady_clock::now();
    video_ready_since_ = std::chrono::steady_clock::time_point{};
    connect_button_->setText("Reconnect");
    setRuntimeUiEnabled(false);
    disconnect_button_->setEnabled(true);
    setStatusBadge("Starting video", "#8a6d1f");
    connection_summary_->setText("Video session started. Waiting for first frame before metadata startup.");
    initializeReadinessTable(readiness_table_);
    initializeEvidenceTable(evidence_table_);
    initializeParserHealthTable(parser_health_table_);
    metrics_table_->setRowCount(0);
    recent_metadata_list_->clear();
    recent_metadata_list_->addItem("Waiting for parsed metadata...");
    poll_timer_->start(33);
}

void QtShellWindow::stopRuntime() {
    if (poll_timer_) {
        poll_timer_->stop();
    }

    if (metadata_started_ && metadata_session_) {
        metadata_session_->stop();
    }
    if (video_session_) {
        video_session_->stop();
    }

    if (logger_ && shared_state_) {
        logger_->finalize_session(*shared_state_);
    }

    metadata_started_ = false;
    runtime_active_ = false;
    runtime_started_at_ = std::chrono::steady_clock::time_point{};
    video_ready_since_ = std::chrono::steady_clock::time_point{};
    metadata_session_.reset();
    video_session_.reset();
    shared_state_.reset();
    logger_.reset();

    if (stream_placeholder_) {
        stream_placeholder_->setText("Not connected yet.");
        stream_placeholder_->setPixmap(QPixmap());
    }
    if (connection_summary_) {
        connection_summary_->setText("Not connected.");
    }
    if (status_badge_) {
        setStatusBadge("Idle", "#3a4652", "#f0f4f8");
    }
    if (connect_button_) {
        connect_button_->setText("Connect");
    }
    if (disconnect_button_) {
        disconnect_button_->setEnabled(false);
    }
    setRuntimeUiEnabled(true);
    initializeReadinessTable(readiness_table_);
    initializeEvidenceTable(evidence_table_);
    initializeParserHealthTable(parser_health_table_);
    if (metrics_table_) {
        metrics_table_->setRowCount(0);
    }
    if (recent_metadata_list_) {
        recent_metadata_list_->clear();
        recent_metadata_list_->addItem("No parsed metadata yet.");
    }
}

void QtShellWindow::updateRuntime() {
    if (!runtime_active_ || !video_session_ || !shared_state_) {
        return;
    }

    video_session_->poll_bus_once();

    if (config_.enable_metadata && metadata_session_) {
        if (!metadata_started_) {
            bool video_ready = false;
            {
                std::lock_guard<std::mutex> lock(shared_state_->frame_mutex);
                video_ready = !shared_state_->current_frame.empty();
            }

            const auto now = std::chrono::steady_clock::now();
            if (video_ready) {
                if (video_ready_since_ == std::chrono::steady_clock::time_point{}) {
                    video_ready_since_ = now;
                }
            } else {
                video_ready_since_ = std::chrono::steady_clock::time_point{};
            }

            const bool startup_wait_expired = now - runtime_started_at_ > std::chrono::seconds(15);
            const bool video_settled = video_ready_since_ != std::chrono::steady_clock::time_point{} &&
                (now - video_ready_since_ >= std::chrono::milliseconds(kMetadataStartupHoldMs));

            if ((video_settled || startup_wait_expired) && metadata_session_->start()) {
                metadata_started_ = true;
            }
        }

        if (metadata_started_) {
            metadata_session_->poll_bus_once();
        }
    }

    refreshUiFromState();
}

void QtShellWindow::refreshUiFromState() {
    if (!shared_state_) {
        return;
    }

    const RuntimeSnapshot snapshot = captureRuntimeSnapshot(*shared_state_);
    const ReadinessViewModel readiness = buildReadinessViewModel(snapshot,
                                                                 runtime_active_,
                                                                 config_.enable_metadata,
                                                                 metadata_started_,
                                                                 runtime_started_at_,
                                                                 video_ready_since_);
    const QString overlay_state_label = buildOverlayStateLabel(snapshot, config_.enable_metadata, metadata_started_);

    renderFrame(stream_placeholder_, snapshot);
    updateReadinessTable(readiness_table_, readiness);
    updateEvidenceTable(evidence_table_, snapshot, overlay_state_label);
    updateParserHealthTable(parser_health_table_, snapshot.parser_health_counts);
    updateMetricsTable(metrics_table_, snapshot);
    updateRecentMetadataList(recent_metadata_list_, snapshot.recent_summaries);
    setStatusBadge(readiness.badge_text, readiness.badge_background, readiness.badge_foreground);
    connection_summary_->setText(readiness.connection_summary);
}

void QtShellWindow::setRuntimeUiEnabled(bool enabled) {
    if (ip_edit_) {
        ip_edit_->setEnabled(enabled);
    }
    if (username_edit_) {
        username_edit_->setEnabled(enabled);
    }
    if (password_edit_) {
        password_edit_->setEnabled(enabled);
    }
    if (profile_combo_) {
        profile_combo_->setEnabled(enabled);
    }
}

void QtShellWindow::setStatusBadge(const QString& text, const QString& background, const QString& foreground) {
    if (!status_badge_) {
        return;
    }
    status_badge_->setText(text);
    status_badge_->setStyleSheet(
        QString("background-color: %1; color: %2; border-radius: 12px; padding: 6px 12px; font-weight: 700;")
            .arg(background, foreground));
}










