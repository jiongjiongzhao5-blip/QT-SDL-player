#ifndef MAINWIND_H
#define MAINWIND_H

#include <QMainWindow>
#include <QString>

#include "mediaplayer.h"

class DisplayWind;
class CtrlBar;
class TitleBar;
class PlayListWind;
class QDockWidget;

class MainWind : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWind(QWidget* parent = nullptr);
    ~MainWind() override;

    void openUrl(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openFile();
    void onPlayOrPause();
    void onStop();
    void onSeek(qint64 ms);
    void onSeekOffset(int seconds);
    void onVolume(int percent);
    void onSpeed(float ratio);

    void onPrepared();
    void onCompleted();
    void onError(const QString& message);
    void onPosition(qint64 ms);
    void onDuration(qint64 ms);

private:
    void buildUi();
    void buildMenu();
    void wireSignals();

    MediaPlayer*   mp_       = nullptr;
    DisplayWind*   display_  = nullptr;
    CtrlBar*       ctrl_     = nullptr;
    TitleBar*      title_    = nullptr;
    PlayListWind*  playlist_ = nullptr;
    QDockWidget*   playlistDock_ = nullptr;

    QString currentFile_;
    qint64  lastPositionMs_ = 0;
};

#endif
