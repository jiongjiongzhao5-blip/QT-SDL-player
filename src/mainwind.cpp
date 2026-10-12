#include "mainwind.h"

#include <algorithm>

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

#include "ctrlbar.h"
#include "displaywind.h"
#include "playlistwind.h"
#include "titlebar.h"

MainWind::MainWind(QWidget* parent)
    : QMainWindow(parent)
{
    mp_ = new MediaPlayer(this);

    buildUi();
    buildMenu();
    wireSignals();

    resize(1280, 800);
    setWindowTitle(QStringLiteral("zjjPlayer (Qt6 + FFmpeg + SDL3)"));
}

MainWind::~MainWind()
{
    mp_->stop();
}

void MainWind::buildUi()
{
    display_ = new DisplayWind(this);
    ctrl_    = new CtrlBar(this);

    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(display_, 1);
    layout->addWidget(ctrl_, 0);
    setCentralWidget(central);

    title_ = new TitleBar(this);
    auto* titleDock = new QDockWidget(this);
    titleDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    titleDock->setTitleBarWidget(new QWidget());
    titleDock->setWidget(title_);
    addDockWidget(Qt::TopDockWidgetArea, titleDock);

    playlist_ = new PlayListWind(this);
    playlistDock_ = new QDockWidget(QStringLiteral("播放列表"), this);
    playlistDock_->setWidget(playlist_);
    playlistDock_->setMinimumWidth(180);
    addDockWidget(Qt::LeftDockWidgetArea, playlistDock_);

    statusBar()->showMessage(QStringLiteral("就绪，可通过“文件 -> 打开”选择视频"));
}

void MainWind::buildMenu()
{
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    fileMenu->addAction(QStringLiteral("打开..."), QKeySequence::Open,
                        this, &MainWind::openFile);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), QKeySequence::Quit,
                        this, &QWidget::close);

    auto* viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    viewMenu->addAction(playlistDock_->toggleViewAction());
}

void MainWind::wireSignals()
{
    connect(ctrl_, &CtrlBar::playOrPauseClicked, this, &MainWind::onPlayOrPause);
    connect(ctrl_, &CtrlBar::stopClicked,        this, &MainWind::onStop);
    connect(ctrl_, &CtrlBar::seekRequested,      this, &MainWind::onSeek);
    connect(ctrl_, &CtrlBar::seekOffsetRequested, this, &MainWind::onSeekOffset);
    connect(ctrl_, &CtrlBar::volumeChanged,      this, &MainWind::onVolume);
    connect(ctrl_, &CtrlBar::speedChanged,       this, &MainWind::onSpeed);
    connect(ctrl_, &CtrlBar::playlistToggled,    this,
            [this] { playlistDock_->setVisible(!playlistDock_->isVisible()); });

    connect(title_, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(title_, &TitleBar::maximizeToggleRequested, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
    });
    connect(title_, &TitleBar::closeRequested, this, &QWidget::close);

    connect(mp_, &MediaPlayer::frameReady,      display_, &DisplayWind::presentFrame);

    connect(mp_, &MediaPlayer::positionChanged, this, &MainWind::onPosition);
    connect(mp_, &MediaPlayer::durationChanged, this, &MainWind::onDuration);
    connect(mp_, &MediaPlayer::prepared,        this, &MainWind::onPrepared);
    connect(mp_, &MediaPlayer::completed,       this, &MainWind::onCompleted);
    connect(mp_, &MediaPlayer::errorOccurred,   this, &MainWind::onError);
}

void MainWind::openUrl(const QString& path)
{
    if (path.isEmpty())
        return;

    mp_->stop();
    display_->clear();
    ctrl_->reset();
    lastPositionMs_ = 0;

    currentFile_ = path;
    mp_->setDataSource(path);
    mp_->play();

    const QFileInfo fi(path);
    title_->setTitle(fi.fileName());
    playlist_->addItem(fi.fileName());
    statusBar()->showMessage(path);
}

void MainWind::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开媒体文件"), QString(),
        QStringLiteral("媒体文件 (*.mp4 *.mkv *.avi *.mov *.flv *.ts *.mp3 *.aac *.wav *.flac);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    openUrl(path);
}

void MainWind::onPlayOrPause()
{
    if (currentFile_.isEmpty()) {
        openFile();
        return;
    }
    mp_->togglePlayPause();
    ctrl_->setPlaying(mp_->isPlaying());
}

void MainWind::onStop()
{
    mp_->stop();
    ctrl_->reset();
    display_->clear();
    lastPositionMs_ = 0;
}

void MainWind::onSeek(qint64 ms)
{
    mp_->seek(ms);
    lastPositionMs_ = ms;
}

void MainWind::onSeekOffset(int seconds)
{
    mp_->seek(std::max<qint64>(0, lastPositionMs_ + seconds * 1000LL));
}

void MainWind::onVolume(int percent)
{
    mp_->setVolume(percent);
}

void MainWind::onSpeed(float ratio)
{
    mp_->setSpeed(ratio);
    statusBar()->showMessage(QStringLiteral("播放倍速：%1x").arg(ratio, 0, 'f', 1), 1500);
}

void MainWind::onPrepared()
{
    ctrl_->setPlaying(true);
}

void MainWind::onCompleted()
{
    ctrl_->setPlaying(false);
    statusBar()->showMessage(QStringLiteral("播放完成"), 2000);
}

void MainWind::onError(const QString& message)
{
    ctrl_->setPlaying(false);
    QMessageBox::warning(this, QStringLiteral("播放出错"), message);
}

void MainWind::onPosition(qint64 ms)
{
    lastPositionMs_ = ms;
    ctrl_->setPositionMs(ms);
}

void MainWind::onDuration(qint64 ms)
{
    ctrl_->setDurationMs(ms);
}

void MainWind::closeEvent(QCloseEvent* event)
{
    mp_->stop();
    QMainWindow::closeEvent(event);
}
