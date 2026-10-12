#include <QApplication>
#include <QDebug>
#include <QFileInfo>

#include "mainwind.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("voice_player_qt6");
    QApplication::setOrganizationName("zjj");

    MainWind window;
    window.show();

    if (argc > 1) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        if (QFileInfo::exists(path)) {
            qDebug() << "[main] 命令行打开:" << path;
            window.openUrl(path);
        } else {
            qDebug() << "[main] 命令行参数不是有效文件，忽略:" << path;
        }
    } else {
        qDebug() << "[main] 未指定文件，可通过“文件 -> 打开”选择媒体";
    }

    return app.exec();
}
