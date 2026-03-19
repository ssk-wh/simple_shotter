#include "capture_overlay.h"
#include "toolbar_widget.h"
#include "style_panel_widget.h"
#include "preview_widget.h"
#include "../platform/platform_api.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>

namespace simpleshotter {

CaptureOverlay::CaptureOverlay(PlatformApi* api, QWidget* parent)
    : QWidget(parent)
    , m_api(api)
    , m_toolbar(new ToolbarWidget(nullptr))
    , m_preview(new PreviewWidget(nullptr))
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    connect(m_toolbar, &ToolbarWidget::copyClicked, this, [this]() {
        commitTextInput();
        confirmCapture(SaveAction::Copy);
    });
    connect(m_toolbar, &ToolbarWidget::saveToDesktopClicked, this, [this]() {
        commitTextInput();
        confirmCapture(SaveAction::SaveToDesktop);
    });
    connect(m_toolbar, &ToolbarWidget::saveToFolderClicked, this, [this]() {
        commitTextInput();
        confirmCapture(SaveAction::SaveToFolder);
    });
    connect(m_toolbar, &ToolbarWidget::cancelClicked, this, [this]() {
        cancelCapture();
    });
    connect(m_toolbar, &ToolbarWidget::toolSelected, this, &CaptureOverlay::onToolSelected);
    connect(m_toolbar, &ToolbarWidget::undoClicked, this, &CaptureOverlay::onUndo);
    connect(m_toolbar, &ToolbarWidget::redoClicked, this, &CaptureOverlay::onRedo);
}

CaptureOverlay::~CaptureOverlay()
{
    delete m_toolbar;
    delete m_preview;
    // m_textInput is parented to this, Qt manages its lifetime
}

QColor CaptureOverlay::annotationColor() const
{
    auto* panel = m_toolbar->stylePanel();
    return panel ? panel->currentColor() : QColor(255, 0, 0);
}

int CaptureOverlay::annotationPenWidth() const
{
    auto* panel = m_toolbar->stylePanel();
    return panel ? panel->currentPenWidth() : 2;
}

int CaptureOverlay::mosaicBlockSize() const
{
    auto* panel = m_toolbar->stylePanel();
    return panel ? panel->currentMosaicSize() : 10;
}

int CaptureOverlay::annotationFontSize() const
{
    auto* panel = m_toolbar->stylePanel();
    return panel ? panel->currentFontSize() : 14;
}

void CaptureOverlay::startCapture()
{
    // Capture screen and cache window list BEFORE showing overlay
    if (m_api) {
        m_screenPixmap = m_api->captureScreen();
        m_cachedWindows = m_api->getVisibleWindows();
    }
    m_controlsCache.clear();

    // Reset state
    m_state = State::AutoDetect;
    m_selectionRect = QRect();
    m_highlightRect = QRect();
    m_clickHighlightRect = QRect();
    m_activeHandle = Handle::None;
    m_activeTool = AnnotationTool::None;
    m_annotations.clear();
    m_currentAnnotation.reset();
    m_isDrawingAnnotation = false;
    if (m_textInput) {
        m_textInput->hide();
        m_textInput->clear();
    }

    // Reset toolbar state
    m_toolbar->setActiveTool(AnnotationTool::None);
    m_toolbar->updateUndoRedoState(false, false);

    // Set up preview magnifier
    m_preview->setScreenPixmap(m_screenPixmap);
    m_toolbar->hide();

    // Cover the entire virtual desktop
    QRect virtualGeometry;
    for (QScreen* screen : QApplication::screens()) {
        virtualGeometry = virtualGeometry.united(screen->geometry());
    }

    setCursor(Qt::CrossCursor);
    setGeometry(virtualGeometry);
    show();
    setGeometry(virtualGeometry);
    setFocus();
    raise();
    activateWindow();

    // Show preview after overlay is visible, so it stays on top
    m_preview->show();
    m_preview->raise();
}

// ---- Selection helpers ----

QRect CaptureOverlay::normalizedSelection() const
{
    return m_selectionRect.normalized();
}

