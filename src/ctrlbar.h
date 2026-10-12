#ifndef CTRLBAR_H
#define CTRLBAR_H

#include <QWidget>

class QPushButton;
class QSlider;
class QLabel;

class CtrlBar : public QWidget
{
    Q_OBJECT
public:
    explicit CtrlBar(QWidget* parent = nullptr);

public slots:
    void setPositionMs(qint64 ms);
    void setDurationMs(qint64 ms);
    void setPlaying(bool playing);
    void reset();

signals:
    void playOrPauseClicked();
    void stopClicked();
    void seekRequested(qint64 ms);
    void seekOffsetRequested(int seconds);
    void volumeChanged(int percent);
    void speedChanged(float ratio);
    void playlistToggled();

private slots:
    void onSliderReleased();
    void cycleSpeed();

private:
    static QString formatTime(qint64 ms);

    QPushButton* playOrPauseBtn_ = nullptr;
    QPushButton* stopBtn_        = nullptr;
    QPushButton* backwardBtn_    = nullptr;
    QPushButton* forwardBtn_     = nullptr;
    QPushButton* speedBtn_       = nullptr;
    QPushButton* volumeBtn_      = nullptr;
    QPushButton* playListBtn_    = nullptr;
    QSlider*     playSlider_     = nullptr;
    QSlider*     volumeSlider_   = nullptr;
    QLabel*      currentLabel_   = nullptr;
    QLabel*      totalLabel_     = nullptr;

    qint64 durationMs_ = 0;
    bool   sliderDragging_ = false;
    int    speedIndex_ = 1;
    static constexpr float kSpeeds[] = {0.5f, 1.0f, 1.5f, 2.0f};
};

#endif
