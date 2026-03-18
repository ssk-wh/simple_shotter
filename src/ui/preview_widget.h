#pragma once

#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QPoint>

namespace simpleshotter {

class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override;

    void setScreenPixmap(const QPixmap& pixmap);
    void updatePosition(const QPoint& screenPos, const QPoint& pixmapPos);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kMagnification = 6;
    static constexpr int kCaptureRadius = 12;
    static constexpr int kWidgetWidth = 160;
    static constexpr int kWidgetHeight = 190;
    static constexpr int kMagnifierSize = 150;
    static constexpr int kColorInfoHeight = 30;
    static constexpr int kMargin = 5;

    QPixmap m_screenPixmap;
    QImage m_screenImage;  // Cached QImage to avoid per-frame conversion
    QPoint m_currentPos;
};

} // namespace simpleshotter
