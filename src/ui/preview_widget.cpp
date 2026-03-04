#include "preview_widget.h"
#include <QPainter>
#include <QApplication>
#include <QScreen>

namespace easyshotter {

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(kWidgetWidth, kWidgetHeight);
}

PreviewWidget::~PreviewWidget() = default;

void PreviewWidget::setScreenPixmap(const QPixmap& pixmap)
{
    m_screenPixmap = pixmap;
}

void PreviewWidget::updatePosition(const QPoint& screenPos)
{
    m_currentPos = screenPos;

    // Position the preview widget near the cursor, offset to lower-right
    const int offsetX = 20;
    const int offsetY = 20;
    QPoint widgetPos(screenPos.x() + offsetX, screenPos.y() + offsetY);

    // Keep within screen bounds
    QScreen* screen = QApplication::screenAt(screenPos);
    if (screen) {
        QRect screenRect = screen->geometry();
        if (widgetPos.x() + width() > screenRect.right()) {
            widgetPos.setX(screenPos.x() - offsetX - width());
        }
        if (widgetPos.y() + height() > screenRect.bottom()) {
            widgetPos.setY(screenPos.y() - offsetY - height());
        }
    }

    move(widgetPos);
    update();
}

void PreviewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    if (m_screenPixmap.isNull()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background rounded rect
    QRectF bgRect(0, 0, kWidgetWidth, kWidgetHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(32, 32, 32, 230));
    painter.drawRoundedRect(bgRect, 6, 6);

    // Magnifier area
    int magX = (kWidgetWidth - kMagnifierSize) / 2;
    int magY = kMargin;
    QRect magRect(magX, magY, kMagnifierSize, kMagnifierSize);

    // Extract the region around cursor from screen pixmap
    QImage screenImage = m_screenPixmap.toImage();
    int srcX = m_currentPos.x() - kCaptureRadius;
    int srcY = m_currentPos.y() - kCaptureRadius;
    int srcSize = kCaptureRadius * 2 + 1;

    // Clamp source rect to image bounds
    QRect srcRect(srcX, srcY, srcSize, srcSize);
    srcRect = srcRect.intersected(screenImage.rect());

    if (!srcRect.isEmpty()) {
        QImage regionImage = screenImage.copy(srcRect);
        QPixmap magnified = QPixmap::fromImage(
            regionImage.scaled(kMagnifierSize, kMagnifierSize, Qt::KeepAspectRatio, Qt::FastTransformation));

        // Clip to magnifier rounded rect
        painter.save();
        QPainterPath clipPath;
        clipPath.addRoundedRect(magRect, 4, 4);
        painter.setClipPath(clipPath);
        painter.drawPixmap(magRect.topLeft(), magnified);
        painter.restore();

        // Draw crosshair at center of magnifier
        QPoint center = magRect.center();
        int crossLen = 8;
        painter.setPen(QPen(QColor(0, 120, 215), 1));
        painter.drawLine(center.x() - crossLen, center.y(), center.x() + crossLen, center.y());
        painter.drawLine(center.x(), center.y() - crossLen, center.x(), center.y() + crossLen);

        // Draw pixel grid lines around center (subtle)
        int pixelSize = kMagnifierSize / srcSize;
        if (pixelSize > 3) {
            painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
            for (int gx = magRect.left(); gx <= magRect.right(); gx += pixelSize) {
                painter.drawLine(gx, magRect.top(), gx, magRect.bottom());
            }
            for (int gy = magRect.top(); gy <= magRect.bottom(); gy += pixelSize) {
                painter.drawLine(magRect.left(), gy, magRect.right(), gy);
            }
        }
    }

    // Magnifier border
    painter.setPen(QPen(QColor(0, 120, 215), 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(magRect, 4, 4);

    // Color info area below magnifier
    int infoY = magY + kMagnifierSize + kMargin;

    // Get pixel color at cursor position
    QColor pixelColor;
    if (m_currentPos.x() >= 0 && m_currentPos.x() < screenImage.width() &&
        m_currentPos.y() >= 0 && m_currentPos.y() < screenImage.height()) {
        pixelColor = screenImage.pixelColor(m_currentPos);
    }

    // Color swatch
    int swatchSize = 16;
    QRect swatchRect(magX, infoY + (kColorInfoHeight - swatchSize) / 2, swatchSize, swatchSize);
    painter.setPen(QPen(QColor(128, 128, 128), 1));
    painter.setBrush(pixelColor);
    painter.drawRect(swatchRect);

    // RGB text
    QFont font("Consolas", 9);
    font.setStyleHint(QFont::Monospace);
    painter.setFont(font);
    painter.setPen(QColor(220, 220, 220));
    QString rgbText = QString("RGB(%1, %2, %3)")
                          .arg(pixelColor.red())
                          .arg(pixelColor.green())
                          .arg(pixelColor.blue());
    QRect textRect(swatchRect.right() + 6, infoY, kMagnifierSize - swatchSize - 6,
                   kColorInfoHeight);
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, rgbText);

    // Position text
    painter.setPen(QColor(160, 160, 160));
    QFont smallFont("Consolas", 8);
    smallFont.setStyleHint(QFont::Monospace);
    painter.setFont(smallFont);
    QString posText = QString("(%1, %2)").arg(m_currentPos.x()).arg(m_currentPos.y());
    QRect posRect(magX, infoY + kColorInfoHeight - 2, kMagnifierSize, 14);
    painter.drawText(posRect, Qt::AlignVCenter | Qt::AlignLeft, posText);
}

} // namespace easyshotter