QRect CaptureOverlay::selectionToScreen(const QRect& widgetRect) const
{
    return QRect(mapToGlobal(widgetRect.topLeft()), widgetRect.size());
}

void CaptureOverlay::confirmCapture(SaveAction action)
{
    QRect sel = normalizedSelection();
    if (!sel.isValid() || sel.isEmpty()) return;

    // Render annotations onto the cropped pixmap
    QPixmap cropped = m_screenPixmap.copy(sel);
    if (!m_annotations.items().empty()) {
        QPainter painter(&cropped);
        painter.translate(-sel.topLeft());
        for (const auto& item : m_annotations.items()) {
            item->drawToPixmap(painter);
        }
    }

    m_toolbar->hide();
    m_preview->hide();
    if (m_textInput) m_textInput->hide();
    hide();

    m_state = State::Idle;
    if (action == SaveAction::Copy) {
        emit captureConfirmed(cropped, sel);
    } else {
        emit captureSaveRequested(cropped, sel, action);
    }
}

void CaptureOverlay::cancelCapture()
{
    m_toolbar->hide();
    m_preview->hide();
    if (m_textInput) {
        m_textInput->hide();
    }
    hide();

    m_state = State::Idle;
    emit captureCancelled();
}

void CaptureOverlay::updateAutoDetect(const QPoint& pos)
{
    if (!m_api) return;

    QPoint screenPos = mapToGlobal(pos);

    WindowInfo matchedWindow;
    for (const auto& win : m_cachedWindows) {
        if (win.rect.contains(screenPos)) {
            matchedWindow = win;
            break;
        }
    }

    if (matchedWindow.handle != 0 && matchedWindow.rect.isValid()) {
        // Always highlight the window first (instant feedback)
        m_highlightRect = matchedWindow.rect.translated(-geometry().topLeft());

        if (m_detectControls) {
            auto it = m_controlsCache.find(matchedWindow.handle);
            if (it == m_controlsCache.end()) {
                // Mark as pending (empty vector) so we don't re-trigger
                m_controlsCache.emplace(matchedWindow.handle, std::vector<ControlInfo>());
                // Load controls in background thread to avoid blocking UI
                quintptr handle = matchedWindow.handle;
                auto* watcher = new QFutureWatcher<std::vector<ControlInfo>>(this);
                connect(watcher, &QFutureWatcher<std::vector<ControlInfo>>::finished, this, [this, handle, watcher]() {
                    if (m_state != State::Idle) {
                        m_controlsCache[handle] = watcher->result();
                        update();
                    }
                    watcher->deleteLater();
                });
                watcher->setFuture(QtConcurrent::run([handle]() {
                    return PlatformApi::getWindowControlsAsync(handle);
                }));
                return;
            }

            // Controls already cached - find best match
            ControlInfo bestControl;
            int bestArea = INT_MAX;
            for (const auto& ctrl : it->second) {
                if (ctrl.rect.contains(screenPos)) {
                    int area = ctrl.rect.width() * ctrl.rect.height();
                    if (area < bestArea) {
                        bestArea = area;
                        bestControl = ctrl;
                    }
                }
            }

            if (bestControl.rect.isValid() && !bestControl.rect.isEmpty()) {
                m_highlightRect = bestControl.rect.translated(-geometry().topLeft());
            }
        }
    } else {
        m_highlightRect = QRect();
    }
}

void CaptureOverlay::updateCursorShape(const QPoint& pos)
{
    Handle h = hitTestHandle(pos);
    setCursor(cursorForHandle(h));
}

// ---- Annotation helpers ----

void CaptureOverlay::raiseToolWidgets()
{
    m_toolbar->raise();
    if (auto* panel = m_toolbar->stylePanel()) {
        if (panel->isVisible()) panel->raise();
    }
}

void CaptureOverlay::onToolSelected(AnnotationTool tool)
{
    commitTextInput();
    m_activeTool = tool;
    m_toolbar->setActiveTool(tool);

    if (tool != AnnotationTool::None) {
        m_state = State::Annotating;
        setCursor(Qt::CrossCursor);
    } else {
        m_state = State::Selected;
        updateCursorShape(mapFromGlobal(QCursor::pos()));
    }
    setFocus();
    raiseToolWidgets();
    update();
}

