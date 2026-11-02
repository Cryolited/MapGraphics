#include "BeamObject.h"

#include "guts/Conversions.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtGlobal>
#include <QtMath>

#include "fpocommon/h/fpo/fpo_common/alsr_lafi.h"

//свои константы: M_PI определён не на всех компиляторах (MSVC)
static const qreal Pi = 3.14159265358979323846;

//эквивалентный радиус Земли для радиоволн (4/3 R), м
static const qreal EarthRadiusM = 6371000.0;
static const qreal EffEarthRadiusM = EarthRadiusM*4.0/3.0;

//число точек контура пятна и ядра
static const int OuterContourPts = 180;
static const int CoreContourPts = 120;

//экранные размеры, пиксели (не зависят от масштаба карты)
static const qreal OuterBorderPx = 1.5;
static const qreal CoreBorderPx = 1.2;
static const qreal CrossHalfPx = 5.5;
static const qreal CrossPenPx = 1.5;

//сдвиг позиции для принудительного обновления boundingRect, град (~1 см)
static const qreal NudgeDeg = 1e-7;

static qreal normalizedBearing(qreal deg)
{
    deg = fmod(deg,360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg;
}

static qreal normalizedLongitude(qreal deg)
{
    deg = fmod(deg + 180.0,360.0);
    if (deg < 0.0)
        deg += 360.0;
    return deg - 180.0;
}

static bool finitePoint(const QPointF &p)
{
    return qIsFinite(p.x()) && qIsFinite(p.y());
}

BeamObject::BeamObject(qreal verticalWidthDeg,
                       qreal horizontalWidthDeg,
                       qreal azimuthFromNoseDeg,
                       bool sizeIsZoomInvariant,
                       QColor fillColor,
                       QColor borderColor,
                       bool fillVisible,
                       MapGraphicsObject *parent) :
    MapGraphicsObject(sizeIsZoomInvariant,parent),
    ref(0),
    _vertWidthDeg(6.2),
    _horWidthDeg(3.0),
    _azFromNoseDeg(azimuthFromNoseDeg),
    _bankDeg(0.0),
    _tiltDeg(0.0),
    _heightM(0.0),
    _surfaceAltM(0.0),
    _maxRangeM(0.0),
    _earthCurvature(true),
    _coreVertDeg(1.0),
    _coreHorDeg(1.0),
    _fillColor(fillColor),
    _coreColor(),
    _borderColor(borderColor),
    _fillVisible(fillVisible),
    _centerCrossVisible(true),
    _dirty(true),
    _updateScheduled(false),
    _refPos(0.0,0.0),
    _aglM(1.0),
    _earthRadiusM(EffEarthRadiusM),
    _rangeLimitM(0.0),
    _centerPt(0.0,0.0),
    _centerOnGround(false),
    _nearRangeM(0.0),
    _farRangeM(0.0),
    _centerRangeM(0.0),
    _valid(false),
    _maxRadius(0.0),
    _boundRadius(0.0)
{
    for (int i = 0; i < 3; ++i)
        _ax[i] = _hx[i] = _vx[i] = 0.0;

    if (verticalWidthDeg > 0.0)
        _vertWidthDeg = qBound<qreal>(0.05,verticalWidthDeg,170.0);
    if (horizontalWidthDeg > 0.0)
        _horWidthDeg = qBound<qreal>(0.05,horizontalWidthDeg,170.0);
}

BeamObject::~BeamObject()
{
}

//pure-virtual from MapGraphicsObject
//прямоугольник обязан быть центрирован в (0,0) - см. PrivateQGraphicsObject
QRectF BeamObject::boundingRect() const
{
    if (_boundRadius <= 0.0)
        return QRectF(-1.0,-1.0,2.0,2.0);
    return QRectF(-_boundRadius,-_boundRadius,2.0*_boundRadius,2.0*_boundRadius);
}

//pure-virtual from MapGraphicsObject
void BeamObject::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!_valid || _outerPoly.size() < 3)
        return;

    const qreal scale = this->pixelScale(painter);
    const bool hasCore = _corePoly.size() >= 3;
    const QColor coreColor = this->effectiveCoreColor();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing,true);

    //заливка: основное пятно с вырезом под ядро (OddEven), затем ядро -
    //прозрачности не складываются, карта под ядром видна предсказуемо
    if (_fillVisible)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(_fillColor);
        if (hasCore)
        {
            QPainterPath ring;
            ring.setFillRule(Qt::OddEvenFill);
            ring.addPolygon(_outerPoly);
            ring.closeSubpath();
            ring.addPolygon(_corePoly);
            ring.closeSubpath();
            painter->drawPath(ring);

            painter->setBrush(coreColor);
            painter->drawPolygon(_corePoly);
        }
        else
            painter->drawPolygon(_outerPoly);
    }

    //кромки постоянной экранной толщины
    painter->setBrush(Qt::NoBrush);
    const QColor outerBorder = this->effectiveBorderColor();
    if (outerBorder.alpha() > 0)
    {
        QPen pen(outerBorder,OuterBorderPx/scale);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->drawPolygon(_outerPoly);
    }
    if (hasCore)
    {
        QColor c = coreColor;
        c.setAlpha(230);
        QPen pen(c,CoreBorderPx/scale);
        pen.setJoinStyle(Qt::RoundJoin);
        painter->setPen(pen);
        painter->drawPolygon(_corePoly);
    }

    if (_centerCrossVisible && _centerOnGround)
        this->drawCenterCross(painter,scale);

    painter->restore();
}

