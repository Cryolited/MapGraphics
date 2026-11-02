#include "SectorObject.h"

#include "guts/Conversions.h"

#include <QPainter>
#include <QtGlobal>
#include <QtMath>

#include "fpocommon/h/fpo/fpo_common/alsr_lafi.h"

//азимут приводится к [0,360) перед LafiD: ref->rotation() может накапливаться
//за пределы 360 (клавиши E/Q) или уходить в минус
static qreal normalizedBearing(qreal deg)
{
    deg = fmod(deg,360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg;
}

//азимут приводится к (-180,180] - для отклонения полосы от биссектрисы
static qreal wrap180(qreal deg)
{
    deg = fmod(deg + 180.0,360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg - 180.0;
}

//перекрестие в середине сектора: полудлина плеча и толщина линии в экранных
//пикселях (не зависят от масштаба карты)
static const qreal CenterCrossHalfPx = 5.5;
static const qreal CenterCrossPenPx = 1.5;

//толщина линий границ сектора (дуги и боковые кромки) в экранных пикселях
static const qreal BorderPenPx = 1.5;

//дуги сектора рисуются ломаной из точек по азимутам; число полос между
//границами: не меньше минимума, чтобы затухание было плавным на узких
//секторах, и не больше максимума, чтобы не плодить точки на широких
static const int MinArcStrips = 24;
static const int MaxArcStrips = 360;

SectorObject::SectorObject(qreal nearRange,
                           qreal farRange,
                           qreal centerAngleFromAbeam,
                           qreal sectorWidth,
                           bool sizeIsZoomInvariant,
                           QColor fillColor,
                           QColor borderColor,
                           bool fillVisible,
                           MapGraphicsObject *parent) :
    MapGraphicsObject(sizeIsZoomInvariant,parent),
    ref(0),
    _nearRange(qMax<qreal>(nearRange,0.0)),
    _farRange(qMax<qreal>(farRange,qMax<qreal>(nearRange,0.0))),
    _centerAngleFromAbeam(centerAngleFromAbeam),
    _sectorWidth(8.0),
    _edgeFadeFraction(0.4),
    _fillColor(fillColor),
    _borderColor(borderColor),
    _fillVisible(fillVisible),
    _centerCrossVisible(true),
    _refPos(0.0,0.0),
    _centerBearingDeg(0.0),
    _maxRadius(0.0),
    _centerPt(0.0,0.0),
    _positionValid(false)
{
    this->setSectorWidth(sectorWidth);
}

SectorObject::~SectorObject()
{
}

//pure-virtual from MapGraphicsObject
QRectF SectorObject::boundingRect() const
{
    if (!_positionValid)
        return QRectF(-1.0,-1.0,2.0,2.0);

    //прямоугольник обязан быть центрирован в (0,0) - см. PrivateQGraphicsObject
    const qreal margin = qMax<qreal>(5.0,0.02*_maxRadius);

    return QRectF(-1.0*(_maxRadius+margin),
                  -1.0*(_maxRadius+margin),
                  2.0*(_maxRadius+margin),
                  2.0*(_maxRadius+margin));
}

//pure-virtual from MapGraphicsObject
void SectorObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!_positionValid)
        return;

    painter->setRenderHint(QPainter::Antialiasing,true);

    //Затухание к боковым границам: сектор разбит на узкие полосы по азимуту,
    //каждая полоса красится своим альфа-каналом (0 у границы, полный цвет
    //в середине). Раньше тут был QConicalGradient, но он, как и arcTo,
    //меряет углы в своей экранной конвенции и давал зеркальную картинку.
    //Остаются только точки - они рисуются так же, как кромки и перекрестие.
    //В контурном режиме (_fillVisible == false) заливка пропускается.
    if (_fillVisible)
    {
        const int nStrips = _outerPts.size() - 1;
        const qreal halfWidth = 0.5*_sectorWidth;
        const qreal step = _sectorWidth/nStrips;
        const qreal fade = qBound<qreal>(0.0,_edgeFadeFraction,0.9)*halfWidth;

        QColor stripColor(_fillColor);
        painter->setPen(Qt::NoPen);
        for (int i = 0; i < nStrips; ++i)
        {
            const qreal bMid = _centerBearingDeg - halfWidth + (i + 0.5)*step;
            qreal alphaScale = 1.0;
            if (fade > 0.0)
            {
                const qreal off = qAbs<qreal>(wrap180(bMid - _centerBearingDeg));
                alphaScale = qBound<qreal>(0.0,(halfWidth - off)/fade,1.0);
            }
            stripColor.setAlpha(qRound(_fillColor.alpha()*alphaScale));
            painter->setBrush(stripColor);

            QPolygonF strip;
            strip << _innerPts[i] << _outerPts[i] << _outerPts[i+1] << _innerPts[i+1];
            painter->drawPolygon(strip);
        }
    }

    this->drawBorder(painter);

    if (_centerCrossVisible)
        this->drawCenterCross(painter);
}

