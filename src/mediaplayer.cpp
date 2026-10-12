#include "mediaplayer.h"

#include <QDebug>

#include <algorithm>

extern "C" {
#include <libavutil/error.h>
}

MediaPlayer::MediaPlayer(QObject* parent)
    : QObject(parent)
{
    positionTimer_ = new QTimer(this);
    positionTimer_->setInterval(200);
    connect(positionTimer_, &QTimer::timeout, this, &MediaPlayer::onPositionTick);
}

MediaPlayer::~MediaPlayer()
{
    stop();
}

void MediaPlayer::setDataSource(const QString& url)
{
    source_ = url;
}

void MediaPlayer::setState(PlaybackState s)
{
    if (state_ == s)
        return;
    state_ = s;

    qDebug() << "[MediaPlayer] 状态 ->"
             << (s == PlaybackState::Idle      ? "Idle"
               : s == PlaybackState::Preparing ? "Preparing"
               : s == PlaybackState::Playing   ? "Playing"
               : s == PlaybackState::Paused    ? "Paused"
               : s == PlaybackState::Completed ? "Completed"
                                               : "Error");

    emit stateChanged(s);
}

void MediaPlayer::teardown()
{
    if (player_) {
        player_->close();
        if (eventThread_.joinable())
            eventThread_.join();
        player_.reset();
    }
    positionTimer_->stop();
}

void MediaPlayer::play()
{
    if (state_ == PlaybackState::Playing || state_ == PlaybackState::Preparing)
        return;
    if (state_ == PlaybackState::Paused) {
        resume();
        return;
    }
    if (source_.isEmpty()) {
        emit errorOccurred(tr("尚未选择要播放的文件"));
        return;
    }

    teardown();

    player_ = std::make_unique<FFPlayer>();
    player_->setFrameCallback([this](const Frame& f) { onKernelFrame(f); });
    player_->setVolume(volume_ / 100.0f);
    player_->setSpeed(speed_);
    player_->prepare(source_.toUtf8().constData());

    setState(PlaybackState::Preparing);

    eventThread_ = std::thread(&MediaPlayer::eventLoop, this);
}

void MediaPlayer::togglePlayPause()
{
    switch (state_) {
    case PlaybackState::Playing:  pause(); break;
    case PlaybackState::Paused:   resume(); break;
    default:                      play(); break;
    }
}

void MediaPlayer::pause()
{
    if (player_ && state_ == PlaybackState::Playing) {
        player_->setPaused(true);
        setState(PlaybackState::Paused);
    }
}

void MediaPlayer::resume()
{
    if (player_ && state_ == PlaybackState::Paused) {
        player_->setPaused(false);
        setState(PlaybackState::Playing);
    }
}

void MediaPlayer::stop()
{
    teardown();
    setState(PlaybackState::Idle);
}

void MediaPlayer::seek(qint64 ms)
{
    if (player_)
        player_->seekMs(static_cast<int64_t>(ms));
}

void MediaPlayer::setVolume(int percent0to100)
{
    volume_ = std::clamp(percent0to100, 0, 100);
    if (player_)
        player_->setVolume(volume_ / 100.0f);
}

void MediaPlayer::setSpeed(float ratio)
{
    speed_ = ratio;
    if (player_)
        player_->setSpeed(ratio);
}

void MediaPlayer::onPositionTick()
{
    if (!player_)
        return;
    emit positionChanged(static_cast<qint64>(player_->positionSeconds() * 1000.0));
    const int64_t total = player_->durationMs();
    if (total >= 0)
        emit durationChanged(static_cast<qint64>(total));
}

void MediaPlayer::onKernelFrame(const Frame& frame)
{
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lock(convertMtx_);
        if (scaler_.toRgb24(frame.frame.get(), rgbCache_, w, h) < 0)
            return;

        QImage image(rgbCache_.data(), w, h, w * 3, QImage::Format_RGB888);

        emit frameReady(image.copy());
    }
}

void MediaPlayer::eventLoop()
{
    Message msg;
    while (player_) {
        const int ret = player_->messages().get(msg, true);
        if (ret < 0)
            break;

        switch (msg.what) {
        case FFMsg::Prepared:
            QMetaObject::invokeMethod(this, [this] {
                setState(PlaybackState::Playing);
                emit prepared();
                positionTimer_->start();
                if (player_) {
                    const int64_t total = player_->durationMs();
                    if (total >= 0)
                        emit durationChanged(static_cast<qint64>(total));
                }
            }, Qt::QueuedConnection);
            break;

        case FFMsg::Completed:
            QMetaObject::invokeMethod(this, [this] {
                setState(PlaybackState::Completed);
                positionTimer_->stop();
                emit completed();
            }, Qt::QueuedConnection);
            break;

        case FFMsg::Error: {
            char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(msg.arg1, buf, sizeof(buf));
            const QString text = QString::fromUtf8(buf);
            QMetaObject::invokeMethod(this, [this, text] {
                setState(PlaybackState::Error);
                emit errorOccurred(text);
            }, Qt::QueuedConnection);
            break;
        }

        default:
            break;
        }
    }
}
