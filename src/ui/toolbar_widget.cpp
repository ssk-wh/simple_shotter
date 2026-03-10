#include "toolbar_widget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>

namespace easyshotter {

ToolbarWidget::ToolbarWidget(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
    layoutButtons();
}

ToolbarWidget::~ToolbarWidget()
{
    delete m_saveMenu;
    delete m_stylePanel;
}

void ToolbarWidget::layoutButtons()
{
    m_buttons.clear();

    // Annotation tools (all enabled, toggleable)
    m_buttons.append({{}, "Rect",    3, true, true, false});
    m_buttons.append({{}, "Ellipse", 4, true, true, false});
    m_buttons.append({{}, "Arrow",   5, true, true, false});
    m_buttons.append({{}, "Text",    6, true, true, false});
    m_buttons.append({{}, "Mosaic",  7, true, true, false});
    // Separator
    m_buttons.append({{}, "Undo",    8, false, false, false});
    m_buttons.append({{}, "Redo",    9, false, false, false});
    // Separator
    m_buttons.append({{}, "Copy",    0, true, false, false});
    m_buttons.append({{}, "Save",    1, true, false, false});
    m_buttons.append({{}, "Cancel",  2, true, false, false});

    // Calculate positions
    int x = kPadding;
    int y = kPadding;
    for (int i = 0; i < m_buttons.size(); i++) {
        // Add separator spacing before undo group and action group
        if (i == 5 || i == 7) {
            x += kSeparatorWidth;
        }
        m_buttons[i].rect = QRect(x, y, kButtonWidth, kButtonHeight);
        x += kButtonWidth + kButtonSpacing;
    }

    int totalWidth = x - kButtonSpacing + kPadding;
    int totalHeight = kButtonHeight + kPadding * 2;
    setFixedSize(totalWidth, totalHeight);
}

void ToolbarWidget::showNearRect(const QRect& selectionRect)
{
    // Position toolbar below the selection, aligned to right edge
    int x = selectionRect.right() - width();
    int y = selectionRect.bottom() + 6;

    // If not enough space below, show above
    QScreen* screen = QApplication::screenAt(selectionRect.center());
    if (screen) {
        QRect screenRect = screen->geometry();
        if (y + height() > screenRect.bottom()) {
            y = selectionRect.top() - height() - 6;
        }
        // Clamp to screen bounds
        if (x < screenRect.left()) x = screenRect.left();
        if (x + width() > screenRect.right()) x = screenRect.right() - width();
        if (y < screenRect.top()) y = selectionRect.bottom() + 6;
    }

    m_lastSelectionRect = selectionRect;
    move(x, y);
    show();
    raise();
}

void ToolbarWidget::setActiveTool(AnnotationTool tool)
{
    for (int i = 0; i < m_buttons.size(); i++) {
        if (m_buttons[i].toggleable) {
            m_buttons[i].toggled = (toolForIconType(m_buttons[i].iconType) == tool);
        }
    }
    showStylePanel(tool);
    update();
}

void ToolbarWidget::updateUndoRedoState(bool canUndo, bool canRedo)
{
    for (int i = 0; i < m_buttons.size(); i++) {
        if (m_buttons[i].iconType == 8) m_buttons[i].enabled = canUndo;
        if (m_buttons[i].iconType == 9) m_buttons[i].enabled = canRedo;
    }
    update();
}

AnnotationTool ToolbarWidget::toolForIconType(int iconType) const
{
    switch (iconType) {
    case 3: return AnnotationTool::Rectangle;
    case 4: return AnnotationTool::Ellipse;
    case 5: return AnnotationTool::Arrow;
    case 6: return AnnotationTool::Text;
    case 7: return AnnotationTool::Mosaic;
    default: return AnnotationTool::None;
    }
}

void ToolbarWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    QRectF bgRect(0, 0, width(), height());
    painter.setPen(QPen(QColor(64, 64, 64, 200), 1));
    painter.setBrush(QColor(48, 48, 48, 230));
    painter.drawRoundedRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5), kCornerRadius, kCornerRadius);

    // Draw separators
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    auto drawSep = [&](int beforeIndex) {
        if (beforeIndex < m_buttons.size()) {
            int sepX = m_buttons[beforeIndex].rect.left() - kSeparatorWidth / 2;
            painter.drawLine(sepX, kPadding + 4, sepX, kPadding + kButtonHeight - 4);
        }
    };
    drawSep(5); // before undo group
    drawSep(7); // before action group

    // Draw buttons
    for (int i = 0; i < m_buttons.size(); i++) {
        bool hovered = (i == m_hoveredIndex);
        bool pressed = (i == m_pressedIndex);
        drawButton(painter, m_buttons[i], hovered, pressed);
    }
}