void CaptureOverlay::onUndo()
{
    m_annotations.undo();
    updateToolbarUndoRedo();
    update();
    setFocus();
    raiseToolWidgets();
}

void CaptureOverlay::onRedo()
{
    m_annotations.redo();
    updateToolbarUndoRedo();
    update();
    setFocus();
    raiseToolWidgets();
}

void CaptureOverlay::updateToolbarUndoRedo()
{
    m_toolbar->updateUndoRedoState(m_annotations.canUndo(), m_annotations.canRedo());
}

void CaptureOverlay::commitTextInput()
{
    if (!m_textInput || !m_textInput->isVisible()) return;

    QString text = m_textInput->toPlainText().trimmed();
    if (!text.isEmpty()) {
        auto item = std::make_unique<TextAnnotation>();
        item->pos = m_textInput->pos() + QPoint(4, m_textInput->fontMetrics().ascent() + 4);
        item->text = text;
        item->color = annotationColor();
        item->font = m_textInput->font();
        m_annotations.addItem(std::move(item));
        updateToolbarUndoRedo();
    }

    m_textInput->hide();
    m_textInput->clear();
    setFocus();
    update();
}

void CaptureOverlay::restoreToolWidgets()
{
    m_toolbar->showNearRect(selectionToScreen(normalizedSelection()));
    m_toolbar->setActiveTool(m_activeTool);
}

void CaptureOverlay::startTextInput(const QPoint& pos)
{
    if (!m_textInput) {
        m_textInput = new QTextEdit(this);
        m_textInput->setFrameShape(QFrame::NoFrame);
        m_textInput->viewport()->setAutoFillBackground(false);
    }
    // Update text style to current settings
    QFont font = m_textInput->font();
    font.setPointSize(annotationFontSize());
    m_textInput->setFont(font);
    QPalette pal = m_textInput->palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Text, annotationColor());
    m_textInput->setPalette(pal);

    QRect sel = normalizedSelection();
    int maxW = sel.right() - pos.x();
    int maxH = sel.bottom() - pos.y();
    if (maxW < 60) maxW = 60;
    if (maxH < 30) maxH = 30;

    m_textInput->setGeometry(pos.x(), pos.y(), qMin(maxW, 300), qMin(maxH, 100));
    m_textInput->clear();
    m_textInput->show();
    m_textInput->setFocus();
}

// ---- Painting ----

void CaptureOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    if (m_screenPixmap.isNull()) return;

    QPainter painter(this);

    // 1. Draw full screen capture as background
    painter.drawPixmap(0, 0, m_screenPixmap);

    // 2. Determine active rect
    QRect activeRect;
    if (m_state == State::AutoDetect) {
        activeRect = m_highlightRect;
    } else if (m_state != State::Idle) {
        activeRect = normalizedSelection();
    }

    // 3. Draw dim overlay with hole for active rect
    drawDimOverlay(painter);

    // 4. Draw highlight region + border
    if (activeRect.isValid() && !activeRect.isEmpty()) {
        drawHighlightRegion(painter, activeRect);
        drawSelectionBorder(painter, activeRect);
        drawSizeLabel(painter, activeRect);
    }

    // 5. Draw handles for selected/moving/resizing states
    if (m_state == State::Selected || m_state == State::Moving || m_state == State::Resizing) {
        QRect sel = normalizedSelection();
        if (sel.isValid()) {
            drawHandles(painter, sel);
        }
    }

    // 6. Draw annotations
    drawAnnotations(painter);
}

