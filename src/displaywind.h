#ifndef DISPLAYWIND_H
#define DISPLAYWIND_H

#include <QImage>
#include <QMutex>
#include <QWidget>

class DisplayWind : public QWidget
{
    Q_OBJECT
public:
    explicit DisplayWind(QWidget* parent = nullptr);

public slots:
    void presentFrame(const QImage& image);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage image_;
    QMutex mutex_;

    int framesSinceLog_ = 0;
    int frameLogCount_  = 0;
};

#endif
