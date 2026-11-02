#ifndef LINEOBJECT_H
#define LINEOBJECT_H

#include "MapGraphics_global.h"
#include "Position.h"
#include "MapGraphicsObject.h"

class MAPGRAPHICSSHARED_EXPORT LineObject : public MapGraphicsObject
{
    Q_OBJECT
public:
    explicit LineObject(const Position& endA,
                        const Position& endB,
                        qreal thickness = 0.0,
                        MapGraphicsObject *parent = 0);
    virtual ~LineObject();

    //pure-virtual from MapGraphicsObject
    QRectF boundingRect() const;

    //pure-virtual from MapGraphicsObject
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    qreal thickness() const;
    void setThickness(qreal nThick);
    void setColor(QColor fillColor);
    void setLen(double);

    MapGraphicsObject* ref;
    double angShift = 0;
    uint8_t* _zoom;
signals:
    
public slots:
    //void rotationChanged(const Position& pos);
    //void rotationChanged();
    void setEndPointA();
    void setShiftPoint(void);
    void setShiftPoint2(int len);
    void setEndPointB(const Position& pos);
    void setEndPoints(const Position& a,
                      const Position& b);
    void zoomChanged(quint8 nZoom);

private slots:
    void updatePositionFromEndPoints();

private:
    Position _a;
    Position _b;
    qreal _thickness;
    QColor _fillColor;
    double _len;

};

#endif // LINEOBJECT_H