void CaptureOverlay::drawDimOverlay(QPainter& painter) const
{
    QRect activeRect;
    if (m_state == State::AutoDetect) {
        activeRect = m_highlightRect;
    } else if (m_state != State::Idle) {
        activeRect = m_selectionRect.normalized();
    }

    QColor dimColor(0, 0, 0, 128);
    if (activeRect.isValid() && !activeRect.isEmpty()) {
        // Draw four rectangles around the hole instead of path subtraction
        QRect full = rect();
        // Top
        painter.fillRect(QRect(full.left(), full.top(), full.width(), activeRect.top() - full.top()), dimColor);
        // Bottom
        painter.fillRect(QRect(full.left(), activeRect.bottom() + 1, full.width(), full.bottom() - activeRect.bottom()), dimColor);
        // Left
        painter.fillRect(QRect(full.left(), activeRect.top(), activeRect.left() - full.left(), activeRect.height()), dimColor);
        // Right
        painter.fillRect(QRect(activeRect.right() + 1, activeRect.top(), full.right() - activeRect.right(), activeRect.height()), dimColor);
    } else {
        painter.fillRect(rect(), dimColor);
    }
}

void CaptureOverlay::drawHighlightRegion(QPainter& painter, const QRect& highlightRect) const
{
    Q_UNUSED(painter)
    Q_UNUSED(highlightRect)
}

void CaptureOverlay::drawSelectionBorder(QPainter& painter, const QRect& selRect) const
{
    painter.save();
    QPen pen(kAccentColor, kBorderWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selRect.adjusted(0, 0, -1, -1));
    painter.restore();
}

void CaptureOverlay::drawHandles(QPainter& painter, const QRect& selRect) const
{
    Q_UNUSED(selRect)
    painter.save();
    QVector<QRect> handles = allHandleRects();
    painter.setPen(QPen(kAccentColor, 1));
    painter.setBrush(Qt::white);
    for (const QRect& hr : handles) {
        painter.drawRect(hr);
    }
    painter.restore();
}

void CaptureOverlay::drawSizeLabel(QPainter& painter, const QRect& selRect) const
{
    painter.save();

    QString text = QString("%1 x %2").arg(selRect.width()).arg(selRect.height());

    QFont font("Segoe UI", 9);
    font.setStyleHint(QFont::SansSerif);
    painter.setFont(font);
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text) + 14;
    int textHeight = fm.height() + 6;

    int labelX = selRect.left();
    int labelY = selRect.top() - textHeight - 4;
    if (labelY < 0) {
        labelY = selRect.top() + 4;
    }

    QRect labelRect(labelX, labelY, textWidth, textHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(kAccentColor);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawRoundedRect(labelRect, 3, 3);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, text);

    painter.restore();
}

void CaptureOverlay::drawAnnotations(QPainter& painter) const
{
    // Clip to selection area
    QRect sel = normalizedSelection();
    if (!sel.isValid()) return;

    painter.save();
    painter.setClipRect(sel);

    // Draw committed annotations
    m_annotations.drawAll(painter);

    // Draw in-progress annotation
    if (m_currentAnnotation) {
        m_currentAnnotation->draw(painter);
    }

    painter.restore();
}

// ---- Hit testing ----

QRect CaptureOverlay::handleRect(Handle handle) const
{
    QRect r = normalizedSelection();
    if (r.isEmpty()) return QRect();

    int hs = kHandleSize;
    int hh = hs / 2;
    int cx = (r.left() + r.right()) / 2;
    int cy = (r.top() + r.bottom()) / 2;

    switch (handle) {
    case Handle::TopLeft:     return QRect(r.left() - hh, r.top() - hh, hs, hs);
    case Handle::Top:         return QRect(cx - hh, r.top() - hh, hs, hs);
    case Handle::TopRight:    return QRect(r.right() - hh, r.top() - hh, hs, hs);
    case Handle::Left:        return QRect(r.left() - hh, cy - hh, hs, hs);
    case Handle::Right:       return QRect(r.right() - hh, cy - hh, hs, hs);
    case Handle::BottomLeft:  return QRect(r.left() - hh, r.bottom() - hh, hs, hs);
    case Handle::Bottom:      return QRect(cx - hh, r.bottom() - hh, hs, hs);
    case Handle::BottomRight: return QRect(r.right() - hh, r.bottom() - hh, hs, hs);
    default: return QRect();
    }
}

