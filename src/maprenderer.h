// maprenderer.h —— 兵棋地图渲染（1940 年代参谋部地形图风格）
#pragma once

#include <QImage>
#include <QPainter>
#include <QSizeF>

#include "mapspec.h"

namespace cf {

struct MapStyle {
    bool grid = true;        // 六角格线
    bool hexNumbers = true;  // 格号
    bool labels = true;      // 地名 / 地区名
    bool shade = true;       // 地形色块与纹理
    bool texture = true;     // 纸张纹理
    bool legend = true;      // 图例与比例尺
    bool frame = true;       // 图廓与接图表
    bool satellite = false;  // 卫星地形风（程序化高程 + 山体阴影 + 冠层/水域渐变）
    bool classic = false;    // 旧式逐格纹理地形（默认关闭；默认为平铺色块风）
};

// 版面尺寸（毫米）
QSizeF mapSizeMm(const MapSpec &m);

// 在 mm 坐标系里绘制整张图（原点 = 页面左上角）
void paintMap(QPainter *p, const MapSpec &m, const MapStyle &st = MapStyle());

// 渲染成位图（dpi 决定像素）
QImage mapImage(const MapSpec &m, int dpi, const MapStyle &st = MapStyle());

// 单格几何：返回格心（毫米，相对页面左上角）
QPointF hexCenterMm(const MapSpec &m, int col, int row);
qreal   hexRadiusMm(const MapSpec &m);   // 外接圆半径 R
qreal   hexHeightMm(const MapSpec &m);   // 对边距 H

// 命中测试：页面坐标（毫米）落在哪一格；找不到返回 false
bool hexAtMm(const MapSpec &m, const QPointF &ptMm, int *col, int *row);

} // namespace cf
