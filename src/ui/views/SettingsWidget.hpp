#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>

class SettingsWidget : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWidget(QWidget* parent = nullptr);

signals:
    void video_source_changed(const QString& source_uri);
    void settings_applied(int surge_threshold, bool surge_enabled, bool surge_beep);
    void data_cleared(bool reset_rois);

private slots:
    void on_btn_save();
    void on_btn_browse();
    void on_btn_clear_data();

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

    // 云端中台手机号密码登录与点位绑定
    QLineEdit* cloud_url_edit_ = nullptr;
    QLineEdit* cloud_client_name_edit_ = nullptr;

    // 双密码体系：边缘设备专属接入认证 (密码 2)
    QLineEdit* cloud_point_code_edit_ = nullptr;
    QLineEdit* cloud_device_pwd_edit_ = nullptr;
    QPushButton* btn_device_auth_ = nullptr;

    QLineEdit* cloud_phone_edit_ = nullptr;
    QLineEdit* cloud_password_edit_ = nullptr;
    QPushButton* btn_cloud_login_ = nullptr;
    QComboBox* cloud_point_combo_ = nullptr;
    QPushButton* btn_bind_point_ = nullptr;
    QPushButton* btn_sync_config_ = nullptr;
    QPushButton* btn_test_report_ = nullptr;
    QLabel* cloud_status_info_label_ = nullptr;

    QLabel* hardware_info_label_ = nullptr;
    QPushButton* btn_save_ = nullptr;

    // 系统数据彻底清理与系统维护
    QCheckBox* reset_rois_chk_ = nullptr;
    QPushButton* btn_clear_data_ = nullptr;
};
