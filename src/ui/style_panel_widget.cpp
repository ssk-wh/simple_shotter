#include "style_panel_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>

namespace simpleshotter {

const QColor StylePanelWidget::kPresetColors[] = {
    QColor(255, 0, 0),      // Red
    QColor(255, 140, 0),    // Orange
    QColor(255, 220, 0),    // Yellow
    QColor(0, 180, 0),      // Green
    QColor(0, 120, 215),    // Blue
    QColor(128, 0, 255),    // Purple
    QColor(255, 255, 255),  // White
    QColor(0, 0, 0),        // Black
};

StylePanelWidget::StylePanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
}

StylePanelWidget::~StylePanelWidget() = default;

void StylePanelWidget::layoutForTool(AnnotationTool tool)
{
    m_currentTool = tool;
    m_showPenWidth = (tool == AnnotationTool::Rectangle ||
                      tool == AnnotationTool::Ellipse ||
                      tool == AnnotationTool::Arrow);
    m_showMosaicSize = (tool == AnnotationTool::Mosaic);
    m_showFontSize = (tool == AnnotationTool::Text);

    m_colorRects.clear();
    m_penWidthRects.clear();
    m_mosaicSizeRects.clear();

    int y = kPadding;

    // Color section
    if (tool != AnnotationTool::Mosaic) {
        m_colorLabelY = y;
        y += kLabelHeight + 4;

        int x = kPadding;
        for (int i = 0; i < kPresetColorCount; i++) {
            m_colorRects.append(QRect(x, y, kSwatchSize, kSwatchSize));
            x += kSwatchSize + kSwatchSpacing;
        }
        y += kSwatchSize + kSectionGap;
    }

    // Pen width section
    if (m_showPenWidth) {
        m_penWidthLabelY = y;
        y += kLabelHeight + 4;

        int x = kPadding;
        for (int i = 0; i < kOptionCount; i++) {
            m_penWidthRects.append(QRect(x, y, kOptionWidth, kOptionHeight));
            x += kOptionWidth + kSwatchSpacing;
        }
        y += kOptionHeight + kPadding;
    }

    // Mosaic size section
    if (m_showMosaicSize) {
        m_mosaicSizeLabelY = y;
        y += kLabelHeight + 4;

        int x = kPadding;
        for (int i = 0; i < kOptionCount; i++) {
            m_mosaicSizeRects.append(QRect(x, y, kOptionWidth, kOptionHeight));
            x += kOptionWidth + kSwatchSpacing;
        }
        y += kOptionHeight + kPadding;
    }

    // Font size slider section
    if (m_showFontSize) {
        m_fontSizeLabelY = y;
        y += kLabelHeight + 4;

        int totalWidth = kPadding * 2 + kPresetColorCount * (kSwatchSize + kSwatchSpacing) - kSwatchSpacing;
        int altWidth = kPadding * 2 + kOptionCount * (kOptionWidth + kSwatchSpacing) - kSwatchSpacing;
        int sliderWidth = qMax(totalWidth, altWidth) - kPadding * 2;
        m_sliderTrackRect = QRect(kPadding, y + (kSliderHeight - kSliderTrackHeight) / 2,
                                   sliderWidth, kSliderTrackHeight);
        y += kSliderHeight + kPadding;
    }

    if (!m_showPenWidth && !m_showMosaicSize && !m_showFontSize && tool != AnnotationTool::Mosaic) {
        y = y - kSectionGap + kPadding;
    }

    int totalWidth = kPadding * 2 + kPresetColorCount * (kSwatchSize + kSwatchSpacing) - kSwatchSpacing;
    int altWidth = kPadding * 2 + kOptionCount * (kOptionWidth + kSwatchSpacing) - kSwatchSpacing;
    totalWidth = qMax(totalWidth, altWidth);

    setFixedSize(totalWidth, y);
}

void StylePanelWidget::showForTool(AnnotationTool tool, const QPoint& screenPos)
{
    if (tool == AnnotationTool::None) {
        hide();
        return;
    }

    layoutForTool(tool);

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
}

