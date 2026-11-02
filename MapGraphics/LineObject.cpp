#include "LineObject.h"

#include "guts/Conversions.h"
#include <QtGlobal>
#include <QtMath>

#include "fpocommon/h/fpo/fpo_common/alsr_lafi.h"

LineObject::LineObject(
                       const Position &endA,
                       const Position &endB,
                       qreal thickness,
                       MapGraphicsObject *parent) :
    MapGraphicsObject(true, parent),
    _a(endA), _b(endB)
{
    _thickness = qBound<qreal>(0.0, thickness, 300.0);
    this->updatePositionFromEndPoints();
}

LineObject::~LineObject()
{
}

//pure-virtual from MapGraphicsObject
QRectF LineObject::boundingRect() const
{
    const qreal avgLat = (_a.latitude() + _b.latitude()) / 2.0;
    const qreal lonPerMeter = Conversions::degreesLonPerMeter(avgLat);
    const qreal latPerMeter = Conversions::degreesLatPerMeter(avgLat);

    const qreal widthLon = qAbs<qreal>(_a.longitude() - _b.longitude());
    const qreal heightlat = qAbs<qreal>(_a.latitude() - _b.latitude());

    const qreal widthMeters = qMax<qreal>(widthLon / lonPerMeter, 5.0);
    const qreal heightMeters = qMax<qreal>(heightlat / latPerMeter, 5.0);

    const QRectF toRet(-1.0 * widthMeters,
                       -1.0 * heightMeters,
                       2.0 * widthMeters,
                       2.0 * heightMeters);

    return toRet;
}

//pure-virtual from MapGraphicsObject
void LineObject::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing, true);
    QPen pen = painter->pen();
    pen.setWidthF(_thickness);
    pen.setColor(_fillColor);
    painter->setPen(pen);

    const qreal avgLat = (_a.latitude() + _b.latitude()) / 2.0;
    const qreal avgLon = (_a.longitude() + _b.longitude()) / 2.0;
    //const qreal avgLat = (latAA + lonAA) / 2.0;
    const qreal lonPerMeter = Conversions::degreesLonPerMeter(avgLon);
    const qreal latPerMeter = Conversions::degreesLatPerMeter(avgLat);

    QPointF center = this->pos();

    const QPointF offsetA = _a.lonLat() - center;
    const QPointF offsetB = _b.lonLat() - center;

//    const QPointF metersA(offsetA.x() / lonPerMeter,
//                          offsetA.y() / latPerMeter);
    const QPointF metersA(0,0);
    const QPointF metersB(offsetB.x() / lonPerMeter,
                          offsetB.y() / latPerMeter);

    QPointF enu = Conversions::lla2enu(_b,_a).toPointF();


    painter->drawLine(metersA, enu);
}

qreal LineObject::thickness() const
{
    return _thickness;
}

void LineObject::setThickness(qreal nThick)
{
    _thickness = qBound<qreal>(0.0, nThick, 5000.0);
    this->redrawRequested();
}

void LineObject::setColor(QColor fillColor)
{
    _fillColor = fillColor;
    this->redrawRequested();
}

void LineObject::setLen(double len)
{
    _len = len;
}

//public slot
void LineObject::setEndPointA()//const Position &pos)
{
    _a.setLatitude(ref->latitude());
    _a.setLongitude(ref->longitude());
    _b.setLatitude(ref->latitude()-_len);
    _b.setLongitude(ref->longitude());
    this->setRotation(ref->rotation() + angShift);
    this->updatePositionFromEndPoints();
    this->redrawRequested();
}

void LineObject::setShiftPoint(void)
{
    LAFI_DOUBLE lafid;
    LafiD(0, _len, 0, qDegreesToRadians(ref->rotation()), qDegreesToRadians(ref->latitude()), qDegreesToRadians(ref->longitude()), &lafid);

    _a.setLatitude(qRadiansToDegrees(lafid.fi));
    _a.setLongitude(qRadiansToDegrees(lafid.la));

    LafiD(-0.25, _len, 0, qDegreesToRadians(ref->rotation()), qDegreesToRadians(ref->latitude()), qDegreesToRadians(ref->longitude()), &lafid);
    _b.setLatitude(qRadiansToDegrees(lafid.fi));
    _b.setLongitude(qRadiansToDegrees(lafid.la));

    updatePositionFromEndPoints();
    //this->updatePositionFromEndPoints();
    redrawRequested();
}

void LineObject::setShiftPoint2(int len)
{
    LAFI_DOUBLE lafid;
    LafiD(0, len, 0, qDegreesToRadians(ref->rotation()), qDegreesToRadians(ref->latitude()), qDegreesToRadians(ref->longitude()), &lafid);

    _a.setLatitude(qRadiansToDegrees(lafid.fi));
    _a.setLongitude(qRadiansToDegrees(lafid.la));

    LafiD(-0.25, len, 0, qDegreesToRadians(ref->rotation()), qDegreesToRadians(ref->latitude()), qDegreesToRadians(ref->longitude()), &lafid);
    _b.setLatitude(qRadiansToDegrees(lafid.fi));
    _b.setLongitude(qRadiansToDegrees(lafid.la));

    updatePositionFromEndPoints();
    //this->updatePositionFromEndPoints();
    redrawRequested();
}

void LineObject::zoomChanged(quint8 nZoom)
{
    //_zoom = nZoom;
}

//public slot
void LineObject::setEndPointB(const Position &pos)
{
    _b = pos;
    this->updatePositionFromEndPoints();
    this->redrawRequested();
}

//public slot
void LineObject::setEndPoints(const Position &a,
                              const Position &b)
{
    _a = a;
    _b = b;
    this->updatePositionFromEndPoints();
    this->redrawRequested();
}

//private slot
void LineObject::updatePositionFromEndPoints()
{
    const qreal avgLon = _a.longitude() ;
    const qreal avgLat = _a.latitude() ;
    this->setPos(QPointF(avgLon, avgLat));
}