void SectorObject::updateObjectData(QString country, int value, int max, int min)
{
    Q_UNUSED(country)
    Q_UNUSED(value)
    Q_UNUSED(max)
    Q_UNUSED(min)
}

qreal SectorObject::nearRange() const
{
    return _nearRange;
}

void SectorObject::setNearRange(qreal nearRange)
{
    _nearRange = qMax<qreal>(nearRange,0.0);
    if (_farRange < _nearRange)
        _farRange = _nearRange;
    if (_positionValid)
        this->updateGeometry();
    this->redrawRequested();
}

qreal SectorObject::farRange() const
{
    return _farRange;
}

void SectorObject::setFarRange(qreal farRange)
{
    _farRange = qMax<qreal>(farRange,_nearRange);
    if (_positionValid)
        this->updateGeometry();
    this->redrawRequested();
}

qreal SectorObject::centerAngleFromAbeam() const
{
    return _centerAngleFromAbeam;
}

void SectorObject::setCenterAngleFromAbeam(qreal angleFromAbeam)
{
    _centerAngleFromAbeam = angleFromAbeam;
    if (_positionValid)
        this->updateGeometry();
    this->redrawRequested();
}

qreal SectorObject::sectorWidth() const
{
    return _sectorWidth;
}

void SectorObject::setSectorWidth(qreal width)
{
    if (width <= 0.0)
        width = 8.0;    //ширина не задана - 8 градусов
    _sectorWidth = qBound<qreal>(0.5,width,359.0);
    if (_positionValid)
        this->updateGeometry();
    this->redrawRequested();
}

qreal SectorObject::edgeFadeFraction() const
{
    return _edgeFadeFraction;
}

void SectorObject::setEdgeFadeFraction(qreal fraction)
{
    _edgeFadeFraction = qBound<qreal>(0.0,fraction,0.9);
    this->redrawRequested();
}

void SectorObject::setColor(QColor fillColor)
{
    _fillColor = fillColor;
    this->redrawRequested();
}

QColor SectorObject::borderColor() const
{
    return _borderColor;
}

void SectorObject::setBorderColor(QColor borderColor)
{
    _borderColor = borderColor;
    this->redrawRequested();
}

bool SectorObject::fillVisible() const
{
    return _fillVisible;
}

void SectorObject::setFillVisible(bool visible)
{
    _fillVisible = visible;
    this->redrawRequested();
}

bool SectorObject::centerCrossVisible() const
{
    return _centerCrossVisible;
}

void SectorObject::setCenterCrossVisible(bool visible)
{
    _centerCrossVisible = visible;
    this->redrawRequested();
}

//public slot
void SectorObject::setPosSector(void)
{
    if (ref == 0)
        return;

    this->setPos(QPointF(ref->longitude(),ref->latitude()));
    this->updateGeometry();
    this->redrawRequested();
}