void BeamObject::updateObjectData(QString country, int value, int max, int min)
{
    Q_UNUSED(country)
    Q_UNUSED(value)
    Q_UNUSED(max)
    Q_UNUSED(min)
}

//---------------------------------------------------------------- сеттеры

qreal BeamObject::verticalWidthDeg() const
{
    return _vertWidthDeg;
}

void BeamObject::setVerticalWidthDeg(qreal widthDeg)
{
    if (widthDeg <= 0.0)
        return;
    widthDeg = qBound<qreal>(0.05,widthDeg,170.0);
    if (widthDeg == _vertWidthDeg)
        return;
    _vertWidthDeg = widthDeg;
    this->markDirty();
}

void BeamObject::setVerticalWidthRad(qreal widthRad)
{
    this->setVerticalWidthDeg(qRadiansToDegrees(widthRad));
}

qreal BeamObject::horizontalWidthDeg() const
{
    return _horWidthDeg;
}

void BeamObject::setHorizontalWidthDeg(qreal widthDeg)
{
    if (widthDeg <= 0.0)
        return;
    widthDeg = qBound<qreal>(0.05,widthDeg,170.0);
    if (widthDeg == _horWidthDeg)
        return;
    _horWidthDeg = widthDeg;
    this->markDirty();
}

void BeamObject::setHorizontalWidthRad(qreal widthRad)
{
    this->setHorizontalWidthDeg(qRadiansToDegrees(widthRad));
}

qreal BeamObject::azimuthFromNoseDeg() const
{
    return _azFromNoseDeg;
}

void BeamObject::setAzimuthFromNoseDeg(qreal angleDeg)
{
    if (angleDeg == _azFromNoseDeg)
        return;
    _azFromNoseDeg = angleDeg;
    this->markDirty();
}

void BeamObject::setAzimuthFromNoseRad(qreal angleRad)
{
    this->setAzimuthFromNoseDeg(qRadiansToDegrees(angleRad));
}

qreal BeamObject::bankDeg() const
{
    return _bankDeg;
}

void BeamObject::setBankDeg(qreal bankDeg)
{
    if (!qIsFinite(bankDeg) || bankDeg == _bankDeg)
        return;
    _bankDeg = bankDeg;
    this->markDirty();
}

void BeamObject::setBankRad(qreal bankRad)
{
    this->setBankDeg(qRadiansToDegrees(bankRad));
}

qreal BeamObject::tiltDeg() const
{
    return _tiltDeg;
}

