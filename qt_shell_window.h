#pragma once

#include <chrono>
#include <memory>

#include <QMainWindow>

#include "app_config.h"

class QListWidget;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTimer;
class QWidget;

class MetadataRtspSession;
class SessionLogger;
struct SharedAppState;
class VideoRtspSession;

class QtShellWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit QtShellWindow(QWidget* parent = nullptr);
    ~QtShellWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* buildConnectionPanel();
    QWidget* buildVerificationPanel();

    void loadDefaultsFromConfig();
    void startRuntime();
    void stopRuntime();
    void updateRuntime();
    void refreshUiFromState();

    QLineEdit* ip_edit_ = nullptr;
    QLineEdit* username_edit_ = nullptr;
    QLineEdit* password_edit_ = nullptr;
    QComboBox* profile_combo_ = nullptr;
    QPushButton* connect_button_ = nullptr;

    QLabel* stream_placeholder_ = nullptr;
    QLabel* connection_summary_ = nullptr;
    QTableWidget* evidence_table_ = nullptr;
    QTableWidget* metrics_table_ = nullptr;
    QListWidget* recent_metadata_list_ = nullptr;
    QTimer* poll_timer_ = nullptr;

    AppConfig config_;
    std::unique_ptr<SessionLogger> logger_;
    std::unique_ptr<SharedAppState> shared_state_;
    std::unique_ptr<VideoRtspSession> video_session_;
    std::unique_ptr<MetadataRtspSession> metadata_session_;
    bool metadata_started_ = false;
    bool runtime_active_ = false;
    std::chrono::steady_clock::time_point runtime_started_at_{};
};
