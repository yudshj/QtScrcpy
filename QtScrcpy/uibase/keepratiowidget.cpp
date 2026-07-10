#include <QResizeEvent>
#include <cmath>

#include "keepratiowidget.h"

KeepRatioWidget::KeepRatioWidget(QWidget *parent) : QWidget(parent) {}

KeepRatioWidget::~KeepRatioWidget() {}

void KeepRatioWidget::setWidget(QWidget *w)
{
    if (!w) {
        return;
    }
    w->setParent(this);
    m_subWidget = w;
}

void KeepRatioWidget::setWidthHeightRatio(float widthHeightRatio)
{
    if (fabs(m_widthHeightRatio - widthHeightRatio) < 0.000001f) {
        return;
    }
    m_widthHeightRatio = widthHeightRatio;
    adjustSubWidget();
}

const QSize KeepRatioWidget::goodSize()
{
    if (!m_subWidget || m_widthHeightRatio < 0.0f) {
        return QSize();
    }
    return m_subWidget->size();
}

void KeepRatioWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustSubWidget();
}

void KeepRatioWidget::adjustSubWidget()
{
    if (!m_subWidget) {
        return;
    }

    QSize curSize = size();
    QPoint pos(0, 0);
    int width = 0;
    int height = 0;
    if (m_widthHeightRatio > 0.0f && !curSize.isEmpty()) {
        const float containerRatio = static_cast<float>(curSize.width()) / curSize.height();
        if (containerRatio > m_widthHeightRatio) {
            height = curSize.height();
            width = qRound(height * m_widthHeightRatio);
            pos.setX((curSize.width() - width) / 2);
        } else {
            width = curSize.width();
            height = qRound(width / m_widthHeightRatio);
            pos.setY((curSize.height() - height) / 2);
        }
    } else {
        // full widget
        height = curSize.height();
        width = curSize.width();
    }
    m_subWidget->setGeometry(pos.x(), pos.y(), width, height);
}
