#include "aircraft.h"

#include <QtDebug>
#include <QKeyEvent>

AircraftObject::AircraftObject(qreal radius,bool sizeIsZoomInvariant, QColor fillColor, MapGraphicsObject *parent) :
    MapGraphicsObject(sizeIsZoomInvariant,parent), _fillColor(fillColor)
{
    _radius = qMax<qreal>(radius,0.01);

    this->setFlag(MapGraphicsObject::ObjectIsSelectable);
    this->setFlag(MapGraphicsObject::ObjectIsMovable);
    this->setFlag(MapGraphicsObject::ObjectIsFocusable);
}

AircraftObject::~AircraftObject()
{
}

QRectF AircraftObject::boundingRect() const
{
    return QRectF(-1*_radius,
                  -1*_radius,
                  2*_radius,
                  2*_radius);
}




QRect rect;
QPoint aircraftPoint;
void AircraftObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{

    Q_UNUSED(option)
    Q_UNUSED(widget)
    painter->setRenderHint(QPainter::Antialiasing,true);
//    painter->setBrush(_fillColor);
//    painter->drawEllipse(QPointF(0,0),
//                         _radius,
//                         _radius);

//    QRectF target(0,0, _radius, _radius);
//    QRectF source(0,0, _radius-5, _radius-5);
    QPixmap image(":/icons/images/planeWind.png");
    //this->setRotation(rotate);
    //painter->rotate(this->rotation());

    rect = this->boundingRect().toRect();
    painter->drawPixmap(rect,image);

    aircraftPoint = painter->brushOrigin();
    ref->setLatitude(this->latitude());
    ref->setLongitude(this->longitude());

}

qreal AircraftObject::radius() const
{
    return _radius;
}

void AircraftObject::setRadius(qreal radius)
{
    _radius = radius;
    this->redrawRequested();
}

//protected
//virtual from MapGraphicsObject
void AircraftObject::keyReleaseEvent(QKeyEvent *event)
{
    double rotate = 10;
    if (event->matches(QKeySequence::Delete))
    {
        this->deleteLater();
        event->accept();
    }
    else if (event->key() == Qt::Key_E)
    {
        rotate = this->rotation() + 5;
        this->setRotation(rotate);
        //this->redrawRequested();
    }
    else if (event->key() == Qt::Key_Q)
    {
        rotate = this->rotation() - 5;
        this->setRotation(rotate);
        this->redrawRequested();
    }
    else
        event->ignore();
}


//void AircraftObject::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
//{
//    if (event->button() != Qt::RightButton)
//    {

//    }
//    else
//    {

//    }
//}