void BeamObject::setTiltDeg(qreal tiltDeg)
{
    if (!qIsFinite(tiltDeg) || tiltDeg == _tiltDeg)
        return;
    _tiltDeg = tiltDeg;
    this->markDirty();
}

void BeamObject::setTiltRad(qreal tiltRad)
{
    this->setTiltDeg(qRadiansToDegrees(tiltRad));
}

qreal BeamObject::height() const
{
    return _heightM;
}

void BeamObject::setHeight(qreal heightM)
{
    if (!qIsFinite(heightM) || heightM == _heightM)
        return;
    _heightM = heightM;
    this->markDirty();
}

qreal BeamObject::surfaceAltitude() const
{
    return _surfaceAltM;
}

void BeamObject::setSurfaceAltitude(qreal altitudeM)
{
    if (!qIsFinite(altitudeM) || altitudeM == _surfaceAltM)
        return;
    _surfaceAltM = altitudeM;
    this->markDirty();
}

qreal BeamObject::maxRange() const
{
    return _maxRangeM;
}

void BeamObject::setMaxRange(qreal rangeM)
{
    rangeM = qMax<qreal>(0.0,rangeM);
    if (rangeM == _maxRangeM)
        return;
    _maxRangeM = rangeM;
    this->markDirty();
}

bool BeamObject::earthCurvature() const
{
    return _earthCurvature;
}

void BeamObject::setEarthCurvature(bool on)
{
    if (on == _earthCurvature)
        return;
    _earthCurvature = on;
    this->markDirty();
}

qreal BeamObject::coreHalfWidthVerticalDeg() const
{
    return _coreVertDeg;
}

qreal BeamObject::coreHalfWidthHorizontalDeg() const
{
    return _coreHorDeg;
}

void BeamObject::setCoreHalfWidthDeg(qreal halfWidthDeg)
{
    this->setCoreHalfWidthDeg(halfWidthDeg,halfWidthDeg);
}

void BeamObject::setCoreHalfWidthDeg(qreal verticalHalfDeg, qreal horizontalHalfDeg)
{
    verticalHalfDeg = qMax<qreal>(0.0,verticalHalfDeg);
    horizontalHalfDeg = qMax<qreal>(0.0,horizontalHalfDeg);
    if (verticalHalfDeg == _coreVertDeg && horizontalHalfDeg == _coreHorDeg)
        return;
    _coreVertDeg = verticalHalfDeg;
    _coreHorDeg = horizontalHalfDeg;
    this->markDirty();
}

qreal BeamObject::nearRange() const
{
    return _valid ? _nearRangeM : 0.0;
}

qreal BeamObject::farRange() const
{
    return _valid ? _farRangeM : 0.0;
}

qreal BeamObject::centerRange() const
{
    return _valid ? _centerRangeM : 0.0;
}

bool BeamObject::footprintValid() const
{
    return _valid;
}

QColor BeamObject::color() const
{
    return _fillColor;
}

void BeamObject::setColor(QColor fillColor)
{
    _fillColor = fillColor;
    this->redrawRequested();
}

QColor BeamObject::coreColor() const
{
    return _coreColor;
}

void BeamObject::setCoreColor(QColor coreColor)
{
    _coreColor = coreColor;
    this->redrawRequested();
}

QColor BeamObject::borderColor() const
{
    return _borderColor;
}

void BeamObject::setBorderColor(QColor borderColor)
{
    _borderColor = borderColor;
    this->redrawRequested();
}

bool BeamObject::fillVisible() const
{
    return _fillVisible;
}

void BeamObject::setFillVisible(bool visible)
{
    _fillVisible = visible;
    this->redrawRequested();
}

bool BeamObject::centerCrossVisible() const
{
    return _centerCrossVisible;
}

void BeamObject::setCenterCrossVisible(bool visible)
{
    _centerCrossVisible = visible;
    this->redrawRequested();
}

//---------------------------------------------------------------- обновление