//точка на азимуте bearingDeg (градусы, по часовой от севера) на дальности range
//от текущего положения самолёта
Position SectorObject::pointAtBearing(qreal bearingDeg, qreal range) const
{
    bearingDeg = normalizedBearing(bearingDeg);

    LAFI_DOUBLE lafid;
    LafiD(0,range,0,
          qDegreesToRadians(bearingDeg),
          qDegreesToRadians(_refPos.latitude()),
          qDegreesToRadians(_refPos.longitude()),
          &lafid);

    return Position(qRadiansToDegrees(lafid.la),   //longitude
                    qRadiansToDegrees(lafid.fi));  //latitude
}

//смещение точки в метрах ENU относительно самолёта
QPointF SectorObject::enuOffset(const Position &pos) const
{
    return Conversions::lla2enu(pos,_refPos).toPointF();
}

void SectorObject::updateGeometry(void)
{
    _positionValid = false;
    _maxRadius = 0.0;
    if (ref == 0)
        return;

    _refPos = Position(ref->longitude(),ref->latitude());

    //угол центра сектора отсчитывается от КУРСА по часовой стрелке:
    //0 - нос, 90 - правый траверз, 180 - хвост, 270 - левый траверз
    _centerBearingDeg = normalizedBearing(ref->rotation() + _centerAngleFromAbeam);
    const qreal halfWidth = _sectorWidth/2.0;

    //Дуги сектора строятся ломаной из точек, взятых по азимутам через LafiD.
    //Раньше путь собирался через QPainterPath::arcTo, но Qt меряет углы дуг
    //против часовой в ЭКРАННЫХ координатах (точка угла - (cos,-sin) в
    //логических координатах), а наши точки ENU лежат в системе с y на север.
    //Дуги зеркалились относительно прямых кромок, путь замыкался
    //самопересечением, и заливка (OddEven) исчезала на части курсов и
    //разворачивалась в противофазу с поворотом. Ломаная из готовых точек
    //никаких угловых конвенций не использует и рисуется тем же механизмом,
    //что кромки и перекрестие (у LineObject сырые точки ENU рисуются верно).
    //число полос делаем чётным: тогда средняя вершина ломаной стоит ровно
    //на биссектрисе - она и берётся точкой перекрестия
    int nStrips = qBound(MinArcStrips,qCeil(_sectorWidth),MaxArcStrips);
    if (nStrips % 2 != 0)
        ++nStrips;
    const int nPts = nStrips + 1;
    const qreal step = _sectorWidth/nStrips;

    QVector<QPointF> outerPts(nPts);
    QVector<QPointF> innerPts(nPts);

    for (int i = 0; i < nPts; ++i)
    {
        const qreal bearing = _centerBearingDeg - halfWidth + i*step;
        outerPts[i] = this->enuOffset(this->pointAtBearing(bearing,_farRange));
        innerPts[i] = (_nearRange > 0.0)
                      ? this->enuOffset(this->pointAtBearing(bearing,_nearRange))
                      : QPointF(0.0,0.0);

        //LafiD вернул нечисловой результат - не рисуем, иначе сектор пропадает
        //вместе с мусорной геометрией
        if (!qIsFinite(outerPts[i].x()) || !qIsFinite(outerPts[i].y()) ||
            !qIsFinite(innerPts[i].x()) || !qIsFinite(innerPts[i].y()))
            return;
    }

    _outerPts = outerPts;
    _innerPts = innerPts;

    //середина сектора - точка перекрестия (и якорь будущей подписи сектора):
    //средняя вершина ломаных дуг, т.е. центр клина ПО ПОСТРОЕНИЮ - куда бы
    //ни сместился или ни повернулся клин, перекрестие переезжает вместе с ним
    const int midIdx = nStrips/2;
    _centerPt = 0.5*(innerPts[midIdx] + outerPts[midIdx]);

    for (int i = 0; i < nPts; ++i)
    {
        const qreal r = qSqrt(outerPts[i].x()*outerPts[i].x() + outerPts[i].y()*outerPts[i].y());
        _maxRadius = qMax<qreal>(_maxRadius,r);
    }

    _positionValid = true;
}

