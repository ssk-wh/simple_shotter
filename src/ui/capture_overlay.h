#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QColor>

namespace easyshotter {

class PlatformApi;
class ToolbarWidget;
class PreviewWidget;

class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CaptureOverlay(PlatformApi* api, QWidget* parent = nullptr);
    ~CaptureOverlay() override;

    void startCapture();

signals:
    void captureConfirmed(const QPixmap& pixmap, const QRect& region);
    void captureCancelled();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class State {
        Idle,
        AutoDetect,
        Selecting,
        Selected,
        Moving,
        Resizing
    };

    enum class Handle {
        None,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight,
        Body
    };

    // Painting helpers
    void drawDimOverlay(QPainter& painter) const;
    void drawHighlightRegion(QPainter& painter, const QRect& rect) const;
    void drawSelectionBorder(QPainter& painter, const QRect& rect) const;
    void drawHandles(QPainter& painter, const QRect& rect) const;
    void drawSizeLabel(QPainter& painter, const QRect& rect) const;

    // Hit testing
    Handle hitTestHandle(const QPoint& pos) const;
    Qt::CursorShape cursorForHandle(Handle handle) const;
    QRect handleRect(Handle handle) const;
    QVector<QRect> allHandleRects() const;

    // Selection helpers
    QRect normalizedSelection() const;
    void confirmCapture();
    void cancelCapture();
    void updateAutoDetect(const QPoint& pos);
    void updateCursorShape(const QPoint& pos);

    static constexpr int kHandleSize = 8;
    static constexpr int kHandleHitMargin = 4;
    static constexpr int kBorderWidth = 2;
    static inline const QColor kAccentColor{0, 120, 215};

    PlatformApi* m_api;
    State m_state = State::Idle;
    QPixmap m_screenPixmap;

    // Selection
    QRect m_selectionRect;
    QRect m_highlightRect;
    QPoint m_dragStartPos;
    QRect m_dragStartRect;
    Handle m_activeHandle = Handle::None;

    // Auto detect
    bool m_detectControls = true;

    // Child widgets
    ToolbarWidget* m_toolbar;
    PreviewWidget* m_preview;
};

} // namespace easyshotter
