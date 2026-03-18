#pragma once

#include <QWidget>
#include <QColor>
#include <QVector>
#include <QRect>
#include "annotation_item.h"

namespace simpleshotter {

class StylePanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit StylePanelWidget(QWidget* parent = nullptr);
    ~StylePanelWidget() override;

    void showForTool(AnnotationTool tool, const QPoint& screenPos);

    QColor currentColor() const { return m_currentColor; }
    int currentPenWidth() const { return m_penWidths[m_penWidthIndex]; }
    int currentMosaicSize() const { return m_mosaicSizes[m_mosaicSizeIndex]; }

signals:
    void colorChanged(const QColor& color);
    void penWidthChanged(int width);
    void mosaicSizeChanged(int size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class HitArea { None, Color, PenWidth, MosaicSize };
    struct HitResult {
        HitArea area = HitArea::None;
        int index = -1;
    };

    void layoutForTool(AnnotationTool tool);
    HitResult hitTest(const QPoint& pos) const;
    void drawColorSection(QPainter& painter) const;
    void drawPenWidthSection(QPainter& painter) const;
    void drawMosaicSizeSection(QPainter& painter) const;

    static constexpr int kSwatchSize = 20;
    static constexpr int kSwatchSpacing = 4;
    static constexpr int kPadding = 8;
    static constexpr int kSectionGap = 10;
    static constexpr int kCornerRadius = 5;
    static constexpr int kLabelHeight = 14;
    static constexpr int kOptionHeight = 24;
    static constexpr int kOptionWidth = 40;

    static const QColor kPresetColors[];
    static constexpr int kPresetColorCount = 8;

    int m_penWidths[3] = {2, 4, 6};
    int m_mosaicSizes[3] = {6, 10, 16};

    QColor m_currentColor{255, 0, 0};
    int m_colorIndex = 0;
    int m_penWidthIndex = 0;
    int m_mosaicSizeIndex = 1;

    AnnotationTool m_currentTool = AnnotationTool::None;
    bool m_showPenWidth = false;
    bool m_showMosaicSize = false;

    QVector<QRect> m_colorRects;
    QVector<QRect> m_penWidthRects;
    QVector<QRect> m_mosaicSizeRects;
    int m_colorLabelY = 0;
    int m_penWidthLabelY = 0;
    int m_mosaicSizeLabelY = 0;

    HitResult m_hovered;
};

} // namespace simpleshotter
