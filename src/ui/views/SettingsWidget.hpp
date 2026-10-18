#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);

signals:
    void video_source_changed(const QString& source_uri);

private slots:
    void on_btn_save();
    void on_btn_browse();

private:
    void setup_ui();

    QLineEdit* video_input_edit_ = nullptr;
    QPushButton* btn_browse_ = nullptr;
    QSpinBox* retention_spin_ = nullptr;

    QCheckBox* surge_enabled_chk_ = nullptr;
    QSpinBox* surge_threshold_spin_ = nullptr;
    QCheckBox* surge_beep_chk_ = nullptr;

    // Webhook 云端推送 UI 控件
    QCheckBox* webhook_enabled_chk_ = nullptr;
    QLineEdit* webhook_url_edit_ = nullptr;
    QLineEdit* webhook_token_edit_ = nullptr;
    QSpinBox* webhook_timeout_spin_ = nullptr;
    QPushButton* btn_test_ping_ = nullptr;
    QLabel* webhook_status_label_ = nullptr;

    QLabel* hardware_info_label_ = nullptr;
    QPushButton* btn_save_ = nullptr;
};
