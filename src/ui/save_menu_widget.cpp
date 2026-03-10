#include "save_menu_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>

namespace easyshotter {

SaveMenuWidget::SaveMenuWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    layoutItems();
}

SaveMenuWidget::~SaveMenuWidget() = default;

void SaveMenuWidget::layoutItems()
{
    m_items.clear();

    QFont font("Segoe UI", 9);
    font.setStyleHint(QFont::SansSerif);
    QFontMetrics fm(font);

    QString text1 = QString::fromUtf8("保存到桌面");
    QString text2 = QString::fromUtf8("另存为...");

    int maxWidth = qMax(fm.horizontalAdvance(text1), fm.horizontalAdvance(text2));
    int itemWidth = maxWidth + kItemPadding * 4;

    int y = kItemPadding;
    m_items.append({QRect(kItemPadding, y, itemWidth, kItemHeight), text1});
    y += kItemHeight;
    m_items.append({QRect(kItemPadding, y, itemWidth, kItemHeight), text2});
    y += kItemHeight + kItemPadding;

    setFixedSize(itemWidth + kItemPadding * 2, y);
}

void SaveMenuWidget::showAt(const QPoint& screenPos)
{
    // Position below the button, ensure on screen
    QPoint pos = screenPos;
    QScreen* screen = QApplication::screenAt(pos);
    if (screen) {
        QRect sr = screen->geometry();
        if (pos.x() + width() > sr.right())
            pos.setX(sr.right() - width());
        if (pos.y() + height() > sr.bottom())
            pos.setY(screenPos.y() - height());
    }
    move(pos);
    show();
    raise();
    setFocus();
}

void SaveMenuWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    QRectF bgRect(0, 0, width(), height());
    painter.setPen(QPen(QColor(64, 64, 64, 200), 1));
    painter.setBrush(QColor(48, 48, 48, 240));
    painter.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);

    // Items
    QFont font("Segoe UI", 9);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);

    for (int i = 0; i < m_items.size(); i++) {
        const auto& item = m_items[i];

        if (i == m_pressedIndex) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 50));
            painter.drawRoundedRect(item.rect, 3, 3);
        } else if (i == m_hoveredIndex) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 30));
            painter.drawRoundedRect(item.rect, 3, 3);
        }

        painter.setPen(QColor(220, 220, 220));
        painter.drawText(item.rect.adjusted(kItemPadding, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, item.text);
    }
}

int SaveMenuWidget::itemAtPos(const QPoint& pos) const
{
    for (int i = 0; i < m_items.size(); i++) {
        if (m_items[i].rect.contains(pos)) return i;
    }
    return -1;
}

void SaveMenuWidget::mouseMoveEvent(QMouseEvent* event)
{
    int idx = itemAtPos(event->pos());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        setCursor(idx >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void SaveMenuWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressedIndex = itemAtPos(event->pos());
        update();
    }
}

void SaveMenuWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressedIndex >= 0) {
        int idx = itemAtPos(event->pos());
        if (idx == m_pressedIndex) {
            hide();
            if (idx == 0) emit saveToDesktop();
            else if (idx == 1) emit saveToFolder();
        }
        m_pressedIndex = -1;
        update();
    }
}

void SaveMenuWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hoveredIndex = -1;
    m_pressedIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void SaveMenuWidget::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event)
    hide();
}

} // namespace easyshotter