//public slot
void BeamObject::setPosBeam(void)
{
    if (ref == 0)
        return;

    //геометрия считается ДО setPos: при смене позиции QGraphicsItem
    //перечитывает boundingRect, и он уже должен быть новым
    this->updateGeometry();
    this->setPos(QPointF(ref->longitude(),ref->latitude()));
    this->redrawRequested();
}

//сеттеры не пересчитывают сразу (за тик их приходит несколько):
//пересчёт - в setPosBeam, а если его не было - отложенно, один раз
void BeamObject::markDirty(void)
{
    _dirty = true;
    if (ref != 0 && !_updateScheduled)
    {
        _updateScheduled = true;
        QTimer::singleShot(0,this,SLOT(applyPendingGeometry()));
    }
}

//private slot
void BeamObject::applyPendingGeometry(void)
{
    _updateScheduled = false;
    if (!_dirty || ref == 0)
        return;
    this->updateGeometry();
    this->redrawRequested();
}

void BeamObject::updateGeometry(void)
{
    _dirty = false;
    _valid = false;
    _centerOnGround = false;
    _nearRangeM = _farRangeM = _centerRangeM = 0.0;
    _outerPoly.clear();
    _corePoly.clear();
    _centerPt = QPointF(0.0,0.0);
    _maxRadius = 0.0;

    if (ref == 0)
    {
        this->updateBoundingRadius();
        return;
    }

    _refPos = Position(ref->longitude(),ref->latitude());
    _aglM = qMax<qreal>(1.0,_heightM - _surfaceAltM);
    _earthRadiusM = EffEarthRadiusM;

    //предельная дальность: радиогоризонт (для сферы - точный, для плоской
    //земли - тот же 4.12*sqrt(H) км) или заданная пользователем
    const qreal horizonM = _earthCurvature
            ? _earthRadiusM*qAcos(_earthRadiusM/(_earthRadiusM + _aglM))
            : qSqrt(2.0*_earthRadiusM*_aglM);
    _rangeLimitM = horizonM;
    if (_maxRangeM > 0.0 && (!_earthCurvature || _maxRangeM < horizonM))
        _rangeLimitM = _maxRangeM;

    //базис луча в ENU (east, north, up):
    //  _ax - ось луча, _hx - горизонтальная поперечная ось, _vx - вертикальная
    const qreal bearing = qDegreesToRadians(normalizedBearing(ref->rotation() + _azFromNoseDeg));
    const qreal elev = qDegreesToRadians(qBound<qreal>(-90.0,_bankDeg + _tiltDeg,90.0));
    const qreal sb = qSin(bearing), cb = qCos(bearing);
    const qreal se = qSin(elev), ce = qCos(elev);

    _ax[0] = ce*sb;  _ax[1] = ce*cb;  _ax[2] = se;
    _hx[0] = cb;     _hx[1] = -sb;    _hx[2] = 0.0;
    _vx[0] = -se*sb; _vx[1] = -se*cb; _vx[2] = ce;

    const qreal halfV = qDegreesToRadians(0.5*_vertWidthDeg);
    const qreal halfH = qDegreesToRadians(0.5*_horWidthDeg);

    if (!this->buildContour(halfV,halfH,OuterContourPts,_outerPoly))
    {
        _outerPoly.clear();
        this->updateBoundingRadius();
        return;   //луч целиком выше горизонта / дальше предельной дальности
    }

    //ядро: не шире луча; если совпадает с лучом в обеих плоскостях - не нужно
    const qreal coreV = qMin<qreal>(_coreVertDeg,0.5*_vertWidthDeg);
    const qreal coreH = qMin<qreal>(_coreHorDeg,0.5*_horWidthDeg);
    if (coreV > 0.0 && coreH > 0.0 &&
        (coreV < 0.5*_vertWidthDeg || coreH < 0.5*_horWidthDeg))
    {
        if (!this->buildContour(qDegreesToRadians(coreV),qDegreesToRadians(coreH),
                                CoreContourPts,_corePoly))
            _corePoly.clear();
    }

    //дальности по оси луча и по её краям в вертикальной плоскости
    qreal az = 0.0;
    const qreal ch = qCos(halfV), sh = qSin(halfV);
    _centerOnGround = this->groundRange(_ax[0],_ax[1],_ax[2],_centerRangeM,az);
    if (_centerOnGround)
    {
        _centerPt = this->groundPoint(az,_centerRangeM);
        if (!finitePoint(_centerPt))
            _centerOnGround = false;
    }
    this->groundRange(ch*_ax[0] - sh*_vx[0], ch*_ax[1] - sh*_vx[1], ch*_ax[2] - sh*_vx[2],
                      _nearRangeM,az);
    this->groundRange(ch*_ax[0] + sh*_vx[0], ch*_ax[1] + sh*_vx[1], ch*_ax[2] + sh*_vx[2],
                      _farRangeM,az);

    for (int i = 0; i < _outerPoly.size(); ++i)
    {
        const QPointF &p = _outerPoly.at(i);
        _maxRadius = qMax<qreal>(_maxRadius,qSqrt(p.x()*p.x() + p.y()*p.y()));
    }

    _valid = true;
    this->updateBoundingRadius();
}