StylePanelWidget::HitResult StylePanelWidget::hitTest(const QPoint& pos) const
{
    for (int i = 0; i < m_colorRects.size(); i++) {
        if (m_colorRects[i].adjusted(-1, -1, 1, 1).contains(pos))
            return {HitArea::Color, i};
    }
    for (int i = 0; i < m_penWidthRects.size(); i++) {
        if (m_penWidthRects[i].contains(pos))
            return {HitArea::PenWidth, i};
    }
    for (int i = 0; i < m_mosaicSizeRects.size(); i++) {
        if (m_mosaicSizeRects[i].contains(pos))
            return {HitArea::MosaicSize, i};
    }
    if (m_showFontSize && !m_sliderTrackRect.isNull()) {
        QRect sliderHitArea = m_sliderTrackRect.adjusted(
            -kSliderThumbRadius, -kSliderThumbRadius,
            kSliderThumbRadius, kSliderThumbRadius);
        if (sliderHitArea.contains(pos))
            return {HitArea::FontSizeSlider, 0};
    }
    return {HitArea::None, -1};
}

void StylePanelWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    QRectF bgRect(0, 0, width(), height());
    painter.setPen(QPen(QColor(64, 64, 64, 200), 1));
    painter.setBrush(QColor(48, 48, 48, 240));
    painter.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);

    if (m_currentTool != AnnotationTool::Mosaic) {
        drawColorSection(painter);
    }
    if (m_showPenWidth) {
        drawPenWidthSection(painter);
    }
    if (m_showMosaicSize) {
        drawMosaicSizeSection(painter);
    }
    if (m_showFontSize) {
        drawFontSizeSection(painter);
    }
}

void StylePanelWidget::drawColorSection(QPainter& painter) const
{
    // Label
    painter.save();
    QFont font("Segoe UI", 8);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(kPadding, m_colorLabelY + kLabelHeight - 2,
                     QString::fromUtf8("颜色"));
    painter.restore();

    // Color swatches
    for (int i = 0; i < m_colorRects.size(); i++) {
        QRect r = m_colorRects[i];
        bool selected = (i == m_colorIndex);
        bool hovered = (m_hovered.area == HitArea::Color && m_hovered.index == i);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (selected) {
            painter.setPen(QPen(Qt::white, 2));
        } else if (hovered) {
            painter.setPen(QPen(QColor(180, 180, 180), 1.5));
        } else {
            painter.setPen(QPen(QColor(80, 80, 80), 1));
        }
        painter.setBrush(kPresetColors[i]);
        painter.drawRoundedRect(r, 3, 3);

        painter.restore();
    }
}

void StylePanelWidget::drawPenWidthSection(QPainter& painter) const
{
    painter.save();
    QFont font("Segoe UI", 8);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(kPadding, m_penWidthLabelY + kLabelHeight - 2,
                     QString::fromUtf8("粗细"));
    painter.restore();

    QString labels[] = {
        QString::fromUtf8("细"),
        QString::fromUtf8("中"),
        QString::fromUtf8("粗")
    };

    for (int i = 0; i < m_penWidthRects.size(); i++) {
        QRect r = m_penWidthRects[i];
        bool selected = (i == m_penWidthIndex);
        bool hovered = (m_hovered.area == HitArea::PenWidth && m_hovered.index == i);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 120, 215, 120));
            painter.drawRoundedRect(r, 3, 3);
        } else if (hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 30));
            painter.drawRoundedRect(r, 3, 3);
        }

        // Draw a line with the actual width to preview
        int lineY = r.center().y();
        painter.setPen(QPen(m_currentColor, m_penWidths[i]));
        painter.drawLine(r.left() + 6, lineY, r.right() - 6, lineY);

        painter.restore();
    }
}

void StylePanelWidget::drawMosaicSizeSection(QPainter& painter) const
{
    painter.save();
    QFont font("Segoe UI", 8);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(kPadding, m_mosaicSizeLabelY + kLabelHeight - 2,
                     QString::fromUtf8("大小"));
    painter.restore();

    QString labels[] = {
        QString::fromUtf8("小"),
        QString::fromUtf8("中"),
        QString::fromUtf8("大")
    };

    for (int i = 0; i < m_mosaicSizeRects.size(); i++) {
        QRect r = m_mosaicSizeRects[i];
        bool selected = (i == m_mosaicSizeIndex);
        bool hovered = (m_hovered.area == HitArea::MosaicSize && m_hovered.index == i);

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (selected) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 120, 215, 120));
            painter.drawRoundedRect(r, 3, 3);
        } else if (hovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(255, 255, 255, 30));
            painter.drawRoundedRect(r, 3, 3);
        }

        // Draw mosaic grid preview
        int blockSize = m_mosaicSizes[i] / 2;
        if (blockSize < 2) blockSize = 2;
        int cx = r.center().x();
        int cy = r.center().y();
        int gridW = blockSize * 3;
        int x0 = cx - gridW / 2;
        int y0 = cy - gridW / 2;
        for (int gx = 0; gx < 3; gx++) {
            for (int gy = 0; gy < 3; gy++) {
                QColor c = ((gx + gy) % 2 == 0) ? QColor(180, 180, 180) : QColor(100, 100, 100);
                painter.fillRect(x0 + gx * blockSize, y0 + gy * blockSize,
                                 blockSize, blockSize, c);
            }
        }

        painter.restore();
    }
}

