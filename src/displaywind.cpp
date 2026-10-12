#include "displaywind.h"

#include <QDebug>
#include <QPainter>
#include <QPalette>

DisplayWind::DisplayWind(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setMinimumSize(320, 180);
}

void DisplayWind::presentFrame(const QImage& image)
{
    {
        QMutexLocker lock(&mutex_);
        image_ = image;
    }

    if (++framesSinceLog_ >= 25) {
        framesSinceLog_ = 0;
        qDebug() << "[DisplayWind] 已显示" << ++frameLogCount_ * 25
                 << "帧左右，尺寸" << image.size();
    }

    update();
}

void DisplayWind::clear()
{
    {
        QMutexLocker lock(&mutex_);
        image_ = QImage();
    }
    update();
}

void DisplayWind::paintEvent(QPaintEvent*  )
{
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    QImage img;
    {
        QMutexLocker lock(&mutex_);
        img = image_;
    }
    if (img.isNull())
        return;

    const QImage scaled = img.scaled(size(), Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation);

    const int x = (width()  - scaled.width())  / 2;
    const int y = (height() - scaled.height()) / 2;
    painter.drawImage(x, y, scaled);
}
