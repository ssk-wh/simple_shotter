#include "region_selector.h"
#include <algorithm>

namespace easyshotter {

RegionSelector::RegionSelector(QObject* parent)
    : QObject(parent)
{
}

RegionSelector::~RegionSelector() = default;

void RegionSelector::beginSelection(const QPoint& pos)
{
    m_startPos = pos;
    m_rect = QRect(pos, pos);
    m_isSelecting = true;
}

void RegionSelector::updateSelection(const QPoint& pos)
{
    if (!m_isSelecting) return;
    m_rect = QRect(m_startPos, pos).normalized();
    emit selectionChanged(m_rect);
}

void RegionSelector::endSelection()
{
    m_isSelecting = false;
    m_rect = m_rect.normalized();
    emit selectionFinished(m_rect);
}

void RegionSelector::setRect(const QRect& rect)
{
    m_rect = rect;
    emit selectionChanged(m_rect);
}

QRect RegionSelector::rect() const
{
    return m_rect;
}

QRect RegionSelector::normalizedRect() const
{
    return m_rect.normalized();
}

RegionSelector::Handle RegionSelector::hitTest(const QPoint& pos) const
{
    auto handles = handleRects();
    const Handle handleTypes[] = {
        Handle::TopLeft, Handle::Top, Handle::TopRight,
        Handle::Left, Handle::Right,
        Handle::BottomLeft, Handle::Bottom, Handle::BottomRight
    };

    for (int i = 0; i < 8; ++i) {
        if (handles[i].contains(pos)) {
            return handleTypes[i];
        }
    }

    if (m_rect.normalized().contains(pos)) {
        return Handle::Body;
    }

    return Handle::None;
}

void RegionSelector::beginResize(Handle handle, const QPoint& pos)
{
    m_activeHandle = handle;
    m_originalRect = m_rect.normalized();
    m_resizeStartPos = pos;
}

void RegionSelector::updateResize(const QPoint& pos)
{
    if (m_activeHandle == Handle::None) return;

    QPoint delta = pos - m_resizeStartPos;
    QRect r = m_originalRect;

    switch (m_activeHandle) {
    case Handle::TopLeft:
        r.setTopLeft(r.topLeft() + delta);
        break;
    case Handle::Top:
        r.setTop(r.top() + delta.y());
        break;
    case Handle::TopRight:
        r.setTopRight(r.topRight() + delta);
        break;
    case Handle::Left:
        r.setLeft(r.left() + delta.x());
        break;
    case Handle::Right:
        r.setRight(r.right() + delta.x());
        break;
    case Handle::BottomLeft:
        r.setBottomLeft(r.bottomLeft() + delta);
        break;
    case Handle::Bottom:
        r.setBottom(r.bottom() + delta.y());
        break;
    case Handle::BottomRight:
        r.setBottomRight(r.bottomRight() + delta);
        break;
    case Handle::Body:
        r.translate(delta);
        break;
    case Handle::None:
        break;
    }

    m_rect = r;
    emit selectionChanged(m_rect);
}

std::array<QRect, 8> RegionSelector::handleRects() const
{
    QRect r = m_rect.normalized();
    int hs = HANDLE_SIZE;
    int hh = hs / 2;

    return {{
        QRect(r.left() - hh, r.top() - hh, hs, hs),
        QRect(r.center().x() - hh, r.top() - hh, hs, hs),
        QRect(r.right() - hh, r.top() - hh, hs, hs),
        QRect(r.left() - hh, r.center().y() - hh, hs, hs),
        QRect(r.right() - hh, r.center().y() - hh, hs, hs),
        QRect(r.left() - hh, r.bottom() - hh, hs, hs),
        QRect(r.center().x() - hh, r.bottom() - hh, hs, hs),
        QRect(r.right() - hh, r.bottom() - hh, hs, hs),
    }};
}

bool RegionSelector::isSelecting() const
{
    return m_isSelecting;
}

} // namespace easyshotter