QVector<QRect> CaptureOverlay::allHandleRects() const
{
    return {
        handleRect(Handle::TopLeft),
        handleRect(Handle::Top),
        handleRect(Handle::TopRight),
        handleRect(Handle::Left),
        handleRect(Handle::Right),
        handleRect(Handle::BottomLeft),
        handleRect(Handle::Bottom),
        handleRect(Handle::BottomRight),
    };
}

CaptureOverlay::Handle CaptureOverlay::hitTestHandle(const QPoint& pos) const
{
    const Handle types[] = {
        Handle::TopLeft, Handle::Top, Handle::TopRight,
        Handle::Left, Handle::Right,
        Handle::BottomLeft, Handle::Bottom, Handle::BottomRight
    };

    for (auto h : types) {
        QRect r = handleRect(h).adjusted(-kHandleHitMargin, -kHandleHitMargin,
                                          kHandleHitMargin, kHandleHitMargin);
        if (r.contains(pos)) return h;
    }

    if (normalizedSelection().contains(pos)) {
        return Handle::Body;
    }

    return Handle::None;
}

Qt::CursorShape CaptureOverlay::cursorForHandle(Handle handle) const
{
    switch (handle) {
    case Handle::TopLeft:
    case Handle::BottomRight:  return Qt::SizeFDiagCursor;
    case Handle::TopRight:
    case Handle::BottomLeft:   return Qt::SizeBDiagCursor;
    case Handle::Top:
    case Handle::Bottom:       return Qt::SizeVerCursor;
    case Handle::Left:
    case Handle::Right:        return Qt::SizeHorCursor;
    case Handle::Body:         return Qt::SizeAllCursor;
    case Handle::None:
    default:                   return Qt::CrossCursor;
    }
}

// ---- Mouse events ----

