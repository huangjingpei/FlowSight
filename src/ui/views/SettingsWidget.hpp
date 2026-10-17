#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>

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
    QLabel* hardware_info_label_ = nullptr;
    QPushButton* btn_save_ = nullptr;
};
