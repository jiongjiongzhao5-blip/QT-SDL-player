#include "titlebar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(44);

    auto* brand = new QPushButton(QStringLiteral("zjjPlayer"), this);
    brand->setFlat(true);
    brand->setFixedWidth(150);
    QFont font = brand->font();
    font.setPointSize(12);
    font.setBold(true);
    brand->setFont(font);

    titleLabel_ = new QLabel(QStringLiteral("未打开文件"), this);
    titleLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    minBtn_   = new QPushButton(QStringLiteral("—"), this);
    maxBtn_   = new QPushButton(QStringLiteral("▢"), this);
    closeBtn_ = new QPushButton(QStringLiteral("✕"), this);
    for (QPushButton* b : {minBtn_, maxBtn_, closeBtn_}) {
        b->setFixedSize(44, 44);
        b->setFlat(true);
    }

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(brand);
    layout->addWidget(titleLabel_, 1);
    layout->addWidget(minBtn_);
    layout->addWidget(maxBtn_);
    layout->addWidget(closeBtn_);

    connect(minBtn_,   &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(maxBtn_,   &QPushButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(closeBtn_, &QPushButton::clicked, this, &TitleBar::closeRequested);
}

void TitleBar::setTitle(const QString& title)
{
    titleLabel_->setText(title);
}
