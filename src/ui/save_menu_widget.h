#pragma once

#include <QWidget>
#include <QRect>
#include <QVector>

namespace easyshotter {

class SaveMenuWidget : public QWidget {
    Q_OBJECT
public:
    explicit SaveMenuWidget(QWidget* parent = nullptr);
    ~SaveMenuWidget() override;

    void showAt(const QPoint& screenPos);

signals:
    void saveToDesktop();
    void saveToFolder();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    struct MenuItem {
        QRect rect;
        QString text;
    };

    void layoutItems();
    int itemAtPos(const QPoint& pos) const;

    static constexpr int kItemHeight = 30;
    static constexpr int kItemPadding = 8;
    static constexpr int kCornerRadius = 4;

    QVector<MenuItem> m_items;
    int m_hoveredIndex = -1;
    int m_pressedIndex = -1;
};

} // namespace easyshotter