void ToolbarWidget::drawButton(QPainter& painter, const ButtonInfo& btn,
                                bool hovered, bool pressed) const
{
    painter.save();

    float opacity = btn.enabled ? 1.0f : 0.35f;
    painter.setOpacity(opacity);

    // Button background
    if (btn.toggled && btn.enabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 120, 215, 120));
        painter.drawRoundedRect(btn.rect, 3, 3);
    } else if (pressed && btn.enabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 50));
        painter.drawRoundedRect(btn.rect, 3, 3);
    } else if (hovered && btn.enabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 30));
        painter.drawRoundedRect(btn.rect, 3, 3);
    }

    // Icon area (centered in button)
    int iconSize = 16;
    QRect iconRect(
        btn.rect.left() + (btn.rect.width() - iconSize) / 2,
        btn.rect.top() + (btn.rect.height() - iconSize) / 2,
        iconSize, iconSize);

    painter.setPen(QPen(QColor(220, 220, 220), 1.5));
    painter.setBrush(Qt::NoBrush);

    switch (btn.iconType) {
    case 0: drawCopyIcon(painter, iconRect); break;
    case 1: drawSaveIcon(painter, iconRect); break;
    case 2: drawCancelIcon(painter, iconRect); break;
    case 3: drawRectIcon(painter, iconRect); break;
    case 4: drawEllipseIcon(painter, iconRect); break;
    case 5: drawArrowIcon(painter, iconRect); break;
    case 6: drawTextIcon(painter, iconRect); break;
    case 7: drawMosaicIcon(painter, iconRect); break;
    case 8: drawUndoIcon(painter, iconRect); break;
    case 9: drawRedoIcon(painter, iconRect); break;
    }

    painter.restore();
}

void ToolbarWidget::drawCopyIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.3);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QRect back(r.left() + 3, r.top(), r.width() - 3, r.height() - 3);
    QRect front(r.left(), r.top() + 3, r.width() - 3, r.height() - 3);
    painter.drawRect(back);
    painter.setBrush(QColor(48, 48, 48, 230));
    painter.drawRect(front);
}

void ToolbarWidget::drawSaveIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.3);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r.adjusted(1, 1, -1, -1));
    painter.drawRect(r.left() + 4, r.top() + 1, r.width() - 8, 5);
    painter.drawRect(r.left() + 3, r.bottom() - 5, r.width() - 6, 5);
}

void ToolbarWidget::drawCancelIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 100, 100), 1.8);
    painter.setPen(pen);
    int m = 3;
    painter.drawLine(r.left() + m, r.top() + m, r.right() - m, r.bottom() - m);
    painter.drawLine(r.right() - m, r.top() + m, r.left() + m, r.bottom() - m);
}

void ToolbarWidget::drawRectIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.5);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r.adjusted(2, 3, -2, -3));
}

void ToolbarWidget::drawEllipseIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.5);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.drawEllipse(r.adjusted(1, 2, -1, -2));
    painter.setRenderHint(QPainter::Antialiasing, false);
}

void ToolbarWidget::drawArrowIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.5);
    painter.setPen(pen);
    painter.drawLine(r.left() + 2, r.bottom() - 2, r.right() - 3, r.top() + 3);
    painter.drawLine(r.right() - 3, r.top() + 3, r.right() - 3, r.top() + 7);
    painter.drawLine(r.right() - 3, r.top() + 3, r.right() - 7, r.top() + 3);
}

void ToolbarWidget::drawTextIcon(QPainter& painter, const QRect& r) const
{
    QFont font("Arial", 11, QFont::Bold);
    painter.setFont(font);
    painter.setPen(QColor(220, 220, 220));
    painter.drawText(r, Qt::AlignCenter, "T");
}

void ToolbarWidget::drawMosaicIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.0);
    painter.setPen(pen);
    int step = 4;
    for (int x = r.left() + 2; x < r.right() - 1; x += step) {
        for (int y = r.top() + 2; y < r.bottom() - 1; y += step) {
            if (((x - r.left()) / step + (y - r.top()) / step) % 2 == 0) {
                painter.fillRect(x, y, step - 1, step - 1, QColor(220, 220, 220));
            }
        }
    }
}

