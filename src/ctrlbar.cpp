#include "ctrlbar.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace {
constexpr int kBtnSize = 32;

QPushButton* makeIconButton(const QIcon& icon)
{
    auto* btn = new QPushButton;
    btn->setFixedSize(kBtnSize, kBtnSize);
    btn->setIcon(icon);
    btn->setFlat(true);
    btn->setCursor(Qt::PointingHandCursor);
    return btn;
}
}

CtrlBar::CtrlBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(72);

    playSlider_ = new QSlider(Qt::Horizontal, this);
    playSlider_->setRange(0, 0);

    auto* bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(8, 0, 8, 4);
    bottomRow->setSpacing(6);

    playOrPauseBtn_ = makeIconButton(QIcon(":/icons/play.png"));
    stopBtn_        = makeIconButton(QIcon(":/icons/stop.png"));
    backwardBtn_    = new QPushButton(QStringLiteral("-10s"), this);
    forwardBtn_     = new QPushButton(QStringLiteral("+10s"), this);
    speedBtn_       = new QPushButton(QStringLiteral("倍速1.0"), this);
    speedBtn_->setFixedWidth(88);

    currentLabel_ = new QLabel("00:00", this);
    auto* slash   = new QLabel("/", this);
    totalLabel_   = new QLabel("00:00", this);
    for (QLabel* l : {currentLabel_, slash, totalLabel_})
        l->setFixedWidth(46);
    slash->setFixedWidth(12);
    slash->setAlignment(Qt::AlignCenter);

    volumeBtn_    = new QPushButton(QStringLiteral("音量"), this);
    volumeSlider_ = new QSlider(Qt::Horizontal, this);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(100);
    volumeSlider_->setFixedWidth(90);

    playListBtn_ = new QPushButton(QStringLiteral("列表"), this);

    bottomRow->addWidget(playOrPauseBtn_);
    bottomRow->addWidget(stopBtn_);
    bottomRow->addWidget(backwardBtn_);
    bottomRow->addWidget(forwardBtn_);
    bottomRow->addWidget(speedBtn_);
    bottomRow->addWidget(currentLabel_);
    bottomRow->addWidget(slash);
    bottomRow->addWidget(totalLabel_);
    bottomRow->addStretch();
    bottomRow->addWidget(volumeBtn_);
    bottomRow->addWidget(volumeSlider_);
    bottomRow->addWidget(playListBtn_);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 2, 0, 0);
    root->setSpacing(2);
    root->addWidget(playSlider_);
    root->addLayout(bottomRow);

    connect(playOrPauseBtn_, &QPushButton::clicked, this, &CtrlBar::playOrPauseClicked);
    connect(stopBtn_,        &QPushButton::clicked, this, &CtrlBar::stopClicked);

    connect(forwardBtn_,  &QPushButton::clicked, this, [this] { emit seekOffsetRequested(10); });
    connect(backwardBtn_, &QPushButton::clicked, this, [this] { emit seekOffsetRequested(-10); });

    connect(speedBtn_,    &QPushButton::clicked, this, &CtrlBar::cycleSpeed);
    connect(playListBtn_, &QPushButton::clicked, this, &CtrlBar::playlistToggled);

    connect(playSlider_, &QSlider::sliderPressed,  this, [this] { sliderDragging_ = true; });
    connect(playSlider_, &QSlider::sliderReleased, this, &CtrlBar::onSliderReleased);

    connect(volumeSlider_, &QSlider::valueChanged, this, &CtrlBar::volumeChanged);
}

QString CtrlBar::formatTime(qint64 ms)
{
    if (ms < 0) ms = 0;
    const qint64 totalSec = ms / 1000;
    const qint64 h = totalSec / 3600;
    const qint64 m = (totalSec % 3600) / 60;
    const qint64 s = totalSec % 60;
    if (h > 0)
        return QString::asprintf("%02lld:%02lld:%02lld",
                                 static_cast<long long>(h),
                                 static_cast<long long>(m),
                                 static_cast<long long>(s));
    return QString::asprintf("%02lld:%02lld",
                             static_cast<long long>(m),
                             static_cast<long long>(s));
}

void CtrlBar::setPositionMs(qint64 ms)
{
    currentLabel_->setText(formatTime(ms));

    if (!sliderDragging_ && durationMs_ > 0) {
        QSignalBlocker blocker(playSlider_);
        playSlider_->setValue(static_cast<int>(ms));
    }
}

void CtrlBar::setDurationMs(qint64 ms)
{
    durationMs_ = ms;
    totalLabel_->setText(formatTime(ms));
    playSlider_->setRange(0, static_cast<int>(ms));
}

void CtrlBar::setPlaying(bool playing)
{
    playOrPauseBtn_->setIcon(QIcon(playing ? ":/icons/pause.png" : ":/icons/play.png"));
}

void CtrlBar::reset()
{
    durationMs_ = 0;
    playSlider_->setRange(0, 0);
    currentLabel_->setText("00:00");
    totalLabel_->setText("00:00");
    setPlaying(false);
}

void CtrlBar::onSliderReleased()
{
    sliderDragging_ = false;
    emit seekRequested(playSlider_->value());
}

void CtrlBar::cycleSpeed()
{
    speedIndex_ = (speedIndex_ + 1) % 4;
    const float ratio = kSpeeds[speedIndex_];
    speedBtn_->setText(QStringLiteral("倍速%1").arg(ratio, 0, 'f', 1));
    emit speedChanged(ratio);
}
