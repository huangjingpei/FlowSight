#include <QApplication>
#include <QDir>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    // 适配高 DPI 屏幕
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);

    // 确保必要运行目录存在
    QDir().mkpath("data");
    QDir().mkpath("clips");
    QDir().mkpath("exports");
    QDir().mkpath("models");
    QDir().mkpath("configs");

    MainWindow window;
    window.show();

    return app.exec();
}
