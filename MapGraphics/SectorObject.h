#ifndef SECTOROBJECT_H
#define SECTOROBJECT_H

#include "MapGraphics_global.h"
#include "MapGraphicsObject.h"
#include "Position.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class MAPGRAPHICSSHARED_EXPORT SectorObject : public MapGraphicsObject
{
    Q_OBJECT
public:
    /*!
     \brief Полупрозрачный "сектор обзора" из самолёта, привязанный к ref.

     Геометрия: кольцевой сектор с вершиной в точке самолёта, от ближней границы
     (nearRange) до дальней (farRange). Центр сектора отсчитывается от КУРСА
     самолёта по часовой стрелке: 0 - нос, 90 - правый траверз, 180 - хвост,
     270 - левый траверз.

     В середине сектора (на биссектрисе, на средней дальности между ближней
     и дальней границами) рисуется маленькое перекрестие постоянного размера
     на экране; отключается setCenterCrossVisible(false).

     Границы сектора (обе дуги и боковые кромки) рисуются цветом borderColor
     толщиной 1.5 экранных пикселя; QColor() (по умолчанию) - границ нет,
     вид существующих секторов не меняется. fillVisible=false даёт
     контурный сектор - заливка не рисуется, остаются только границы
     (и перекрестие); если при этом borderColor не задан, границы берутся
     цветом заливки с полной непрозрачностью, чтобы контурный сектор
     не оказался пустым.

     nearRange/farRange задаются в тех же единицах, что _shift у CircleObject
     (передаются в LafiD напрямую).

     \param nearRange дальность ближней границы
     \param farRange дальность дальней границы
     \param centerAngleFromAbeam угол центра сектора от курса по часовой
            стрелке (0 - нос, 90 - правый траверз, 180 - хвост,
            270 - левый траверз), градусы
     \param sectorWidth ширина сектора, градусы (если <= 0, берётся 8.0)
     \param sizeIsZoomInvariant как у остальных объектов (для сектора в метрах - false)
     \param fillColor цвет заливки (по умолчанию полупрозрачный синий)
     \param borderColor цвет границ (дуги и боковые кромки); не задан
            (QColor()) - границ нет
     \param fillVisible false - контурный сектор (только границы, без заливки)
     \param parent
    */
    explicit SectorObject(qreal nearRange,
                          qreal farRange,
                          qreal centerAngleFromAbeam = 0.0,
                          qreal sectorWidth = 8.0,
                          bool sizeIsZoomInvariant = false,
                          QColor fillColor = QColor(0,0,255,70),
                          QColor borderColor = QColor(),
                          bool fillVisible = true,
                          MapGraphicsObject *parent = 0);
    virtual ~SectorObject();

    //pure-virtual from MapGraphicsObject
    QRectF boundingRect() const;

    //pure-virtual from MapGraphicsObject
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void updateObjectData(QString country, int value, int max, int min);

    qreal nearRange() const;
    void setNearRange(qreal nearRange);

    qreal farRange() const;
    void setFarRange(qreal farRange);

    qreal centerAngleFromAbeam() const;
    void setCenterAngleFromAbeam(qreal angleFromAbeam);

    qreal sectorWidth() const;
    void setSectorWidth(qreal width);

    qreal edgeFadeFraction() const;
    void setEdgeFadeFraction(qreal fraction);

    void setColor(QColor fillColor);

    QColor borderColor() const;
    void setBorderColor(QColor borderColor);

    bool fillVisible() const;
    void setFillVisible(bool visible);

    bool centerCrossVisible() const;
    void setCenterCrossVisible(bool visible);

    //Самолёт, к которому привязан сектор (тот же ref, что в CircleObject/LineObject)
    MapGraphicsObject *ref;

signals:

public slots:
    //Вызывать по connect, когда пришли координаты/курс самолёта (как setPosCircle)
    void setPosSector(void);

private:
    Position pointAtBearing(qreal bearingDeg, qreal range) const;
    QPointF enuOffset(const Position &pos) const;
    void updateGeometry(void);
    void drawBorder(QPainter *painter) const;
    void drawCenterCross(QPainter *painter) const;

    qreal _nearRange;
    qreal _farRange;
    qreal _centerAngleFromAbeam;
    qreal _sectorWidth;
    qreal _edgeFadeFraction;
    QColor _fillColor;
    QColor _borderColor;
    bool _fillVisible;
    bool _centerCrossVisible;

    Position _refPos;
    qreal _centerBearingDeg;     //азимут биссектрисы сектора, [0,360)
    qreal _maxRadius;            //наибольший радиус точек ломаной, метры
    QPointF _centerPt;           //середина сектора (точка перекрестия), метры ENU
    QVector<QPointF> _outerPts;  //внешняя дуга: точки по азимутам, метры ENU
    QVector<QPointF> _innerPts;  //внутренняя дуга (либо все (0,0)), метры ENU
    bool _positionValid;
};

#endif // SECTOROBJECT_H
