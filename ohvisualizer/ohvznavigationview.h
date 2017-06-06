#ifndef OHVZNAVIGATIONVIEW_H
#define OHVZNAVIGATIONVIEW_H

#include <QWidget>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>

class QGraphicsScene;
class QGraphicsView;
class ohvzModel;

class CustomElipse : public QGraphicsEllipseItem
{
public:
    CustomElipse (const QRectF& rect) : QGraphicsEllipseItem(rect) {
        setFlag(QGraphicsItem::ItemIsMovable);
        setFlag(QGraphicsItem::ItemSendsScenePositionChanges);
    }

    void addLine(QGraphicsLineItem *line, bool isPoint1) {
        this->line = line;
        isP1 = isPoint1;
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value)
    {
        if (change == ItemPositionChange && scene()) {
            // value is the new position.
            QPointF newPos = value.toPointF();

            moveLineToCenter(newPos);
        }
        return QGraphicsItem::itemChange(change, value);
    }

    void moveLineToCenter(QPointF newPos) {
        // Converts the elipse position (top-left)
        // to its center position
        int xOffset = rect().x() + rect().width()/2;
        int yOffset = rect().y() + rect().height()/2;

        QPointF newCenterPos = QPointF(newPos.x() + xOffset, newPos.y() + yOffset);

        // Move the required point of the line to the center of the elipse
        QPointF p1 = isP1 ? newCenterPos : line->line().p1();
        QPointF p2 = isP1 ? line->line().p2() : newCenterPos;

        line->setLine(QLineF(p1, p2));
    }

private:
    QGraphicsLineItem *line;
    bool isP1;
};



class ohvzNavigationView : public QWidget
{
    Q_OBJECT
public:
    explicit ohvzNavigationView(ohvzModel *model, QWidget *parent = 0);

    void update();

    ohvzModel *m_model;
    QGraphicsScene *m_scene;
    QGraphicsView *m_view;

signals:

public slots:
};

#endif // OHVZNAVIGATIONVIEW_H