//Контур следа: образующие конуса по эллипсу в направляющих косинусах
//(u - поперёк по горизонтали, w - по вертикали), каждая продлевается до земли.
//Возвращает false, если ни одна образующая не достаёт землю в пределах
//предельной дальности (пятна нет). Не достающие - кладутся на дугу R_max.
bool BeamObject::buildContour(qreal halfVertRad, qreal halfHorRad, int nPts, QPolygonF &poly)
{
    poly.clear();
    poly.reserve(nPts);

    const qreal su = qSin(halfHorRad);
    const qreal sw = qSin(halfVertRad);
    bool anyOnGround = false;

    for (int i = 0; i < nPts; ++i)
    {
        const qreal t = 2.0*Pi*qreal(i)/qreal(nPts);
        const qreal u = su*qCos(t);
        const qreal w = sw*qSin(t);
        const qreal c = qSqrt(qMax<qreal>(0.0,1.0 - u*u - w*w));

        const qreal e = c*_ax[0] + u*_hx[0] + w*_vx[0];
        const qreal n = c*_ax[1] + u*_hx[1] + w*_vx[1];
        const qreal up = c*_ax[2] + u*_hx[2] + w*_vx[2];

        qreal range = 0.0, az = 0.0;
        if (this->groundRange(e,n,up,range,az))
            anyOnGround = true;

        const QPointF p = this->groundPoint(az,range);
        if (!finitePoint(p))
        {
            poly.clear();
            return false;
        }
        poly << p;
    }

    if (!anyOnGround)
        poly.clear();
    return anyOnGround;
}

//Дальность по земле до точки, где образующая (east,north,up) встречает землю.
//Сфера: самолёт на высоте agl над сферой радиуса Re; ищем ближнее пересечение
//луча со сферой и берём дугу Re*gamma. Плоская: agl/tan(угла депрессии).
bool BeamObject::groundRange(qreal e, qreal n, qreal u, qreal &rangeM, qreal &azDeg) const
{
    const qreal horiz = qSqrt(e*e + n*n);
    azDeg = (horiz > 1e-12) ? qRadiansToDegrees(qAtan2(e,n)) : 0.0;

    bool hit = false;
    qreal range = 0.0;
    if (_earthCurvature)
    {
        if (u < 0.0)
        {
            const qreal Re = _earthRadiusM;
            const qreal Rh = Re + _aglM;
            const qreal disc = Rh*Rh*u*u - _aglM*(2.0*Re + _aglM);
            if (disc >= 0.0)
            {
                const qreal s = -Rh*u - qSqrt(disc);    //наклонная дальность
                const qreal gamma = qAtan2(s*horiz,Rh + s*u);
                range = Re*gamma;
                hit = true;
            }
        }
    }
    else if (u < -1e-12)
    {
        range = _aglM*horiz/(-u);
        hit = true;
    }

    if (!hit || !qIsFinite(range) || range > _rangeLimitM)
    {
        rangeM = _rangeLimitM;
        return false;
    }
    rangeM = qMax<qreal>(0.0,range);
    return true;
}