int StylePanelWidget::fontSizeFromSliderPos(int x) const
{
    int left = m_sliderTrackRect.left() + kSliderThumbRadius;
    int right = m_sliderTrackRect.right() - kSliderThumbRadius;
    if (right <= left) return kFontSizeMin;
    double ratio = qBound(0.0, double(x - left) / (right - left), 1.0);
    return qRound(kFontSizeMin + ratio * (kFontSizeMax - kFontSizeMin));
}

int StylePanelWidget::sliderPosFromFontSize(int size) const
{
    int left = m_sliderTrackRect.left() + kSliderThumbRadius;
    int right = m_sliderTrackRect.right() - kSliderThumbRadius;
    double ratio = double(size - kFontSizeMin) / (kFontSizeMax - kFontSizeMin);
    return qRound(left + ratio * (right - left));
}

void StylePanelWidget::drawFontSizeSection(QPainter& painter) const
{
    // Label with current size value
    painter.save();
    QFont font("Segoe UI", 8);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    painter.setPen(QColor(160, 160, 160));
    QString label = QString::fromUtf8("字号 %1").arg(m_fontSize);
    painter.drawText(kPadding, m_fontSizeLabelY + kLabelHeight - 2, label);
    painter.restore();

    // Slider track
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(80, 80, 80));
    painter.drawRoundedRect(m_sliderTrackRect, kSliderTrackHeight / 2, kSliderTrackHeight / 2);

    // Filled portion
    int thumbX = sliderPosFromFontSize(m_fontSize);
    QRect filledRect(m_sliderTrackRect.left(), m_sliderTrackRect.top(),
                     thumbX - m_sliderTrackRect.left(), kSliderTrackHeight);
    painter.setBrush(QColor(0, 120, 215));
    painter.drawRoundedRect(filledRect, kSliderTrackHeight / 2, kSliderTrackHeight / 2);

    // Thumb
    int thumbY = m_sliderTrackRect.center().y();
    bool hovered = (m_hovered.area == HitArea::FontSizeSlider) || m_draggingSlider;
    painter.setPen(QPen(hovered ? Qt::white : QColor(180, 180, 180), 1.5));
    painter.setBrush(QColor(0, 120, 215));
    painter.drawEllipse(QPoint(thumbX, thumbY), kSliderThumbRadius, kSliderThumbRadius);

    painter.restore();
}

void StylePanelWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggingSlider) {
        int newSize = fontSizeFromSliderPos(event->pos().x());
        if (newSize != m_fontSize) {
            m_fontSize = newSize;
            emit fontSizeChanged(m_fontSize);
            update();
        }
        return;
    }

    HitResult hit = hitTest(event->pos());
    if (hit.area != m_hovered.area || hit.index != m_hovered.index) {
        m_hovered = hit;
        setCursor(hit.area != HitArea::None ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void StylePanelWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    HitResult hit = hitTest(event->pos());
    if (hit.area == HitArea::Color && hit.index >= 0 && hit.index < kPresetColorCount) {
        m_colorIndex = hit.index;
        m_currentColor = kPresetColors[hit.index];
        emit colorChanged(m_currentColor);
        update();
    } else if (hit.area == HitArea::PenWidth && hit.index >= 0 && hit.index < kOptionCount) {
        m_penWidthIndex = hit.index;
        emit penWidthChanged(m_penWidths[hit.index]);
        update();
    } else if (hit.area == HitArea::MosaicSize && hit.index >= 0 && hit.index < kOptionCount) {
        m_mosaicSizeIndex = hit.index;
        emit mosaicSizeChanged(m_mosaicSizes[hit.index]);
        update();
    } else if (hit.area == HitArea::FontSizeSlider) {
        m_draggingSlider = true;
        m_fontSize = fontSizeFromSliderPos(event->pos().x());
        emit fontSizeChanged(m_fontSize);
        update();
    }
}

void StylePanelWidget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
    m_draggingSlider = false;
}

void StylePanelWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hovered = {HitArea::None, -1};
    setCursor(Qt::ArrowCursor);
    update();
}

} // namespace simpleshotter
