#ifndef BEAMOBJECT_H
#define BEAMOBJECT_H

#include "MapGraphics_global.h"
#include "MapGraphicsObject.h"
#include "Position.h"

#include <QColor>
#include <QPointF>
#include <QPolygonF>

class MAPGRAPHICSSHARED_EXPORT BeamObject : public MapGraphicsObject
{
    Q_OBJECT
public:
    /*!
     \brief Пятно облучения (след ДН антенны) на земле, привязанное к ref.

     ВСЕ УГЛЫ В ГРАДУСАХ (как в SectorObject); варианты с суффиксом Rad
     принимают радианы (setTiltRad / setBankRad / ...).

     ГЕОМЕТРИЯ
     Луч - эллиптический конус с вершиной в самолёте: полная ширина
     horizontalWidth (поперёк оси, в горизонтальной плоскости антенны) и
     verticalWidth (в вертикальной плоскости, т.е. "по дальности").
     Контур ДН - эллипс в пространстве направляющих косинусов антенны;
     каждая образующая продлевается до пересечения с землёй. Получается
     честный след ДН: при пологом луче - вытянутая "капля" (дальний край
     шире ближнего), у горизонта дальний край обрезается дугой предельной
     дальности, при крутом луче, накрывающем надир, - пятно вокруг самолёта.

     Ось луча:
       - азимут = ref->rotation() + azimuthFromNose (по часовой, град);
         в этом проекте rotation() + 0 - ЛЕВЫЙ траверз (по умолчанию 0),
         180 - правый траверз;
       - угол места оси = bank + tilt (как в коде трёх линий:
         ang = -tan(bank + tilt), дальность = (heig - altSurf)/ang),
         т.е. ОТРИЦАТЕЛЬНАЯ сумма - луч смотрит ВНИЗ. Для траверзного
         луча это точно совпадает с поворотом жёстко закреплённой антенны
         креном (крен вращает антенну вокруг продольной оси самолёта,
         т.е. ровно в вертикальной плоскости луча).

     Земля по умолчанию - сфера эквивалентного радиуса 4/3 R (рефракция
     радиоволн): у горизонта плоская модель сильно занижает дальность.
     setEarthCurvature(false) - плоская земля, дальности тогда в точности
     как x1/x2/x3 из кода трёх линий.

     ЗОНА ПОВЫШЕННОЙ ЭНЕРГИИ ("ядро")
     След внутреннего конуса +/-coreHalfWidth от оси (по умолчанию 1 град
     в обеих плоскостях, но не шире самого луча). Рисуется тем же цветом,
     но плотнее, и со своей кромкой; основная заливка под ядром вырезана,
     поэтому прозрачности не складываются и карта под ядром читается
     одинаково при любом фоне. Цвет ядра можно задать отдельно
     (setCoreColor), 0 - ядро не рисовать (setCoreHalfWidthDeg(0)).
     В точке пересечения оси с землёй - перекрестие постоянного
     экранного размера.

     ОБНОВЛЕНИЕ
     setTiltRad, setBankRad, setHeight и прочие сеттеры только запоминают
     значение; пересчёт - один раз в setPosBeam() (или, если setPosBeam
     в этом цикле не вызывали, - сам, отложенно, на следующем проходе
     цикла событий). Порядок вызовов в слоте телеметрии не важен.

     \param verticalWidthDeg полная ширина ДН по вертикали (по углу места), град
     \param horizontalWidthDeg полная ширина ДН по горизонтали (по азимуту), град
     \param azimuthFromNoseDeg добавка к ref->rotation() по часовой, град (0 - левый траверз)
     \param sizeIsZoomInvariant должно быть false (размеры пятна в метрах)
     \param fillColor цвет заливки пятна; ядро - тот же цвет плотнее
     \param borderColor цвет кромки пятна; QColor() - цвет заливки,
            но непрозрачнее; Qt::transparent - без кромки
     \param fillVisible false - только контуры (пятно и ядро)
     \param parent
    */
    explicit BeamObject(qreal verticalWidthDeg = 6.2,
                        qreal horizontalWidthDeg = 3.0,
                        qreal azimuthFromNoseDeg = 0.0,
                        bool sizeIsZoomInvariant = false,
                        QColor fillColor = QColor(0,90,255,45),
                        QColor borderColor = QColor(),
                        bool fillVisible = true,
                        MapGraphicsObject *parent = 0);
    virtual ~BeamObject();

    //pure-virtual from MapGraphicsObject
    QRectF boundingRect() const;

    //pure-virtual from MapGraphicsObject
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void updateObjectData(QString country, int value, int max, int min);

    //Самолёт, к которому привязано пятно (тот же ref, что у SectorObject)
    MapGraphicsObject *ref;

    //---- ширина ДН (полная), град / рад ----
    qreal verticalWidthDeg() const;
    void setVerticalWidthDeg(qreal widthDeg);
    void setVerticalWidthRad(qreal widthRad);

    qreal horizontalWidthDeg() const;
    void setHorizontalWidthDeg(qreal widthDeg);
    void setHorizontalWidthRad(qreal widthRad);

