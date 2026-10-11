#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "ffplayer.h"
#include "imagescaler.h"

enum class PlaybackState {
    Idle,
    Preparing,
    Playing,
    Paused,
    Completed,
    Error
};

class MediaPlayer : public QObject
{
    Q_OBJECT
public:
    explicit MediaPlayer(QObject* parent = nullptr);
    ~MediaPlayer() override;

    void setDataSource(const QString& url);
    QString dataSource() const { return source_; }

    bool isPlaying() const { return state_ == PlaybackState::Playing; }
    PlaybackState state() const { return state_; }

public slots:
    void play();
    void togglePlayPause();
    void pause();
    void resume();
    void stop();
    void seek(qint64 ms);

    void setVolume(int percent0to100);
    void setSpeed(float ratio);

signals:
    void prepared();
    void completed();
    void errorOccurred(const QString& message);
    void frameReady(const QImage& frame);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void stateChanged(PlaybackState state);

private slots:
    void onPositionTick();

private:
    void eventLoop();
    void onKernelFrame(const Frame& frame);
    void setState(PlaybackState s);
    void teardown();

    std::unique_ptr<FFPlayer> player_;
    std::thread eventThread_;
    ImageScaler scaler_;
    std::mutex  convertMtx_;
    std::vector<uint8_t> rgbCache_;

    QString source_;
    PlaybackState state_ = PlaybackState::Idle;
    QTimer* positionTimer_ = nullptr;
    int   volume_ = 100;
    float speed_  = 1.0f;
};

#endif
