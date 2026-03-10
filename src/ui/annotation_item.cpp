#include "annotation_item.h"
#include <QPainterPath>
#include <QtMath>

namespace easyshotter {

// ---- RectAnnotation ----

void RectAnnotation::draw(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(color, penWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect.normalized());
    painter.restore();
}

// ---- EllipseAnnotation ----

void EllipseAnnotation::draw(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, penWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(rect.normalized());
    painter.restore();
}

// ---- ArrowAnnotation ----

void ArrowAnnotation::draw(QPainter& painter) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    double dx = endPos.x() - startPos.x();
    double dy = endPos.y() - startPos.y();
    double length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0) {
        painter.restore();
        return;
    }

    // Draw line
    painter.setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(startPos, endPos);

    // Arrow head
    double angle = std::atan2(dy, dx);
    double headLen = std::min(16.0, length * 0.3);
    double headAngle = M_PI / 6.0;

    QPointF p1(endPos.x() - headLen * std::cos(angle - headAngle),
               endPos.y() - headLen * std::sin(angle - headAngle));
    QPointF p2(endPos.x() - headLen * std::cos(angle + headAngle),
               endPos.y() - headLen * std::sin(angle + headAngle));

    QPainterPath headPath;
    headPath.moveTo(endPos);
    headPath.lineTo(p1);
    headPath.lineTo(p2);
    headPath.closeSubpath();

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPath(headPath);

    painter.restore();
}

// ---- TextAnnotation ----

void TextAnnotation::draw(QPainter& painter) const
{
    if (text.isEmpty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(font);
    painter.setPen(color);

    // Draw text with shadow for readability
    painter.setPen(QColor(0, 0, 0, 120));
    painter.drawText(pos + QPoint(1, 1), text);
    painter.setPen(color);
    painter.drawText(pos, text);

    painter.restore();
}

// ---- MosaicAnnotation ----

void MosaicAnnotation::setSource(const QPixmap& pixmap)
{
    sourcePixmap = pixmap;
    m_cachedImage = pixmap.toImage();
}

void MosaicAnnotation::addPoint(const QPoint& pt)
{
    if (!points.empty()) {
        const QPoint& last = points.back();
        int dx = pt.x() - last.x();
        int dy = pt.y() - last.y();
        if (dx * dx + dy * dy < (brushRadius * brushRadius / 4))
            return;
    }
    points.push_back(pt);
}

AnnotationItem* MosaicAnnotation::clone() const
{
    auto* c = new MosaicAnnotation();
    c->points = points;
    c->brushRadius = brushRadius;
    c->blockSize = blockSize;
    c->sourcePixmap = sourcePixmap;
    c->m_cachedImage = m_cachedImage;
    return c;
}

void MosaicAnnotation::draw(QPainter& painter) const
{
    drawToPixmap(painter);
}

void MosaicAnnotation::drawToPixmap(QPainter& painter) const
{
    if (m_cachedImage.isNull() || points.empty()) return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QImage& srcImage = m_cachedImage;
    int imgW = srcImage.width();
    int imgH = srcImage.height();

    // Build a mask of affected pixels
    QRegion affectedRegion;
    for (const auto& pt : points) {
        affectedRegion += QRect(pt.x() - brushRadius, pt.y() - brushRadius,
                                brushRadius * 2, brushRadius * 2);
    }

    // Clip to image bounds
    affectedRegion &= QRect(0, 0, imgW, imgH);

    // For each block in the affected region, pixelate
    QRect bounds = affectedRegion.boundingRect();
    int bx0 = (bounds.left() / blockSize) * blockSize;
    int by0 = (bounds.top() / blockSize) * blockSize;

    for (int by = by0; by < bounds.bottom(); by += blockSize) {
        for (int bx = bx0; bx < bounds.right(); bx += blockSize) {
            QRect block(bx, by, blockSize, blockSize);
            block &= QRect(0, 0, imgW, imgH);
            if (block.isEmpty()) continue;

            if (!affectedRegion.intersects(block)) continue;

            // Average color of the block
            int r = 0, g = 0, b = 0, count = 0;
            for (int y = block.top(); y <= block.bottom(); y++) {
                for (int x = block.left(); x <= block.right(); x++) {
                    QRgb px = srcImage.pixel(x, y);
                    r += qRed(px);
                    g += qGreen(px);
                    b += qBlue(px);
                    count++;
                }
            }
            if (count > 0) {
                QColor avg(r / count, g / count, b / count);
                painter.fillRect(block, avg);
            }
        }
    }

    painter.restore();
}

// ---- AnnotationManager ----

void AnnotationManager::addItem(std::unique_ptr<AnnotationItem> item)
{
    m_items.push_back(std::move(item));
    m_redoStack.clear();
}

void AnnotationManager::undo()
{
    if (m_items.empty()) return;
    m_redoStack.push_back(std::move(m_items.back()));
    m_items.pop_back();
}

void AnnotationManager::redo()
{
    if (m_redoStack.empty()) return;
    m_items.push_back(std::move(m_redoStack.back()));
    m_redoStack.pop_back();
}

bool AnnotationManager::canUndo() const { return !m_items.empty(); }
bool AnnotationManager::canRedo() const { return !m_redoStack.empty(); }

void AnnotationManager::clear()
{
    m_items.clear();
    m_redoStack.clear();
}

void AnnotationManager::drawAll(QPainter& painter) const
{
    for (const auto& item : m_items) {
        item->draw(painter);
    }
}

} // namespace easyshotter
