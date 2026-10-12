#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QWidget>

class QLabel;
class QPushButton;

class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

    void setTitle(const QString& title);

signals:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

private:
    QLabel*      titleLabel_ = nullptr;
    QPushButton* minBtn_     = nullptr;
    QPushButton* maxBtn_     = nullptr;
    QPushButton* closeBtn_   = nullptr;
};

#endif
