#pragma once

#include <QRect>
#include <QPoint>
#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QFont>
#include <vector>
#include <memory>

namespace simpleshotter {

enum class AnnotationTool {
    None,
    Rectangle,
    Ellipse,
    Arrow,
    Text,
    Mosaic
};

class AnnotationItem {
public:
    virtual ~AnnotationItem() = default;
    virtual void draw(QPainter& painter) const = 0;
    virtual void drawToPixmap(QPainter& painter) const { draw(painter); }
    virtual AnnotationItem* clone() const = 0;
};

class RectAnnotation : public AnnotationItem {
public:
    QRect rect;
    QColor color{255, 0, 0};
    int penWidth = 2;

    void draw(QPainter& painter) const override;
    AnnotationItem* clone() const override { return new RectAnnotation(*this); }
};

class EllipseAnnotation : public AnnotationItem {
public:
    QRect rect;
    QColor color{255, 0, 0};
    int penWidth = 2;

    void draw(QPainter& painter) const override;
    AnnotationItem* clone() const override { return new EllipseAnnotation(*this); }
};

class ArrowAnnotation : public AnnotationItem {
public:
    QPoint startPos;
    QPoint endPos;
    QColor color{255, 0, 0};
    int penWidth = 2;

    void draw(QPainter& painter) const override;
    AnnotationItem* clone() const override { return new ArrowAnnotation(*this); }
};

class TextAnnotation : public AnnotationItem {
public:
    QPoint pos;
    QString text;
    QColor color{255, 0, 0};
    QFont font{QString(), 14};

    void draw(QPainter& painter) const override;
    AnnotationItem* clone() const override { return new TextAnnotation(*this); }
};

class MosaicAnnotation : public AnnotationItem {
public:
    std::vector<QPoint> points;
    int brushRadius = 10;
    int blockSize = 8;
    QPixmap sourcePixmap;

    void setSource(const QPixmap& pixmap);
    void addPoint(const QPoint& pt);
    void draw(QPainter& painter) const override;
    void drawToPixmap(QPainter& painter) const override;
    AnnotationItem* clone() const override;

private:
    QImage m_cachedImage;
};

class AnnotationManager {
public:
    void addItem(std::unique_ptr<AnnotationItem> item);
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void clear();

    void drawAll(QPainter& painter) const;

    const std::vector<std::unique_ptr<AnnotationItem>>& items() const { return m_items; }

private:
    std::vector<std::unique_ptr<AnnotationItem>> m_items;
    std::vector<std::unique_ptr<AnnotationItem>> m_redoStack;
};

} // namespace simpleshotter
