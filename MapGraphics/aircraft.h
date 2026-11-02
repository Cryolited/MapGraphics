#ifndef AIRCRAFTOBJECT_H
#define AIRCRAFTOBJECT_H

#include "MapGraphics_global.h"
#include "MapGraphicsObject.h"
#include "Position.h"

class MAPGRAPHICSSHARED_EXPORT AircraftObject : public MapGraphicsObject
{
    Q_OBJECT
public:
    explicit AircraftObject(qreal radius,bool sizeIsZoomInvariant=true, QColor fillColor = QColor(0,0,0,0),MapGraphicsObject *parent = 0);
    virtual ~AircraftObject();

    //pure-virtual from MapGraphicsObject
    QRectF boundingRect() const;

    //pure-virtual from MapGraphicsObject
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    qreal radius() const;
    void setRadius(qreal radius);

    Position* ref;
    
signals:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    //QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value);
    void keyReleaseEvent2(QKeyEvent *event);
    // keyReleaseEvent(QKeyEvent *event);
    void test();

public slots:


protected:
    //virtual from MapGraphicsObject
    virtual void keyReleaseEvent(QKeyEvent *event);

private:
    qreal _radius;
    QColor _fillColor;
    
};

#endif // AIRCRAFTOBJECT_H
