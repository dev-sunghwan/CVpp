#include "qt_shell_window.h"

#include <chrono>
#include <map>
#include <regex>
#include <unordered_map>
#include <unordered_set>

#include <QCloseEvent>
#include <QComboBox>
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
#include <QFontDatabase>
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

QString simplifySummaryLine(const std::string& line) {
    std::smatch match;
    std::regex status_re(R"(status=([^ ]+))");
    std::regex objects_re(R"(objects=(\d+))");
    std::regex object_re(R"(id=(\d+),type=([A-Za-z]+),score=(\d+)%)");

    QString status = "unknown";
    QString objects = "0";
    if (std::regex_search(line, match, status_re) && match.size() > 1) {
        status = QString::fromStdString(match[1].str());
    }
    if (std::regex_search(line, match, objects_re) && match.size() > 1) {
        objects = QString::fromStdString(match[1].str());
    }

    QStringList fragments;
    auto begin = std::sregex_iterator(line.begin(), line.end(), object_re);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end && fragments.size() < 3; ++it) {
        const auto& object_match = *it;
        fragments << QString("%1 #%2 (%3%)")
                         .arg(QString::fromStdString(object_match[2].str()))
                         .arg(QString::fromStdString(object_match[1].str()))
                         .arg(QString::fromStdString(object_match[3].str()));
    }

    QString summary = QString("%1 | objects=%2").arg(status, objects);
    if (!fragments.isEmpty()) {
        summary += " | " + fragments.join(", ");
    }
    return summary;
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
    auto* box = new QGroupBox("Connection");
    auto* layout = new QHBoxLayout(box);

    auto* form_widget = new QWidget(box);
    auto* form = new QFormLayout(form_widget);
    form->setLabelAlignment(Qt::AlignRight);

    ip_edit_ = new QLineEdit(form_widget);
    username_edit_ = new QLineEdit(form_widget);
    password_edit_ = new QLineEdit(form_widget);
    password_edit_->setEchoMode(QLineEdit::Password);
    profile_combo_ = new QComboBox(form_widget);
    profile_combo_->addItems({"2", "4", "10"});
    profile_combo_->setCurrentText("2");

    form->addRow("IP Address", ip_edit_);
    form->addRow("Username", username_edit_);
    form->addRow("Password", password_edit_);
    form->addRow("Profile", profile_combo_);

    auto* action_layout = new QVBoxLayout();
    status_badge_ = new QLabel("Idle");
    status_badge_->setAlignment(Qt::AlignCenter);
    status_badge_->setMinimumWidth(180);
    connect_button_ = new QPushButton("Connect");
    disconnect_button_ = new QPushButton("Disconnect");
    disconnect_button_->setEnabled(false);
    connection_summary_ = new QLabel("Not connected.");
    connection_summary_->setWordWrap(true);
    connection_summary_->setStyleSheet("color: #c7d1da; background-color: #182029; border: 1px solid #2f3d4b; border-radius: 6px; padding: 8px;");

    connect(connect_button_, &QPushButton::clicked, this, [this]() { startRuntime(); });
    connect(disconnect_button_, &QPushButton::clicked, this, [this]() { stopRuntime(); });

    action_layout->addWidget(status_badge_);
    action_layout->addWidget(connect_button_);
    action_layout->addWidget(disconnect_button_);
    action_layout->addWidget(connection_summary_);
    action_layout->addStretch(1);

    layout->addWidget(form_widget, 0);
    layout->addLayout(action_layout, 1);
    setStatusBadge("Idle", "#3a4652", "#f0f4f8");
    return box;
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

    auto* evidence_box = new QGroupBox("Evidence");
    auto* evidence_layout = new QVBoxLayout(evidence_box);
    evidence_table_ = new QTableWidget(5, 2, evidence_box);
    configureTwoColumnTable(evidence_table_, "Field", "Value");
    setTableRow(evidence_table_, 0, "raw", "not connected");
    setTableRow(evidence_table_, 1, "parsed", "0");
    setTableRow(evidence_table_, 2, "overlay", "0");
    setTableRow(evidence_table_, 3, "age", "n/a");
    setTableRow(evidence_table_, 4, "fresh", "no");
    evidence_layout->addWidget(evidence_table_);

    auto* metrics_box = new QGroupBox("Session Metrics");
    auto* metrics_layout = new QVBoxLayout(metrics_box);
    metrics_table_ = new QTableWidget(0, 3, metrics_box);
    configureThreeColumnTable(metrics_table_, "Type", "Detections", "Unique IDs");
    metrics_layout->addWidget(metrics_table_);

    auto* metadata_box = new QGroupBox("Recent Metadata");
    auto* metadata_layout = new QVBoxLayout(metadata_box);
    recent_metadata_list_ = new QListWidget(metadata_box);
    recent_metadata_list_->addItem("No parsed metadata yet.");
    recent_metadata_list_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    metadata_layout->addWidget(recent_metadata_list_);

    right_layout->addWidget(evidence_box);
    right_layout->addWidget(metrics_box);
    right_layout->addWidget(metadata_box, 1);

    splitter->addWidget(video_box);
    splitter->addWidget(right_panel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({920, 520});

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
    if (!logger_->initialize(config_.output_root, error_message)) {
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
    connect_button_->setText("Reconnect");
    setRuntimeUiEnabled(false);
    disconnect_button_->setEnabled(true);
    setStatusBadge("Starting video", "#8a6d1f");
    connection_summary_->setText("Video session started. Waiting for first frame before metadata startup.");
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

    metadata_started_ = false;
    runtime_active_ = false;
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
    if (evidence_table_) {
        setTableRow(evidence_table_, 0, "raw", "not connected");
        setTableRow(evidence_table_, 1, "parsed", "0");
        setTableRow(evidence_table_, 2, "overlay", "0");
        setTableRow(evidence_table_, 3, "age", "n/a");
        setTableRow(evidence_table_, 4, "fresh", "no");
    }
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
                    connection_summary_->setText("Video is running. Holding metadata startup briefly to stabilize the video baseline.");
                }
            } else {
                video_ready_since_ = std::chrono::steady_clock::time_point{};
            }

            const bool startup_wait_expired = now - runtime_started_at_ > std::chrono::seconds(15);
            const bool video_settled = video_ready_since_ != std::chrono::steady_clock::time_point{} &&
                (now - video_ready_since_ >= std::chrono::milliseconds(1500));

            if ((video_settled || startup_wait_expired) && metadata_session_->start()) {
                metadata_started_ = true;
                setStatusBadge("Metadata starting", "#0e7490");
                connection_summary_->setText("Video baseline is stable. Metadata session has started and is waiting for the first payload.");
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

    cv::Mat frame_copy;
    {
        std::lock_guard<std::mutex> lock(shared_state_->frame_mutex);
        if (!shared_state_->current_frame.empty()) {
            frame_copy = shared_state_->current_frame.clone();
        }
    }

    std::vector<DetectedObject> overlay_objects;
    bool metadata_is_fresh = false;
    int total_raw_metadata_samples = 0;
    int last_parsed_object_count = 0;
    int total_detection_events = 0;
    int total_parsed_payloads = 0;
    std::chrono::steady_clock::time_point last_raw_metadata_seen{};
    std::string status_text = "No metadata parsed yet";
    std::map<std::string, int> detections_by_type;
    std::unordered_map<std::string, std::unordered_set<int>> unique_id_sets_by_type;
    std::map<std::string, int> unique_ids_by_type;
    std::vector<std::string> recent_summaries;

    {
        std::lock_guard<std::mutex> lock(shared_state_->meta_mutex);
        const auto now = std::chrono::steady_clock::now();
        metadata_is_fresh =
            shared_state_->has_metadata_update &&
            (std::chrono::duration_cast<std::chrono::milliseconds>(now - shared_state_->last_metadata_update).count() <= 1500);
        status_text = shared_state_->last_parse_status_text;
        total_raw_metadata_samples = shared_state_->total_raw_metadata_samples;
        last_parsed_object_count = shared_state_->last_parsed_object_count;
        total_detection_events = shared_state_->total_detection_events;
        total_parsed_payloads = shared_state_->total_parsed_payloads;
        last_raw_metadata_seen = shared_state_->last_raw_metadata_seen;
        detections_by_type = toSortedMap(shared_state_->detections_by_type);
        unique_id_sets_by_type = shared_state_->unique_ids_by_type;
        unique_ids_by_type = toSortedUniqueCountMap(shared_state_->unique_ids_by_type);
        recent_summaries = shared_state_->recent_parsed_summaries;

        if (metadata_is_fresh) {
            overlay_objects = shared_state_->current_objects;
        }
    }

    bool has_video_frame = !frame_copy.empty();
    if (has_video_frame) {
        if (!overlay_objects.empty()) {
            drawOverlay(frame_copy, overlay_objects);
        }
        cv::cvtColor(frame_copy, frame_copy, cv::COLOR_BGR2RGB);
        QImage image(frame_copy.data, frame_copy.cols, frame_copy.rows, static_cast<int>(frame_copy.step), QImage::Format_RGB888);
        const QPixmap pixmap = QPixmap::fromImage(image.copy());
        stream_placeholder_->setPixmap(pixmap.scaled(stream_placeholder_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        stream_placeholder_->setText(QString());
    }

    const auto age_ms = total_raw_metadata_samples > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_raw_metadata_seen).count()
        : -1LL;

    setTableRow(evidence_table_, 0, "raw", total_raw_metadata_samples > 0 ? "seen" : "not-seen");
    setTableRow(evidence_table_, 1, "parsed", QString::number(last_parsed_object_count));
    setTableRow(evidence_table_, 2, "overlay", QString::number(static_cast<int>(overlay_objects.size())));
    setTableRow(evidence_table_, 3, "age", age_ms >= 0 ? QString::number(age_ms) + " ms" : "n/a");
    setTableRow(evidence_table_, 4, "fresh", metadata_is_fresh ? "yes" : "no");

    std::map<std::string, int> combined_types = detections_by_type;
    for (const auto& entry : unique_ids_by_type) {
        if (!combined_types.count(entry.first)) {
            combined_types[entry.first] = 0;
        }
    }
    const std::vector<std::string> vehicle_family_types = {"Vehicle", "Car", "Bus", "Truck", "Motorcycle"};
    const int vehicle_family_detections = familyDetectionCount(detections_by_type, vehicle_family_types);
    const int vehicle_family_unique = familyUniqueCount(unique_id_sets_by_type, vehicle_family_types);
    metrics_table_->setRowCount(static_cast<int>(combined_types.size()) + 2);
    int row = 0;
    setTripleTableRow(metrics_table_, row++, "Payloads / events",
                      QString::number(total_parsed_payloads),
                      QString::number(total_detection_events));
    setTripleTableRow(metrics_table_, row++, "Vehicle family",
                      QString::number(vehicle_family_detections),
                      QString::number(vehicle_family_unique));
    for (const auto& entry : combined_types) {
        const int unique_count = unique_ids_by_type.count(entry.first) ? unique_ids_by_type[entry.first] : 0;
        QString label = QString::fromStdString(entry.first);
        if (entry.first == "Vehicle") {
            label += " (general)";
        }
        setTripleTableRow(metrics_table_,
                          row++,
                          label,
                          QString::number(entry.second),
                          QString::number(unique_count));
    }

    recent_metadata_list_->clear();
    if (recent_summaries.empty()) {
        recent_metadata_list_->addItem("No parsed metadata yet.");
    } else {
        for (const auto& line : recent_summaries) {
            recent_metadata_list_->addItem(simplifySummaryLine(line));
        }
    }

    if (has_video_frame && total_raw_metadata_samples > 0) {
        setStatusBadge("Live", "#166534");
        connection_summary_->setText(QString::fromStdString("Video and metadata active. Last parser status: " + status_text));
    } else if (has_video_frame) {
        setStatusBadge("Video only", "#0e7490");
        connection_summary_->setText("Video is active. Waiting for the first metadata payload.");
    } else if (total_raw_metadata_samples > 0) {
        setStatusBadge("Metadata only", "#b45309");
        connection_summary_->setText(QString::fromStdString("Metadata is active, but video has not started yet. Last parser status: " + status_text));
    } else {
        setStatusBadge("Retrying", "#8a6d1f");
        connection_summary_->setText("Video session is retrying. Metadata is not active yet.");
    }
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