//маленькое перекрестие в середине сектора постоянного размера на экране:
//белая подложка под цветным крестом, чтобы было видно на любом фоне карты.
//Рисуется в ЛОКАЛЬНЫХ координатах тем же преобразованием, что и заливка
//(клин): вариант с deviceTransform()/resetTransform() и рисованием в
//пикселях устройства даёт смещение - у MapGraphicsObject преобразования
//накладываются обёрткой PrivateQGraphicsObject, и "device-координаты"
//внутри paint() не совпадают с экранными. Постоянный размер на экране
//держим компенсацией масштаба painter'а: плечо = CenterCrossHalfPx/масштаб.
void SectorObject::drawCenterCross(QPainter *painter) const
{
    qreal scale = qAbs<qreal>(painter->transform().m11());
    if (!qIsFinite(scale) || scale < 1e-12)
        scale = qAbs<qreal>(painter->transform().m22());
    if (!qIsFinite(scale) || scale < 1e-12)
        scale = 1.0;

    const qreal half = CenterCrossHalfPx/scale;
    const QPointF left(_centerPt.x() - half, _centerPt.y());
    const QPointF right(_centerPt.x() + half, _centerPt.y());
    const QPointF top(_centerPt.x(), _centerPt.y() - half);
    const QPointF bottom(_centerPt.x(), _centerPt.y() + half);

    painter->save();

    QPen haloPen(Qt::white, (CenterCrossPenPx + 1.7)/scale);
    haloPen.setCapStyle(Qt::FlatCap);
    painter->setPen(haloPen);
    painter->drawLine(left,right);
    painter->drawLine(top,bottom);

    QPen corePen(_fillColor);
//    corePen.setAlpha(255);
    corePen.setWidthF(CenterCrossPenPx/scale);
    corePen.setCapStyle(Qt::FlatCap);
    painter->setPen(corePen);
    painter->drawLine(left,right);
    painter->drawLine(top,bottom);

    painter->restore();
}

//границы сектора: обе дуги и две боковые кромки. Цвет - _borderColor; если он
//не задан (invalid QColor), в контурном режиме берётся цвет заливки с полной
//непрозрачностью (иначе "только границы" было бы пустым экраном), в обычном
//режиме границ нет - вид старых секторов не меняется. Толщина в экранных
//пикселях держится компенсацией масштаба painter'а - как у перекрестия
//(см. комментарий в drawCenterCross: device-координаты внутри paint()
//не совпадают с экранными, поэтому рисуем в локальных).
void SectorObject::drawBorder(QPainter *painter) const
{
    QColor borderColor = _borderColor;
    if (!borderColor.isValid())
    {
        if (!_fillVisible)
        {
            borderColor = _fillColor;
            borderColor.setAlpha(255);
        }
        else
            return;
    }
    if (borderColor.alpha() <= 0)
        return;

    qreal scale = qAbs<qreal>(painter->transform().m11());
    if (!qIsFinite(scale) || scale < 1e-12)
        scale = qAbs<qreal>(painter->transform().m22());
    if (!qIsFinite(scale) || scale < 1e-12)
        scale = 1.0;

    QPen pen(borderColor, BorderPenPx/scale);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);

    painter->save();
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);

    //дальняя дуга и боковые кромки (при nearRange == 0 кромки сходятся
    //в вершину сектора - точку самолёта)
    painter->drawPolyline(_outerPts);
    painter->drawLine(_outerPts.first(), _innerPts.first());
    painter->drawLine(_outerPts.last(), _innerPts.last());

    //ближняя дуга есть только у кольцевого сектора
    if (_nearRange > 0.0)
        painter->drawPolyline(_innerPts);

    painter->restore();
}