void CaptureOverlay::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        if (m_state == State::Annotating) {
            // Right-click in annotation mode: deselect tool
            commitTextInput();
            m_activeTool = AnnotationTool::None;
            m_currentAnnotation.reset();
            m_isDrawingAnnotation = false;
            m_state = State::Selected;
            restoreToolWidgets();
            updateCursorShape(event->pos());
            update();
        } else if (m_state == State::Selected) {
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
            m_annotations.clear();
            updateToolbarUndoRedo();
            m_toolbar->hide();
            m_preview->show();
            m_preview->raise();
            setCursor(Qt::CrossCursor);
            update();
        } else {
            cancelCapture();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    QPoint pos = event->pos();

    if (m_state == State::Annotating) {
        QRect sel = normalizedSelection();
        if (!sel.contains(pos)) return;

        if (m_activeTool == AnnotationTool::Text) {
            commitTextInput();
            m_toolbar->hide();
            startTextInput(pos);
            return;
        }

        m_isDrawingAnnotation = true;
        m_dragStartPos = pos;
        m_toolbar->hide();

        switch (m_activeTool) {
        case AnnotationTool::Rectangle: {
            auto item = std::make_unique<RectAnnotation>();
            item->rect = QRect(pos, pos);
            item->color = annotationColor();
            item->penWidth = annotationPenWidth();
            m_currentAnnotation = std::move(item);
            break;
        }
        case AnnotationTool::Ellipse: {
            auto item = std::make_unique<EllipseAnnotation>();
            item->rect = QRect(pos, pos);
            item->color = annotationColor();
            item->penWidth = annotationPenWidth();
            m_currentAnnotation = std::move(item);
            break;
        }
        case AnnotationTool::Arrow: {
            auto item = std::make_unique<ArrowAnnotation>();
            item->startPos = pos;
            item->endPos = pos;
            item->color = annotationColor();
            item->penWidth = annotationPenWidth();
            m_currentAnnotation = std::move(item);
            break;
        }
        case AnnotationTool::Mosaic: {
            auto item = std::make_unique<MosaicAnnotation>();
            item->setSource(m_screenPixmap);
            item->blockSize = mosaicBlockSize();
            item->addPoint(pos);
            m_currentAnnotation = std::move(item);
            break;
        }
        default:
            m_isDrawingAnnotation = false;
            break;
        }
        update();
        return;
    }

    if (m_state == State::Selected) {
        Handle h = hitTestHandle(pos);
        if (h == Handle::Body) {
            m_state = State::Moving;
            m_activeHandle = h;
            m_dragStartPos = pos;
            m_dragStartRect = normalizedSelection();
            m_toolbar->hide();
        } else if (h != Handle::None) {
            m_state = State::Resizing;
            m_activeHandle = h;
            m_dragStartPos = pos;
            m_dragStartRect = normalizedSelection();
            m_toolbar->hide();
        } else {
            m_state = State::Selecting;
            m_dragStartPos = pos;
            m_selectionRect = QRect(pos, pos);
            m_annotations.clear();
            updateToolbarUndoRedo();
            m_toolbar->hide();
            m_preview->show();
            m_preview->raise();
        }
    } else if (m_state == State::AutoDetect) {
        m_clickHighlightRect = m_highlightRect;
        m_state = State::Selecting;
        m_dragStartPos = pos;
        m_selectionRect = QRect(pos, pos);
        m_toolbar->hide();
    } else {
        m_state = State::Selecting;
        m_dragStartPos = pos;
        m_selectionRect = QRect(pos, pos);
        m_toolbar->hide();
    }
    update();
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent* event)
{
    QPoint pos = event->pos();

    if (m_state == State::Annotating) {
        if (m_isDrawingAnnotation && m_currentAnnotation) {
            switch (m_activeTool) {
            case AnnotationTool::Rectangle:
                static_cast<RectAnnotation*>(m_currentAnnotation.get())->rect = QRect(m_dragStartPos, pos);
                break;
            case AnnotationTool::Ellipse:
                static_cast<EllipseAnnotation*>(m_currentAnnotation.get())->rect = QRect(m_dragStartPos, pos);
                break;
            case AnnotationTool::Arrow:
                static_cast<ArrowAnnotation*>(m_currentAnnotation.get())->endPos = pos;
                break;
            case AnnotationTool::Mosaic:
                static_cast<MosaicAnnotation*>(m_currentAnnotation.get())->addPoint(pos);
                break;
            default:
                break;
            }
            update();
        }
        return;
    }

    switch (m_state) {
    case State::AutoDetect:
        if (m_mouseMoveThrottle.isValid() && m_mouseMoveThrottle.elapsed() < kMouseMoveThrottleMs)
            return;
        m_mouseMoveThrottle.restart();
        updateAutoDetect(pos);
        m_preview->updatePosition(mapToGlobal(pos), pos);
        update();
        break;

    case State::Selecting:
        m_selectionRect = QRect(m_dragStartPos, pos);
        m_preview->updatePosition(mapToGlobal(pos), pos);
        update();
        break;

    case State::Selected:
        updateCursorShape(pos);
        break;

    case State::Moving: {
        QPoint delta = pos - m_dragStartPos;
        m_selectionRect = m_dragStartRect.translated(delta);
        update();
        break;
    }

    case State::Resizing: {
        QPoint delta = pos - m_dragStartPos;
        QRect r = m_dragStartRect;

        switch (m_activeHandle) {
        case Handle::TopLeft:     r.setTopLeft(r.topLeft() + delta); break;
        case Handle::Top:         r.setTop(r.top() + delta.y()); break;
        case Handle::TopRight:    r.setTopRight(r.topRight() + delta); break;
        case Handle::Left:        r.setLeft(r.left() + delta.x()); break;
        case Handle::Right:       r.setRight(r.right() + delta.x()); break;
        case Handle::BottomLeft:  r.setBottomLeft(r.bottomLeft() + delta); break;
        case Handle::Bottom:      r.setBottom(r.bottom() + delta.y()); break;
        case Handle::BottomRight: r.setBottomRight(r.bottomRight() + delta); break;
        default: break;
        }

        m_selectionRect = r;
        update();
        break;
    }

    default:
        break;
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    if (m_state == State::Annotating && m_isDrawingAnnotation) {
        m_isDrawingAnnotation = false;
        if (m_currentAnnotation) {
            // Check if the annotation is large enough to keep
            bool keep = true;
            if (m_activeTool == AnnotationTool::Rectangle) {
                QRect r = static_cast<RectAnnotation*>(m_currentAnnotation.get())->rect.normalized();
                keep = r.width() > 2 && r.height() > 2;
            } else if (m_activeTool == AnnotationTool::Ellipse) {
                QRect r = static_cast<EllipseAnnotation*>(m_currentAnnotation.get())->rect.normalized();
                keep = r.width() > 2 && r.height() > 2;
            } else if (m_activeTool == AnnotationTool::Arrow) {
                auto* arrow = static_cast<ArrowAnnotation*>(m_currentAnnotation.get());
                int dx = arrow->endPos.x() - arrow->startPos.x();
                int dy = arrow->endPos.y() - arrow->startPos.y();
                keep = (dx * dx + dy * dy) > 9;
            }

            if (keep) {
                m_annotations.addItem(std::move(m_currentAnnotation));
                updateToolbarUndoRedo();
            } else {
                m_currentAnnotation.reset();
            }
        }
        // Re-show toolbar and style panel after drawing
        restoreToolWidgets();
        update();
        return;
    }

    switch (m_state) {
    case State::Selecting: {
        QRect sel = normalizedSelection();
        if (sel.width() > 3 && sel.height() > 3) {
            m_selectionRect = sel;
            m_state = State::Selected;
            m_toolbar->showNearRect(selectionToScreen(sel));
            m_preview->hide();
        } else if (m_clickHighlightRect.isValid() && !m_clickHighlightRect.isEmpty()) {
            m_selectionRect = m_clickHighlightRect;
            m_state = State::Selected;
            m_toolbar->showNearRect(selectionToScreen(m_selectionRect));
            m_preview->hide();
        } else {
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
        }
        m_clickHighlightRect = QRect();
        update();
        break;
    }

    case State::Moving:
    case State::Resizing:
        m_selectionRect = normalizedSelection();
        m_state = State::Selected;
        m_activeHandle = Handle::None;
        m_toolbar->showNearRect(selectionToScreen(m_selectionRect));
        update();
        break;

    default:
        break;
    }
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_state == State::Selected || m_state == State::Annotating) {
            commitTextInput();
            confirmCapture();
        } else if (m_state == State::AutoDetect && !m_highlightRect.isEmpty()) {
            m_selectionRect = m_highlightRect;
            confirmCapture();
        }
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    // If text input is active, let it handle keys
    if (m_textInput && m_textInput->isVisible() && m_textInput->hasFocus()) {
        if (event->key() == Qt::Key_Escape) {
            commitTextInput();
            restoreToolWidgets();
            return;
        }
        // Let QTextEdit handle the key
        return;
    }

    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_state == State::Annotating) {
            m_currentAnnotation.reset();
            m_isDrawingAnnotation = false;
            m_activeTool = AnnotationTool::None;
            m_toolbar->setActiveTool(AnnotationTool::None);
            m_state = State::Selected;
            updateCursorShape(mapFromGlobal(QCursor::pos()));
            update();
        } else if (m_state == State::Selected) {
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
            m_annotations.clear();
            updateToolbarUndoRedo();
            m_toolbar->hide();
            m_preview->show();
            m_preview->raise();
            setCursor(Qt::CrossCursor);
            update();
        } else {
            cancelCapture();
        }
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_state == State::Selected || m_state == State::Annotating) {
            commitTextInput();
            confirmCapture();
        }
        break;

    case Qt::Key_Tab:
        m_detectControls = !m_detectControls;
        if (m_state == State::AutoDetect) {
            updateAutoDetect(mapFromGlobal(QCursor::pos()));
            update();
        }
        break;

    case Qt::Key_Z:
        if (event->modifiers() & Qt::ControlModifier) {
            if (event->modifiers() & Qt::ShiftModifier) {
                onRedo();
            } else {
                onUndo();
            }
        }
        break;

    case Qt::Key_Y:
        if (event->modifiers() & Qt::ControlModifier) {
            onRedo();
        }
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

} // namespace simpleshotter
