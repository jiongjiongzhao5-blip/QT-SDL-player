#ifndef PLAYLISTWIND_H
#define PLAYLISTWIND_H

#include <QWidget>

class QListWidget;

class PlayListWind : public QWidget
{
    Q_OBJECT
public:
    explicit PlayListWind(QWidget* parent = nullptr);

    void addItem(const QString& name);
    void clearItems();

private:
    QListWidget* list_ = nullptr;
};

#endif
