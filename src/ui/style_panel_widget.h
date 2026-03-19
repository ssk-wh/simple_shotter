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
    void layoutForTool(AnnotationTool tool);

    QColor currentColor() const { return m_currentColor; }
    int currentPenWidth() const { return m_penWidths[m_penWidthIndex]; }
    int currentMosaicSize() const { return m_mosaicSizes[m_mosaicSizeIndex]; }
    int currentFontSize() const { return m_fontSize; }

signals:
    void colorChanged(const QColor& color);
    void penWidthChanged(int width);
    void mosaicSizeChanged(int size);
    void fontSizeChanged(int size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class HitArea { None, Color, PenWidth, MosaicSize, FontSizeSlider };
    struct HitResult {
        HitArea area = HitArea::None;
        int index = -1;
    };

    HitResult hitTest(const QPoint& pos) const;
    void drawColorSection(QPainter& painter) const;
    void drawPenWidthSection(QPainter& painter) const;
    void drawMosaicSizeSection(QPainter& painter) const;
    void drawFontSizeSection(QPainter& painter) const;
    int fontSizeFromSliderPos(int x) const;
    int sliderPosFromFontSize(int size) const;

    static constexpr int kSwatchSize = 20;
    static constexpr int kSwatchSpacing = 4;
    static constexpr int kPadding = 8;
    static constexpr int kSectionGap = 10;
    static constexpr int kCornerRadius = 5;
    static constexpr int kLabelHeight = 14;
    static constexpr int kOptionHeight = 24;
    static constexpr int kOptionWidth = 40;
    static constexpr int kOptionCount = 4;

    static const QColor kPresetColors[];
    static constexpr int kPresetColorCount = 8;

    int m_penWidths[4] = {2, 4, 8, 14};
    int m_mosaicSizes[4] = {8, 14, 22, 32};

    QColor m_currentColor{255, 0, 0};
    int m_colorIndex = 0;
    int m_penWidthIndex = 0;
    int m_mosaicSizeIndex = 1;

    AnnotationTool m_currentTool = AnnotationTool::None;
    bool m_showPenWidth = false;
    bool m_showMosaicSize = false;
    bool m_showFontSize = false;

    QVector<QRect> m_colorRects;
    QVector<QRect> m_penWidthRects;
    QVector<QRect> m_mosaicSizeRects;
    int m_colorLabelY = 0;
    int m_penWidthLabelY = 0;
    int m_mosaicSizeLabelY = 0;

    // Font size slider
    static constexpr int kFontSizeMin = 10;
    static constexpr int kFontSizeMax = 72;
    static constexpr int kSliderHeight = 20;
    static constexpr int kSliderTrackHeight = 4;
    static constexpr int kSliderThumbRadius = 7;
    int m_fontSize = 14;
    int m_fontSizeLabelY = 0;
    QRect m_sliderTrackRect;
    bool m_draggingSlider = false;

    HitResult m_hovered;
};

} // namespace simpleshotter
