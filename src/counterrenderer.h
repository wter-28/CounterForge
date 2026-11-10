// counterrenderer.h —— 算子绘制引擎
#pragma once

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QStringList>
#include <QVector>

#include "counterspec.h"

namespace cf {

struct RenderStyle {
    bool shadow      = false;  // 柔和投影（预览用）
    bool paperBorder = false;  // 白色裁切边
    bool texture     = true;   // 纸纹与油墨斑驳
    bool weathering  = true;   // 模切边缘磨损
    int  supplyStyle = 0;      // 补给队中心图形：0 实心卡车 / 1 线稿卡车 / 2 北约补给符号
};

// 在 rect 内绘制一枚算子（按正方形取最小边）
void paintCounter(QPainter *p, const QRectF &rect, const CounterSpec &s,
                  const RenderStyle &st = RenderStyle());

// 北约军兵种符号（外框 + 内部图元）
void paintNatoSymbol(QPainter *p, const QRectF &frame, const QColor &color, Branch b);

// 补给队的中心图形：实心卡车 / 线稿卡车
void paintSupplyTruck(QPainter *p, const QRectF &box, const QColor &color);
void paintSupplyTruckLine(QPainter *p, const QRectF &box, const QColor &color);

// 规模符号
void paintEchelon(QPainter *p, const QPointF &center, qreal height,
                  const QColor &color, Echelon e);

// 堆叠圈（右上角）：空圈 = 1，实心圈 = 2
void paintStackMark(QPainter *p, const QPointF &center, qreal diameter,
                    const QColor &color, int stacking);

QImage counterImage(const CounterSpec &s, int px, const RenderStyle &st = RenderStyle());
QImage sheetImage(const QVector<CounterSpec> &items, int dpi, qreal counterMm,
                  bool cutMarks, bool labels);
QImage iconImage(const CounterSpec &s, int px);

// 纸纹贴图（界面背景也用它，保证质感统一）
const QImage &paperGrain();

QColor formationPreset(int i);
QStringList formationPresetNames();

} // namespace cf
