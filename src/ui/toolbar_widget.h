#pragma once

#include <QWidget>
#include <QVector>

namespace easyshotter {

class ToolbarWidget : public QWidget {
    Q_OBJECT
public:
    explicit ToolbarWidget(QWidget* parent = nullptr);
    ~ToolbarWidget() override;

    void showNearRect(const QRect& selectionRect);

signals:
    void copyClicked();
    void saveClicked();
    void cancelClicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct ButtonInfo {
        QRect rect;
        QString text;
        int iconType; // 0=copy, 1=save, 2=cancel, 3+=reserved
        bool enabled;
    };

    void layoutButtons();
    void drawButton(QPainter& painter, const ButtonInfo& btn, bool hovered, bool pressed) const;
    void drawCopyIcon(QPainter& painter, const QRect& iconRect) const;
    void drawSaveIcon(QPainter& painter, const QRect& iconRect) const;
    void drawCancelIcon(QPainter& painter, const QRect& iconRect) const;
    void drawPenIcon(QPainter& painter, const QRect& iconRect) const;
    void drawMosaicIcon(QPainter& painter, const QRect& iconRect) const;
    void drawArrowIcon(QPainter& painter, const QRect& iconRect) const;
    void drawTextIcon(QPainter& painter, const QRect& iconRect) const;
    void drawUndoIcon(QPainter& painter, const QRect& iconRect) const;
    void drawRedoIcon(QPainter& painter, const QRect& iconRect) const;
    int buttonAtPos(const QPoint& pos) const;

    static constexpr int kButtonWidth = 32;
    static constexpr int kButtonHeight = 28;
    static constexpr int kButtonSpacing = 2;
    static constexpr int kPadding = 6;
    static constexpr int kSeparatorWidth = 8;
    static constexpr int kCornerRadius = 5;

    QVector<ButtonInfo> m_buttons;
    int m_hoveredIndex = -1;
    int m_pressedIndex = -1;
};

} // namespace easyshotter
