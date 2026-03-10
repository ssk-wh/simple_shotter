#pragma once

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QVector>
#include <QColor>
#include "../platform/platform_api.h"
#include "annotation_item.h"
#include <unordered_map>
#include <memory>

class QTextEdit;

namespace easyshotter {

class ToolbarWidget;
class PreviewWidget;

class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CaptureOverlay(PlatformApi* api, QWidget* parent = nullptr);
    ~CaptureOverlay() override;

    void startCapture();

    enum class SaveAction { Copy, SaveToDesktop, SaveToFolder };

signals:
    void captureConfirmed(const QPixmap& pixmap, const QRect& region);
    void captureSaveRequested(const QPixmap& pixmap, const QRect& region, SaveAction action);
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
        Resizing,
        Annotating
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
    void drawAnnotations(QPainter& painter) const;

    // Hit testing
    Handle hitTestHandle(const QPoint& pos) const;
    Qt::CursorShape cursorForHandle(Handle handle) const;
    QRect handleRect(Handle handle) const;
    QVector<QRect> allHandleRects() const;

    // Selection helpers
    QRect normalizedSelection() const;
    QRect selectionToScreen(const QRect& widgetRect) const;
    void confirmCapture(SaveAction action = SaveAction::Copy);
    void cancelCapture();
    void updateAutoDetect(const QPoint& pos);
    void updateCursorShape(const QPoint& pos);

    // Annotation helpers
    void raiseToolWidgets();
    void onToolSelected(AnnotationTool tool);
    void onUndo();
    void onRedo();
    void updateToolbarUndoRedo();
    void commitTextInput();
    void startTextInput(const QPoint& pos);

    static constexpr int kHandleSize = 8;
    static constexpr int kHandleHitMargin = 4;
    static constexpr int kBorderWidth = 2;
    static inline const QColor kAccentColor{0, 120, 215};
    QColor annotationColor() const;
    int annotationPenWidth() const;
    int mosaicBlockSize() const;

    PlatformApi* m_api;
    State m_state = State::Idle;
    QPixmap m_screenPixmap;

    // Selection
    QRect m_selectionRect;
    QRect m_highlightRect;
    QRect m_clickHighlightRect;
    QPoint m_dragStartPos;
    QRect m_dragStartRect;
    Handle m_activeHandle = Handle::None;

    // Auto detect
    bool m_detectControls = true;
    std::vector<WindowInfo> m_cachedWindows;
    std::unordered_map<quintptr, std::vector<ControlInfo>> m_controlsCache;

    // Annotation
    AnnotationTool m_activeTool = AnnotationTool::None;
    AnnotationManager m_annotations;
    std::unique_ptr<AnnotationItem> m_currentAnnotation;
    bool m_isDrawingAnnotation = false;
    QTextEdit* m_textInput = nullptr;

    // Child widgets
    ToolbarWidget* m_toolbar;
    PreviewWidget* m_preview;
};

} // namespace easyshotter