    //---- направление оси ----
    //добавка к ref->rotation() по часовой, град (0 - левый траверз, 180 - правый)
    qreal azimuthFromNoseDeg() const;
    void setAzimuthFromNoseDeg(qreal angleDeg);
    void setAzimuthFromNoseRad(qreal angleRad);

    //крен; входит в угол места оси как bank + tilt (отрицательно - вниз)
    qreal bankDeg() const;
    void setBankDeg(qreal bankDeg);
    void setBankRad(qreal bankRad);

    //наклон ДН; входит в угол места оси как bank + tilt (отрицательно - вниз)
    qreal tiltDeg() const;
    void setTiltDeg(qreal tiltDeg);
    void setTiltRad(qreal tiltRad);

    //---- высоты, дальность ----
    //высота самолёта, м (работает разность с высотой поверхности)
    qreal height() const;
    void setHeight(qreal heightM);

    //высота поверхности под пятном, м (по умолчанию 0)
    qreal surfaceAltitude() const;
    void setSurfaceAltitude(qreal altitudeM);

    //предельная дальность пятна, м; 0 (по умолчанию) - радиогоризонт
    qreal maxRange() const;
    void setMaxRange(qreal rangeM);

    //true (по умолчанию) - сферическая земля 4/3 R; false - плоская
    bool earthCurvature() const;
    void setEarthCurvature(bool on);

    //---- зона повышенной энергии ----
    //полуширина ядра от оси, град; setCoreHalfWidthDeg задаёт обе плоскости.
    //Не шире самого луча; 0 в обеих - ядро не рисуется
    qreal coreHalfWidthVerticalDeg() const;
    qreal coreHalfWidthHorizontalDeg() const;
    void setCoreHalfWidthDeg(qreal halfWidthDeg);
    void setCoreHalfWidthDeg(qreal verticalHalfDeg, qreal horizontalHalfDeg);

    //---- результаты расчёта по оси луча (по азимуту оси), м ----
    //ближний/дальний край (угол места оси -/+ половина вертикальной ширины)
    //и точка оси; 0 - пятна нет
    qreal nearRange() const;
    qreal farRange() const;
    qreal centerRange() const;
    //пятно есть (хоть часть ДН достаёт землю)
    bool footprintValid() const;

    //---- вид ----
    QColor color() const;
    void setColor(QColor fillColor);

    //цвет ядра; QColor() (по умолчанию) - цвет заливки, плотнее
    QColor coreColor() const;
    void setCoreColor(QColor coreColor);

    QColor borderColor() const;
    void setBorderColor(QColor borderColor);

    bool fillVisible() const;
    void setFillVisible(bool visible);

    bool centerCrossVisible() const;
    void setCenterCrossVisible(bool visible);

signals:

public slots:
    //Вызывать, когда обновилось положение/курс ЛА (как setPosSector)
    void setPosBeam(void);

private slots:
    void applyPendingGeometry(void);

private:
    void markDirty(void);
    void updateGeometry(void);
    //контур следа конуса с полуширинами (рад) в двух плоскостях, метры ENU
    bool buildContour(qreal halfVertRad, qreal halfHorRad, int nPts, QPolygonF &poly);
    //дальность по земле (м) и азимут (град) образующей с направлением
    //(east, north, up); false - образующая не достаёт землю (дальность = R_max)
    bool groundRange(qreal e, qreal n, qreal u, qreal &rangeM, qreal &azDeg) const;
    QPointF groundPoint(qreal azDeg, qreal rangeM) const;
    void updateBoundingRadius(void);
    qreal pixelScale(QPainter *painter) const;
    QColor effectiveBorderColor(void) const;
    QColor effectiveCoreColor(void) const;
    void drawCenterCross(QPainter *painter, qreal scale) const;

    //входные данные
    qreal _vertWidthDeg;
    qreal _horWidthDeg;
    qreal _azFromNoseDeg;
    qreal _bankDeg;
    qreal _tiltDeg;
    qreal _heightM;
    qreal _surfaceAltM;
    qreal _maxRangeM;
    bool _earthCurvature;
    qreal _coreVertDeg;
    qreal _coreHorDeg;

    QColor _fillColor;
    QColor _coreColor;
    QColor _borderColor;
    bool _fillVisible;
    bool _centerCrossVisible;

    //состояние пересчёта
    bool _dirty;
    bool _updateScheduled;

    //текущая геометрия (метры ENU относительно самолёта, y - на север)
    Position _refPos;
    qreal _aglM;
    qreal _earthRadiusM;
    qreal _rangeLimitM;
    qreal _ax[3];               //ось луча (east, north, up)
    qreal _hx[3];               //горизонтальная ось поперёк луча
    qreal _vx[3];               //вертикальная ось поперёк луча
    QPolygonF _outerPoly;
    QPolygonF _corePoly;
    QPointF _centerPt;
    bool _centerOnGround;
    qreal _nearRangeM;
    qreal _farRangeM;
    qreal _centerRangeM;
    bool _valid;
    qreal _maxRadius;           //наибольшее удаление точек пятна, м
    qreal _boundRadius;         //радиус boundingRect (с запасом и гистерезисом), м
};

#endif // BEAMOBJECT_H
