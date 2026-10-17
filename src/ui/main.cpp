#include <QApplication>
#include <QDir>
#include <QFont>
#include "MainWindow.hpp"
#include "Theme.hpp"

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);

    // 设置默认高质量全局字体
    QFont default_font("Segoe UI", 10);
    default_font.setStyleHint(QFont::SansSerif);
    app.setFont(default_font);

    // 应用工业级暗色主题 QSS
    app.setStyleSheet(Theme::get_global_stylesheet());

    // 确保必要目录存在
    QDir().mkpath("data");
    QDir().mkpath("clips");
    QDir().mkpath("exports");
    QDir().mkpath("models");
    QDir().mkpath("configs");

    MainWindow window;
    window.show();

    return app.exec();
}