void ToolbarWidget::drawUndoIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.5);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(r.left() + 3, r.center().y());
    path.cubicTo(r.left() + 3, r.top() + 2,
                 r.right() - 3, r.top() + 2,
                 r.right() - 3, r.center().y());
    painter.drawPath(path);
    painter.drawLine(r.left() + 3, r.center().y(), r.left() + 6, r.center().y() - 3);
    painter.drawLine(r.left() + 3, r.center().y(), r.left() + 6, r.center().y() + 3);
}

void ToolbarWidget::drawRedoIcon(QPainter& painter, const QRect& r) const
{
    QPen pen(QColor(220, 220, 220), 1.5);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(r.right() - 3, r.center().y());
    path.cubicTo(r.right() - 3, r.top() + 2,
                 r.left() + 3, r.top() + 2,
                 r.left() + 3, r.center().y());
    painter.drawPath(path);
    painter.drawLine(r.right() - 3, r.center().y(), r.right() - 6, r.center().y() - 3);
    painter.drawLine(r.right() - 3, r.center().y(), r.right() - 6, r.center().y() + 3);
}

int ToolbarWidget::buttonAtPos(const QPoint& pos) const
{
    for (int i = 0; i < m_buttons.size(); i++) {
        if (m_buttons[i].rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

void ToolbarWidget::mouseMoveEvent(QMouseEvent* event)
{
    int idx = buttonAtPos(event->pos());
    if (idx != m_hoveredIndex) {
        m_hoveredIndex = idx;
        if (idx >= 0 && m_buttons[idx].enabled) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        update();
    }
}

void ToolbarWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int idx = buttonAtPos(event->pos());
        if (idx >= 0 && m_buttons[idx].enabled) {
            m_pressedIndex = idx;
            update();
        }
    }
}

void ToolbarWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_pressedIndex >= 0) {
        int idx = buttonAtPos(event->pos());
        if (idx == m_pressedIndex && m_buttons[idx].enabled) {
            int iconType = m_buttons[idx].iconType;

            if (m_buttons[idx].toggleable) {
                AnnotationTool tool = toolForIconType(iconType);
                bool wasToggled = m_buttons[idx].toggled;
                for (int i = 0; i < m_buttons.size(); i++) {
                    if (m_buttons[i].toggleable) m_buttons[i].toggled = false;
                }
                if (!wasToggled) {
                    m_buttons[idx].toggled = true;
                    showStylePanel(tool);
                    emit toolSelected(tool);
                } else {
                    showStylePanel(AnnotationTool::None);
                    emit toolSelected(AnnotationTool::None);
                }
            } else {
                switch (iconType) {
                case 0: emit copyClicked(); break;
                case 1: showSaveMenu(idx); break;
                case 2: emit cancelClicked(); break;
                case 8: emit undoClicked(); break;
                case 9: emit redoClicked(); break;
                }
            }
        }
        m_pressedIndex = -1;
        update();
    }
}

void ToolbarWidget::leaveEvent(QEvent* event)
{
    Q_UNUSED(event)
    m_hoveredIndex = -1;
    m_pressedIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
}

void ToolbarWidget::showStylePanel(AnnotationTool tool)
{
    if (tool == AnnotationTool::None) {
        if (m_stylePanel) m_stylePanel->hide();
        return;
    }

    if (!m_stylePanel) {
        m_stylePanel = new StylePanelWidget(nullptr);
    }

    // Position below toolbar
    QPoint panelPos(pos().x(), pos().y() + height() + 4);
    m_stylePanel->showForTool(tool, panelPos);
}

void ToolbarWidget::showSaveMenu(int buttonIndex)
{
    if (!m_saveMenu) {
        m_saveMenu = new SaveMenuWidget(nullptr);
        connect(m_saveMenu, &SaveMenuWidget::saveToDesktop, this, &ToolbarWidget::saveToDesktopClicked);
        connect(m_saveMenu, &SaveMenuWidget::saveToFolder, this, &ToolbarWidget::saveToFolderClicked);
    }

    QRect btnRect = m_buttons[buttonIndex].rect;
    QPoint menuPos = mapToGlobal(QPoint(btnRect.left(), btnRect.bottom() + 4));
    m_saveMenu->showAt(menuPos);
}

} // namespace easyshotter
