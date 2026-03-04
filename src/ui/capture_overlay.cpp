#include "capture_overlay.h"
#include "toolbar_widget.h"
#include "preview_widget.h"
#include "../platform/platform_api.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>

namespace easyshotter {

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
        confirmCapture();
    });
    connect(m_toolbar, &ToolbarWidget::saveClicked, this, [this]() {
        confirmCapture();
    });
    connect(m_toolbar, &ToolbarWidget::cancelClicked, this, [this]() {
        cancelCapture();
    });
}

CaptureOverlay::~CaptureOverlay()
{
    delete m_toolbar;
    delete m_preview;
}

void CaptureOverlay::startCapture()
{
    // Cover the entire virtual desktop
    QRect virtualGeometry;
    for (QScreen* screen : QApplication::screens()) {
        virtualGeometry = virtualGeometry.united(screen->geometry());
    }
    setGeometry(virtualGeometry);

    // Capture screen
    if (m_api) {
        m_screenPixmap = m_api->captureScreen();
    }

    // Reset state
    m_state = State::AutoDetect;
    m_selectionRect = QRect();
    m_highlightRect = QRect();
    m_activeHandle = Handle::None;

    // Set up preview magnifier
    m_preview->setScreenPixmap(m_screenPixmap);
    m_preview->show();
    m_toolbar->hide();

    setCursor(Qt::CrossCursor);
    showFullScreen();
    setFocus();
    raise();
    activateWindow();
}

// ---- Selection helpers ----

QRect CaptureOverlay::normalizedSelection() const
{
    return m_selectionRect.normalized();
}

void CaptureOverlay::confirmCapture()
{
    QRect sel = normalizedSelection();
    if (!sel.isValid() || sel.isEmpty()) return;

    QPixmap cropped = m_screenPixmap.copy(sel);

    m_toolbar->hide();
    m_preview->hide();
    hide();

    m_state = State::Idle;
    emit captureConfirmed(cropped, sel);
}

void CaptureOverlay::cancelCapture()
{
    m_toolbar->hide();
    m_preview->hide();
    hide();

    m_state = State::Idle;
    emit captureCancelled();
}

void CaptureOverlay::updateAutoDetect(const QPoint& pos)
{
    if (!m_api) return;

    // Try to find window at cursor position
    WindowInfo winInfo = m_api->windowAtPoint(pos);

    if (winInfo.handle != 0 && winInfo.rect.isValid()) {
        // If control detection is enabled, try to find a finer control
        if (m_detectControls) {
            ControlInfo ctrlInfo = m_api->controlAtPoint(pos);
            if (ctrlInfo.rect.isValid() && !ctrlInfo.rect.isEmpty()) {
                m_highlightRect = ctrlInfo.rect;
                return;
            }
        }
        m_highlightRect = winInfo.rect;
    } else {
        m_highlightRect = QRect();
    }
}

void CaptureOverlay::updateCursorShape(const QPoint& pos)
{
    Handle h = hitTestHandle(pos);
    setCursor(cursorForHandle(h));
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
}

void CaptureOverlay::drawDimOverlay(QPainter& painter) const
{
    QRect activeRect;
    if (m_state == State::AutoDetect) {
        activeRect = m_highlightRect;
    } else if (m_state != State::Idle) {
        activeRect = m_selectionRect.normalized();
    }

    if (activeRect.isValid() && !activeRect.isEmpty()) {
        // Draw dim overlay with hole
        QPainterPath fullPath;
        fullPath.addRect(rect());
        QPainterPath holePath;
        holePath.addRect(activeRect);
        QPainterPath dimPath = fullPath.subtracted(holePath);
        painter.fillPath(dimPath, QColor(0, 0, 0, 128));
    } else {
        painter.fillRect(rect(), QColor(0, 0, 0, 128));
    }
}

void CaptureOverlay::drawHighlightRegion(QPainter& painter, const QRect& highlightRect) const
{
    Q_UNUSED(painter)
    Q_UNUSED(highlightRect)
    // The "hole" in dim overlay already reveals the original image beneath
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

    // Position above the top-left corner
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
        if (m_state == State::Selected) {
            // Right-click on selected: go back to auto-detect
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
            m_toolbar->hide();
            m_preview->show();
            setCursor(Qt::CrossCursor);
            update();
        } else {
            cancelCapture();
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    QPoint pos = event->pos();

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
            // Click outside selection: start new selection
            m_state = State::Selecting;
            m_dragStartPos = pos;
            m_selectionRect = QRect(pos, pos);
            m_toolbar->hide();
            m_preview->show();
        }
    } else if (m_state == State::AutoDetect) {
        if (m_highlightRect.isValid() && m_highlightRect.contains(pos)) {
            // Accept auto-detected region
            m_selectionRect = m_highlightRect;
            m_state = State::Selected;
            m_toolbar->showNearRect(m_selectionRect);
            m_preview->hide();
            update();
            return;
        }
        // Start manual selection
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

    switch (m_state) {
    case State::AutoDetect:
        updateAutoDetect(pos);
        m_preview->updatePosition(pos);
        update();
        break;

    case State::Selecting:
        m_selectionRect = QRect(m_dragStartPos, pos);
        m_preview->updatePosition(pos);
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

    switch (m_state) {
    case State::Selecting: {
        QRect sel = normalizedSelection();
        if (sel.width() > 3 && sel.height() > 3) {
            m_selectionRect = sel;
            m_state = State::Selected;
            m_toolbar->showNearRect(sel);
            m_preview->hide();
        } else {
            // Selection too small, go back to auto-detect
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
        }
        update();
        break;
    }

    case State::Moving:
    case State::Resizing:
        m_selectionRect = normalizedSelection();
        m_state = State::Selected;
        m_activeHandle = Handle::None;
        m_toolbar->showNearRect(m_selectionRect);
        update();
        break;

    default:
        break;
    }
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_state == State::Selected) {
            confirmCapture();
        } else if (m_state == State::AutoDetect && !m_highlightRect.isEmpty()) {
            m_selectionRect = m_highlightRect;
            confirmCapture();
        }
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (m_state == State::Selected) {
            // First ESC: go back to auto-detect
            m_state = State::AutoDetect;
            m_selectionRect = QRect();
            m_toolbar->hide();
            m_preview->show();
            setCursor(Qt::CrossCursor);
            update();
        } else {
            cancelCapture();
        }
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_state == State::Selected) {
            confirmCapture();
        }
        break;

    case Qt::Key_Tab:
        // Toggle between window-level and control-level detection
        m_detectControls = !m_detectControls;
        if (m_state == State::AutoDetect) {
            updateAutoDetect(mapFromGlobal(QCursor::pos()));
            update();
        }
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

} // namespace easyshotter
