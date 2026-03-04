#pragma once

#include <QObject>
#include <QRect>
#include <QPoint>
#include <array>

namespace easyshotter {

class RegionSelector : public QObject {
    Q_OBJECT
public:
    enum class Handle {
        None,
        TopLeft, Top, TopRight,
        Left, Right,
        BottomLeft, Bottom, BottomRight,
        Body
    };

    explicit RegionSelector(QObject* parent = nullptr);
    ~RegionSelector();

    void beginSelection(const QPoint& pos);
    void updateSelection(const QPoint& pos);
    void endSelection();

    void setRect(const QRect& rect);
    QRect rect() const;
    QRect normalizedRect() const;

    Handle hitTest(const QPoint& pos) const;
    void beginResize(Handle handle, const QPoint& pos);
    void updateResize(const QPoint& pos);

    std::array<QRect, 8> handleRects() const;

    bool isSelecting() const;

signals:
    void selectionChanged(const QRect& region);
    void selectionFinished(const QRect& region);

private:
    static constexpr int HANDLE_SIZE = 8;

    QRect m_rect;
    QPoint m_startPos;
    Handle m_activeHandle = Handle::None;
    QRect m_originalRect;
    QPoint m_resizeStartPos;
    bool m_isSelecting = false;
};

} // namespace easyshotter