//точка на азимуте azDeg и дальности rangeM от самолёта, метры ENU
QPointF BeamObject::groundPoint(qreal azDeg, qreal rangeM) const
{
    if (rangeM < 0.01)
        return QPointF(0.0,0.0);

    LAFI_DOUBLE lafid;
    LafiD(0,rangeM,0,
          qDegreesToRadians(normalizedBearing(azDeg)),
          qDegreesToRadians(_refPos.latitude()),
          qDegreesToRadians(_refPos.longitude()),
          &lafid);

    const Position p(normalizedLongitude(qRadiansToDegrees(lafid.la)),
                     qRadiansToDegrees(lafid.fi));
    return Conversions::lla2enu(p,_refPos).toPointF();
}

//Радиус boundingRect держится с запасом и меняется ступенями: когда он
//меняется, а самолёт стоит на месте, QGraphicsItem сам boundingRect не
//перечитает (prepareGeometryChange вызывается только при смене позиции) -
//пятно обрезалось бы старым прямоугольником. Поэтому при смене радиуса
//позиция "дёргается" на ~1 см туда и обратно.
void BeamObject::updateBoundingRadius(void)
{
    qreal newRadius = _boundRadius;
    if (!_valid)
        newRadius = 0.0;
    else
    {
        const qreal need = 1.05*_maxRadius + 50.0;
        if (_boundRadius < need || _boundRadius > 3.0*need)
            newRadius = 1.5*need;
    }

    if (newRadius == _boundRadius)
        return;
    _boundRadius = newRadius;

    const QPointF p = this->pos();
    this->setPos(QPointF(p.x() + NudgeDeg,p.y()));
    this->setPos(p);
}

//пикселей на метр (painter масштабирован PrivateQGraphicsObject'ом)
qreal BeamObject::pixelScale(QPainter *painter) const
{
    const QTransform &t = painter->transform();
    qreal s = qSqrt(t.m11()*t.m11() + t.m12()*t.m12());
    if (!qIsFinite(s) || s < 1e-12)
        s = qSqrt(t.m21()*t.m21() + t.m22()*t.m22());
    if (!qIsFinite(s) || s < 1e-12)
        s = 1.0;
    return s;
}

QColor BeamObject::effectiveBorderColor(void) const
{
    if (_borderColor.isValid())
        return _borderColor;

    QColor c = _fillColor;
    c.setAlpha(_fillVisible ? qBound(0,3*_fillColor.alpha() + 40,220) : 255);
    return c;
}

QColor BeamObject::effectiveCoreColor(void) const
{
    if (_coreColor.isValid())
        return _coreColor;

    QColor c = _fillColor;
    c.setAlpha(qBound(0,qRound(2.6*_fillColor.alpha()),200));
    return c;
}

//перекрестие в точке пересечения оси с землёй: белая подложка + цветной крест
void BeamObject::drawCenterCross(QPainter *painter, qreal scale) const
{
    const qreal half = CrossHalfPx/scale;
    const QPointF l(_centerPt.x() - half,_centerPt.y());
    const QPointF r(_centerPt.x() + half,_centerPt.y());
    const QPointF t(_centerPt.x(),_centerPt.y() - half);
    const QPointF b(_centerPt.x(),_centerPt.y() + half);

    QPen halo(Qt::white,(CrossPenPx + 1.7)/scale);
    halo.setCapStyle(Qt::FlatCap);
    painter->setPen(halo);
    painter->drawLine(l,r);
    painter->drawLine(t,b);

    QColor c = _fillColor;
    c.setAlpha(255);
    QPen pen(c,CrossPenPx/scale);
    pen.setCapStyle(Qt::FlatCap);
    painter->setPen(pen);
    painter->drawLine(l,r);
    painter->drawLine(t,b);
}
