// counterrenderer.cpp —— 算子绘制引擎
// 目标观感：厚卡纸 + 油墨印刷 + 模切边缘，而不是扁平矢量图标
#include "counterrenderer.h"

#include <QFontMetricsF>
#include <QImage>
#include <QLinearGradient>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QRadialGradient>
#include <QStringList>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace cf {

// ══════════════════════════════════════════════════════════════════
//  基础工具
// ══════════════════════════════════════════════════════════════════
static const QStringList &condensedFamilies()
{
    static const QStringList f{QStringLiteral("Bahnschrift SemiCondensed"),
                               QStringLiteral("Bahnschrift"),
                               QStringLiteral("Arial Narrow"),
                               QStringLiteral("Arial")};
    return f;
}

static const QStringList &cjkFamilies()
{
    static const QStringList f{QStringLiteral("Microsoft YaHei UI"),
                               QStringLiteral("Microsoft YaHei"),
                               QStringLiteral("SimHei"),
                               QStringLiteral("Arial")};
    return f;
}

static QColor mixColor(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor(int(a.red()   * (1 - t) + b.red()   * t),
                  int(a.green() * (1 - t) + b.green() * t),
                  int(a.blue()  * (1 - t) + b.blue()  * t),
                  int(a.alpha() * (1 - t) + b.alpha() * t));
}

static QColor shade(const QColor &c, qreal f)
{
    float h = 0, s = 0, v = 0, a = 1;
    c.getHsvF(&h, &s, &v, &a);
    v = float(qBound(0.0, double(v) * f, 1.0));
    if (f > 1.0) s = float(qBound(0.0, double(s) * 0.94, 1.0));
    return QColor::fromHsvF(qBound(0.0f, h, 1.0f), s, v, a);
}

// 把任意颜色压成"印刷油墨"：略降饱和、轻微偏暖，避免纯 RGB 的塑料感
static QColor printInk(const QColor &c)
{
    if (!c.isValid()) return c;
    QColor hsv = c.toHsv();
    hsv.setHsv(hsv.hue(),
               int(qMin(255.0, hsv.saturation() * 1.02)),
               int(qMin(255.0, hsv.value() * 1.02)),
               c.alpha());
    return mixColor(hsv.toRgb(), QColor(226, 214, 190), 0.012);
}

// 上部底色：压到接近黑的中性深色，只带一点点编制色倾向
// （参考成品：无论编制色是什么，上部都是同一种近黑色，编制色只在下部彩带体现）
static QColor autoTopColor(const QColor &fc)
{
    return mixColor(QColor(0x24, 0x23, 0x22), shade(fc, 0.36), 0.42);
}

// ── 纸纹（细颗粒 + 纤维）──────────────────────────────────────────
static const QImage &paperGrainTexture()
{
    static const QImage img = [] {
        const int N = 512;
        QImage im(N, N, QImage::Format_ARGB32_Premultiplied);
        im.fill(Qt::transparent);
        QPainter p(&im);
        p.setRenderHint(QPainter::Antialiasing, true);
        QRandomGenerator rng(20331u);

        for (int i = 0; i < N * 55; ++i) {
            const int x = rng.bounded(N);
            const int y = rng.bounded(N);
            const bool light = (rng.bounded(100) < 58);
            const int a = 5 + rng.bounded(15);
            p.setPen(light ? QColor(255, 250, 238, a) : QColor(18, 15, 11, a));
            p.drawPoint(x, y);
        }
        for (int i = 0; i < 460; ++i) {
            const qreal x = rng.bounded(N);
            const qreal y = rng.bounded(N);
            const qreal len = 3 + rng.bounded(15);
            const qreal ang = rng.bounded(360) * 0.0174532925;
            p.setPen(QPen(QColor(255, 249, 235, 7 + rng.bounded(9)), 0.7));
            p.drawLine(QPointF(x, y), QPointF(x + std::cos(ang) * len, y + std::sin(ang) * len));
        }
        p.end();
        return im;
    }();
    return img;
}

// ── 低频道斑驳（油墨不匀 / 纸面吸墨差异）─────────────────────────
static const QImage &inkMottleTexture()
{
    static const QImage img = [] {
        const int N = 320;
        QImage im(N, N, QImage::Format_ARGB32_Premultiplied);
        im.fill(Qt::transparent);
        QPainter p(&im);
        p.setRenderHint(QPainter::Antialiasing, true);
        QRandomGenerator rng(9182u);
        for (int i = 0; i < 46; ++i) {
            const qreal x = rng.bounded(N);
            const qreal y = rng.bounded(N);
            const qreal r = 26 + rng.bounded(78);
            QRadialGradient g(x, y, r);
            const int a = 5 + rng.bounded(9);
            g.setColorAt(0.0, QColor(24, 19, 12, a));
            g.setColorAt(0.55, QColor(24, 19, 12, a / 2));
            g.setColorAt(1.0, QColor(24, 19, 12, 0));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawEllipse(QPointF(x, y), r, r);
        }
        p.end();
        return im;
    }();
    return img;
}

// ── 模切边缘的轻微磨损（paper chip）───────────────────────────────
static void paintEdgeWear(QPainter *p, const QPainterPath &face)
{
    static QVector<qreal> ts, rs;
    if (ts.isEmpty()) {
        QRandomGenerator rng(5150u);
        for (int i = 0; i < 34; ++i) {
            ts << rng.generateDouble();
            rs << 1.8 + rng.generateDouble() * 4.0;
        }
    }
    p->save();
    p->setPen(Qt::NoPen);
    for (int i = 0; i < ts.size(); ++i) {
        const QPointF pt = face.pointAtPercent(ts[i]);
        QPointF d(500.0 - pt.x(), 500.0 - pt.y());
        const qreal len = std::hypot(d.x(), d.y());
        if (len < 1.0) continue;
        d /= len;
        const qreal r = rs[i];
        p->setBrush(QColor(246, 239, 224, 14 + int(r * 4)));
        p->drawEllipse(pt + d * (r * 0.45), r, r * 0.72);
    }
    p->restore();
}

static void drawFitText(QPainter *p, const QString &text, const QRectF &box,
                        const QStringList &families, int weight, const QColor &color,
                        qreal maxPx, qreal minPx, Qt::Alignment align,
                        qreal spacingPct = 100.0, qreal stretch = 100.0)
{
    if (text.isEmpty() || box.width() <= 0 || box.height() <= 0)
        return;

    QFont f;
    f.setFamilies(families);
    f.setWeight(QFont::Weight(weight));
    f.setStretch(int(stretch));
    f.setLetterSpacing(QFont::PercentageSpacing, spacingPct);
    f.setPixelSize(int(maxPx));

    QFontMetricsF fm(f);
    qreal w = qMax<qreal>(1.0, fm.horizontalAdvance(text));
    qreal h = qMax<qreal>(1.0, fm.height());
    qreal px = maxPx;
    if (w > box.width() || h > box.height())
        px = maxPx * qMin(box.width() / w, box.height() / h);
    px = qBound(minPx, px, maxPx);

    f.setPixelSize(int(qMax<qreal>(1.0, px)));
    p->setFont(f);
    p->setPen(color);

    QFontMetricsF fm2(f);
    const qreal adv = fm2.horizontalAdvance(text);
    QRectF br = fm2.boundingRect(text);

    qreal x;
    if (align & Qt::AlignLeft)       x = box.left();
    else if (align & Qt::AlignRight) x = box.right() - adv;
    else                             x = box.center().x() - adv / 2.0;

    qreal y = box.center().y() - (br.top() + br.bottom()) / 2.0;
    p->drawText(QPointF(x, y), text);
}

static void drawArrowHead(QPainter *p, const QPointF &tip, const QPointF &from, qreal len)
{
    QLineF l(from, tip);
    if (l.length() < 0.001) return;
    const qreal ang = std::atan2(-(tip.y() - from.y()), tip.x() - from.x());
    const qreal spread = 0.46;
    QPolygonF tri;
    tri << tip
        << QPointF(tip.x() - len * std::cos(ang - spread), tip.y() + len * std::sin(ang - spread))
        << QPointF(tip.x() - len * std::cos(ang + spread), tip.y() + len * std::sin(ang + spread));
    p->setBrush(p->pen().color());
    p->drawPolygon(tri);
}

// ══════════════════════════════════════════════════════════════════
//  北约军兵种符号
// ══════════════════════════════════════════════════════════════════
void paintNatoSymbol(QPainter *p, const QRectF &frame, const QColor &color, Branch b)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const qreal w = frame.width();
    const qreal h = frame.height();
    const qreal cx = frame.center().x();
    const qreal cy = frame.center().y();
    const qreal lw = qMax<qreal>(0.7, w * 0.052);

    const QPen pen(color, lw, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);

    // 外框：直角矩形
    p->drawRect(frame);

    const qreal i = lw * 0.5;
    const QRectF inner = frame.adjusted(i, i, -i, -i);

    auto drawX = [&]() {
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawLine(inner.topLeft(), inner.bottomRight());
        p->drawLine(inner.topRight(), inner.bottomLeft());
    };

    // 单斜杠（骑兵 / 侦察）：mult 为相对框线的线宽倍数。
    // 两端各回缩半个线宽，圆头正好切于内框直角，不会鼓出一坨糊住角部。
    auto drawSlash = [&](qreal mult) {
        const qreal sw = lw * mult;
        const QLineF ln(inner.bottomLeft(), inner.topRight());
        if (ln.length() < 1.0) return;
        const QPointF d = (ln.p2() - ln.p1()) / ln.length();
        p->setPen(QPen(color, sw, Qt::SolidLine, Qt::RoundCap));
        p->setBrush(Qt::NoBrush);
        p->drawLine(ln.p1() + d * (sw * 0.5), ln.p2() - d * (sw * 0.5));
        p->setPen(pen);
    };

    // 把 hole 从画布上"挖掉"，再画 X —— 避免修饰符与交叉线糊在一起
    auto drawXErasing = [&](const QPainterPath &hole) {
        if (hole.isEmpty()) { drawX(); return; }
        QPainterPath clip;
        clip.addRect(frame.adjusted(-6, -6, 6, 6));
        clip = clip.subtracted(hole);
        p->save();
        p->setClipPath(clip, Qt::IntersectClip);
        drawX();
        p->restore();
    };

    auto bandOf = [&](const QPainterPath &path, qreal width) {
        QPainterPathStroker st;
        st.setWidth(width);
        st.setCapStyle(Qt::RoundCap);
        st.setJoinStyle(Qt::RoundJoin);
        return st.createStroke(path);
    };

    auto capsule = [&](qreal rx, qreal ry) {
        QPainterPath c;
        c.addRoundedRect(QRectF(cx - rx, cy - ry, rx * 2, ry * 2), ry, ry);
        return c;
    };

    auto disc = [&](qreal rx, qreal ry, bool filled) {
        if (filled) {
            p->setPen(Qt::NoPen);
            p->setBrush(color);
            p->drawEllipse(QPointF(cx, cy), rx, ry);
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
        } else {
            p->setPen(pen);
            p->setBrush(Qt::NoBrush);
            p->drawEllipse(QPointF(cx, cy), rx, ry);
        }
    };

    switch (b) {
    case Branch::Infantry:                                   // 步兵
        drawX();
        break;

    case Branch::Motorized: {                                // 摩托化步兵
        const QPainterPath c = capsule(w * 0.215, h * 0.105);
        drawXErasing(c.united(bandOf(c, lw * 1.7)));
        p->setPen(pen);
        p->drawPath(c);
        break;
    }

    case Branch::Mechanized: {                               // 装甲步兵
        const QPainterPath c = capsule(w * 0.205, h * 0.100);
        drawXErasing(c);
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawPath(c);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::Armor:                                      // 装甲兵
        p->drawEllipse(QPointF(cx, cy), w * 0.345, h * 0.265);
        break;

    case Branch::Cavalry: {                                  // 骑兵
        drawSlash(1.55);                                     // 单斜杠，略重于框线
        break;
    }

    case Branch::Artillery:                                  // 炮兵
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(QPointF(cx, cy), h * 0.165, h * 0.165);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;

    case Branch::Rocket: {                                   // 火箭炮
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(QPointF(cx - w * 0.02, cy + h * 0.055), h * 0.135, h * 0.135);
        p->setPen(pen);
        QPainterPath arrow;
        arrow.moveTo(cx - w * 0.02, cy + h * 0.055);
        arrow.lineTo(frame.right() - lw * 2.4, frame.top() + lw * 2.4);
        p->drawPath(arrow);
        drawArrowHead(p, QPointF(frame.right() - lw * 2.4, frame.top() + lw * 2.4),
                      QPointF(cx - w * 0.02, cy + h * 0.055), lw * 3.2);
        break;
    }

    case Branch::Mortar: {                                   // 迫击炮
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(QPointF(cx, cy + h * 0.135), h * 0.135, h * 0.135);
        p->setPen(pen);
        p->drawLine(QPointF(cx, cy + h * 0.005), QPointF(cx, frame.top() + lw * 2.6));
        drawArrowHead(p, QPointF(cx, frame.top() + lw * 2.6), QPointF(cx, cy + h * 0.135),
                      lw * 3.2);
        break;
    }

    case Branch::AntiTank: {                                 // 反坦克
        p->drawEllipse(QPointF(cx, cy), w * 0.300, h * 0.225);
        const QPointF from(cx - w * 0.255, cy + h * 0.180);
        const QPointF to(frame.right() - lw * 1.8, frame.top() + lw * 2.2);
        p->drawLine(from, to);
        drawArrowHead(p, to, from, lw * 3.0);
        break;
    }

    case Branch::AntiAir: {                                  // 防空
        p->drawArc(QRectF(cx - w * 0.285, cy - h * 0.315, w * 0.570, h * 0.570),
                   20 * 16, 140 * 16);
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(QPointF(cx, cy + h * 0.155), h * 0.115, h * 0.115);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::Engineer:                                   // 工兵
        for (int k = 0; k < 3; ++k) {
            const qreal y = cy + h * (k - 1) * 0.20;
            p->drawLine(QPointF(inner.left(), y), QPointF(inner.right(), y));
        }
        break;

    case Branch::Recon:                                      // 侦察
        drawSlash(0.85);                                     // 细斜杠，明显轻于骑兵
        break;

    case Branch::ArmoredRecon:                               // 装甲侦察
        p->drawEllipse(QPointF(cx, cy), w * 0.300, h * 0.232);
        drawSlash(0.85);
        break;

    case Branch::Signal: {                                   // 通信：闪电
        QPolygonF bolt;
        bolt << QPointF(cx + w * 0.115, frame.top() + h * 0.155)
             << QPointF(cx - w * 0.150, cy + h * 0.030)
             << QPointF(cx - w * 0.020, cy + h * 0.030)
             << QPointF(cx - w * 0.105, frame.bottom() - h * 0.155)
             << QPointF(cx + w * 0.150, cy - h * 0.045)
             << QPointF(cx + w * 0.020, cy - h * 0.045);
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawPolygon(bolt);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::HQ: {                                       // 指挥部：旗
        const qreal baseY = frame.bottom() - h * 0.145;
        p->setPen(QPen(color, lw * 1.6, Qt::SolidLine, Qt::FlatCap));
        p->drawLine(QPointF(cx - w * 0.205, baseY), QPointF(cx + w * 0.205, baseY));
        p->drawLine(QPointF(cx - w * 0.105, baseY), QPointF(cx - w * 0.105, frame.top() + h * 0.110));
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        QPolygonF flag;
        flag << QPointF(cx - w * 0.105, frame.top() + h * 0.110)
             << QPointF(cx + w * 0.170, frame.top() + h * 0.110)
             << QPointF(cx + w * 0.170, frame.top() + h * 0.330)
             << QPointF(cx - w * 0.105, frame.top() + h * 0.330);
        p->drawPolygon(flag);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::Airborne: {                                 // 空降：X + 翼
        drawX();
        QPainterPath wing;
        wing.moveTo(cx - w * 0.320, frame.top() + h * 0.300);
        wing.cubicTo(cx - w * 0.175, frame.top() + h * 0.015,
                     cx + w * 0.175, frame.top() + h * 0.015,
                     cx + w * 0.320, frame.top() + h * 0.300);
        p->setPen(QPen(color, lw * 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(wing);
        p->setPen(pen);
        break;
    }

    case Branch::Mountain: {                                 // 山地
        QPolygonF ridge;
        const qreal baseY = frame.bottom() - h * 0.095;
        ridge << QPointF(cx - w * 0.330, baseY)
              << QPointF(cx - w * 0.120, cy - h * 0.080)
              << QPointF(cx - w * 0.020, cy + h * 0.045)
              << QPointF(cx + w * 0.080, cy - h * 0.015)
              << QPointF(cx + w * 0.325, baseY);
        drawX();
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawPolygon(ridge);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::Marine: {                                   // 海军陆战队：锚
        QPainterPath anchor;
        anchor.addEllipse(QRectF(cx - h * 0.062, cy - h * 0.335, h * 0.124, h * 0.124));
        anchor.moveTo(cx, cy - h * 0.215);
        anchor.lineTo(cx, cy + h * 0.205);
        anchor.moveTo(cx - w * 0.130, cy - h * 0.075);
        anchor.lineTo(cx + w * 0.130, cy - h * 0.075);
        QPainterPath arms;
        arms.moveTo(cx - w * 0.150, cy + h * 0.115);
        arms.cubicTo(cx - w * 0.150, cy + h * 0.290,
                     cx + w * 0.150, cy + h * 0.290,
                     cx + w * 0.150, cy + h * 0.115);
        drawXErasing(bandOf(bandOf(anchor, lw * 2.2).united(bandOf(arms, lw * 2.2)), lw));
        p->setPen(QPen(color, lw * 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(anchor);
        p->drawPath(arms);
        p->setPen(pen);
        break;
    }

    case Branch::Supply: {                                   // 补给：板条箱
        const QRectF box(cx - w * 0.225, cy - h * 0.170, w * 0.450, h * 0.340);
        p->drawRect(box);
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawRect(QRectF(box.left(), cy - h * 0.026, box.width(), h * 0.052));
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    case Branch::Transport: {                                // 运输
        p->drawLine(QPointF(cx - w * 0.260, cy - h * 0.045), QPointF(cx + w * 0.260, cy - h * 0.045));
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(QPointF(cx - w * 0.135, cy + h * 0.145), h * 0.100, h * 0.100);
        p->drawEllipse(QPointF(cx + w * 0.135, cy + h * 0.145), h * 0.100, h * 0.100);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        break;
    }

    default:
        drawX();
        break;
    }

    p->restore();
}

// ══════════════════════════════════════════════════════════════════
//  车辆剪影（100 x 56 设计空间，车头朝左）
// ══════════════════════════════════════════════════════════════════
namespace {

// 设计空间固定 100 × 60，地面线在 y = 56，车头朝左
QPainterPath rrPath(qreal x, qreal y, qreal w, qreal h, qreal r)
{
    QPainterPath p;
    p.addRoundedRect(QRectF(x, y, w, h), r, r);
    return p;
}

QPainterPath circlePath(qreal cx, qreal cy, qreal r)
{
    QPainterPath p;
    p.addEllipse(QPointF(cx, cy), r, r);
    return p;
}

QPainterPath polyPath(std::initializer_list<QPointF> pts)
{
    QPainterPath p;
    bool first = true;
    for (const QPointF &pt : pts) {
        if (first) { p.moveTo(pt); first = false; }
        else       { p.lineTo(pt); }
    }
    p.closeSubpath();
    return p;
}

// 车轮：外胎挖掉轮毂，读起来才像轮子
QPainterPath wheelPath(qreal cx, qreal cy, qreal r, qreal hub)
{
    return circlePath(cx, cy, r).subtracted(circlePath(cx, cy, hub));
}

// 在车体上沿车轮挖出 1.3 单位宽的轮拱缝
void cutArch(QPainterPath &body, qreal cx, qreal cy, qreal r)
{
    body = body.subtracted(circlePath(cx, cy, r + 0.55).subtracted(circlePath(cx, cy, r)));
}

// 履带环 + 负重轮
QPainterPath trackPath(qreal x, qreal y, qreal w, qreal h, int n, qreal r)
{
    const QPainterPath outer = rrPath(x, y, w, h, h / 2.0);
    const QPainterPath inner = rrPath(x + 3.2, y + 3.2, w - 6.4, h - 6.4, (h - 6.4) / 2.0);
    QPainterPath p = outer.subtracted(inner);
    const qreal cy = y + h / 2.0;
    const qreal step = (w - r * 4.0) / qMax(1, n - 1);
    p.addPath(wheelPath(x + r * 2.0, cy, r * 1.18, r * 0.5));      // 诱导轮
    for (int i = 1; i < n - 1; ++i)
        p.addPath(wheelPath(x + r * 2.0 + i * step, cy, r, r * 0.42));
    p.addPath(wheelPath(x + w - r * 2.0, cy, r * 1.18, r * 0.5));  // 主动轮
    return p;
}

QPainterPath tankPath()
{
    QPainterPath p = trackPath(3, 38, 94, 18, 6, 4.2);

    p.addPath(polyPath({{4, 38}, {7, 30}, {18, 28}, {86, 28}, {93, 33}, {96, 38}}));
    p.addPath(rrPath(83, 23.6, 10, 4.4, 1.2));                     // 尾部工具箱
    p.addPath(polyPath({{25, 28}, {30.5, 17.2}, {50, 16.4}, {60, 16.8}, {66, 18.8}, {70, 28}}));
    p.addPath(rrPath(38, 12.6, 9.4, 4.4, 1.6));                    // 指挥塔
    p.addPath(polyPath({{24, 21.6}, {30, 18.4}, {32, 25.6}, {25, 27.6}}));  // 防盾
    p.addPath(polyPath({{3, 19.8}, {27, 18.6}, {27, 22.6}, {3, 24.2}}));    // 炮管
    p.addPath(rrPath(1, 18.8, 6, 6.2, 1.2));                       // 炮口制退器
    p.addPath(polyPath({{64, 18.0}, {64.8, 17.8}, {70.2, 4.0}, {69.3, 4.2}})); // 天线
    return p;
}

QPainterPath armoredCarPath()
{
    // 车体：低矮、首尾略收
    QPainterPath body = polyPath({{7.6, 44.4}, {9.4, 31.0}, {19.0, 26.4}, {44.0, 25.0},
                                  {76.0, 26.4}, {87.0, 31.0}, {88.6, 44.4}});
    cutArch(body, 26, 45.4, 7.4);
    cutArch(body, 73, 45.4, 7.4);

    // 封闭式炮塔（斜面）+ 炮座 + 细长机炮
    body.addPath(polyPath({{36.4, 26.6}, {40.4, 14.6}, {56.4, 14.2}, {60.6, 26.6}}));
    body.addPath(rrPath(36.2, 18.2, 4.8, 5.2, 1.2));                 // 炮座
    body.addPath(polyPath({{36.2, 19.0}, {20.8, 19.6}, {20.8, 21.0}, {36.2, 21.4}})); // 机炮
    body.addPath(rrPath(18.8, 18.8, 3.0, 3.0, 0.8));                 // 消焰器
    body.addPath(polyPath({{47.0, 14.4}, {52.6, 14.2}, {52.6, 11.8}, {47.0, 12.0}})); // 车长塔

    body.addPath(wheelPath(26, 45.4, 7.4, 1.9));
    body.addPath(wheelPath(73, 45.4, 7.4, 1.9));
    return body;
}

QPainterPath truckPath()
{
    QPainterPath body = rrPath(29, 19, 63, 24.4, 1.5);             // 货厢
    body.addPath(polyPath({{6, 43.4}, {6, 24}, {16, 24}, {24, 30.4}, {30, 43.4}})); // 驾驶室
    body.addPath(rrPath(3, 40.4, 5.4, 4.6, 1.2));                  // 保险杠
    cutArch(body, 20, 46.4, 6.9);
    cutArch(body, 76, 46.4, 6.9);

    body = body.subtracted(rrPath(8.8, 27, 8.4, 6.4, 1.2));        // 车窗
    body = body.subtracted(rrPath(33.4, 19, 1.2, 24.4, 0.5));      // 车厢前板缝

    body.addPath(wheelPath(20, 46.4, 6.9, 1.7));
    body.addPath(wheelPath(76, 46.4, 6.9, 1.7));
    return body;
}

QPainterPath halftrackPath()
{
    // 前部车体（带倾斜首上装甲）
    QPainterPath body = polyPath({{5, 41}, {7, 29}, {14, 26}, {27, 26}, {29, 41}});
    // 后部敞开式载员舱
    body.addPath(rrPath(27, 23.4, 67, 16.6, 2.0));
    body.addPath(rrPath(27, 21.6, 67, 2.6, 1.2));                  // 舱壁上沿
    body.addPath(rrPath(6, 20.8, 9.6, 5.4, 1.0));                  // 机枪防盾
    body.addPath(rrPath(33, 21.0, 6.4, 3.2, 1.0));                 // 舱内机枪
    cutArch(body, 17, 47, 6.4);
    body = body.subtracted(rrPath(9.6, 29.4, 10.4, 5.6, 1.0));     // 风挡
    body = body.subtracted(rrPath(31, 27.6, 60, 1.4, 0.6));        // 舱壁分缝

    QPainterPath tr = trackPath(41, 38.6, 53, 16, 4, 3.6);
    body.addPath(tr);
    body.addPath(wheelPath(17, 47, 6.4, 2.2));
    return body;
}

QPainterPath motorcyclePath()
{
    // 车头朝左，与其它车辆一致
    QPainterPath body;

    // 车架/发动机：一整块实心楔形
    body.addPath(polyPath({{28, 33.4}, {62, 33.0}, {74, 39.2}, {78, 44.2},
                           {30, 44.2}, {24, 38.6}}));
    // 油箱 + 车座
    body.addPath(rrPath(31.5, 24.6, 20.0, 9.2, 3.2));
    body.addPath(rrPath(50.0, 28.4, 14.6, 5.2, 2.0));
    // 前叉（粗一点，读得出是前悬）
    body.addPath(polyPath({{16.4, 46.6}, {22.4, 46.6}, {31.4, 27.4}, {26.6, 26.6}}));
    // 车把
    body.addPath(polyPath({{27.0, 23.4}, {34.4, 24.4}, {34.0, 26.4}, {26.6, 25.6}}));
    // 后摇臂
    body.addPath(polyPath({{62, 36.0}, {80, 42.6}, {78.4, 45.4}, {60, 38.8}}));
    // 排气管
    body.addPath(polyPath({{58, 38.0}, {80.6, 40.6}, {80.2, 42.6}, {57.6, 40.2}}));

    // 骑手：前倾姿态
    body.addPath(circlePath(43.6, 11.4, 4.4));                                     // 头盔
    body.addPath(polyPath({{39.4, 15.8}, {47.8, 14.8}, {51.6, 29.6}, {42.2, 30.8}})); // 躯干
    body.addPath(polyPath({{46.6, 16.4}, {49.2, 15.6}, {33.8, 24.4}, {32.2, 22.6}})); // 手臂→车把
    body.addPath(polyPath({{41.4, 28.6}, {46.6, 27.8}, {49.4, 37.4}, {43.8, 38.4}})); // 大腿
    body.addPath(polyPath({{44.2, 36.6}, {47.4, 36.0}, {50.2, 45.2}, {46.6, 45.8}})); // 小腿
    body.addPath(rrPath(44.2, 44.6, 8.0, 3.0, 1.2));                               // 脚踏

    body.addPath(wheelPath(19, 46.6, 7.0, 1.9));
    body.addPath(wheelPath(82, 46.6, 7.0, 1.9));
    return body;
}

QPainterPath spgPath()
{
    QPainterPath p = trackPath(3, 38, 94, 18, 6, 4.2);
    p.addPath(polyPath({{4, 38}, {7, 30}, {16, 28}, {88, 28}, {94, 33}, {96, 38}}));
    p.addPath(polyPath({{14, 28}, {22, 16}, {63, 16}, {70, 28}}));         // 战斗室
    p.addPath(polyPath({{2, 20.4}, {24, 19.4}, {24, 23.6}, {2, 25.0}}));    // 炮管
    p.addPath(rrPath(0.4, 19.4, 5.6, 5.8, 1.0));                           // 制退器
    p.addPath(rrPath(45, 12.6, 9, 3.6, 1.5));                              // 车长塔
    return p;
}

QPainterPath helicopterPath()
{
    QPainterPath p = rrPath(24, 17, 42, 19, 9.5);                  // 机身
    p.addPath(polyPath({{24, 21.6}, {17.4, 25.6}, {20, 28.4}, {24, 31}})); // 机鼻
    p.addPath(polyPath({{64, 23}, {97, 20.4}, {97, 27}, {64, 32}}));       // 尾梁
    p.addPath(polyPath({{91, 21}, {97, 10}, {97, 21}}));                   // 垂尾
    p.addPath(rrPath(95, 13.6, 2, 10, 1));                                 // 尾桨
    p.addPath(rrPath(43, 11, 4, 7, 1));                                    // 桨毂
    p.addPath(rrPath(7, 8.4, 86, 2.2, 1.1));                               // 主旋翼
    p.addPath(rrPath(26, 48, 34, 2.2, 1.1));                               // 滑橇
    p.addPath(polyPath({{31, 35}, {33, 35}, {33, 48}, {31, 48}}));
    p.addPath(polyPath({{55, 35}, {57, 35}, {57, 48}, {55, 48}}));
    p = p.subtracted(rrPath(28, 20, 13, 8, 3.5));                          // 座舱玻璃
    return p;
}

// 补给队的卡车标记：设计空间 100 × 56，地面线 y ≈ 52.4，车头朝左
// 按真车比例画 2.5 吨军卡（车长 6.7 m、车高 2.9 m、轮径 1.1 m 折算到 86 × 37 单位）：
// 帆布篷顶只比驾驶室顶高一点点——之前篷布比车顶高出太多，看着像个集装箱。
constexpr qreal kTruckBot  = 40.0;    // 底盘下缘
constexpr qreal kTruckTop  = 14.8;    // 车厢顶
constexpr qreal kTruckRoof = 18.0;    // 驾驶室顶
constexpr qreal kTruckHood = 32.4;    // 机罩顶
constexpr qreal kTruckBedL = 30.0;    // 车厢前壁
constexpr qreal kTruckBedR = 94.0;    // 车厢尾板
constexpr qreal kTruckWr   = 6.8;     // 车轮半径
constexpr qreal kTruckWy   = 46.0;    // 轮心
constexpr qreal kTruckFw   = 18.0;    // 前轮
constexpr qreal kTruckRw   = 73.5;    // 后轮
constexpr int   kTruckScall = 3;      // 篷布三段

QPainterPath truckBodyPath()
{
    QPainterPath p;

    // 车厢：直角箱体，顶角略收
    QPainterPath bed;
    bed.moveTo(kTruckBedL, kTruckBot);
    bed.lineTo(kTruckBedL, kTruckTop + 1.4);
    bed.quadTo(kTruckBedL, kTruckTop, kTruckBedL + 1.4, kTruckTop);
    bed.lineTo(kTruckBedR - 1.4, kTruckTop);
    bed.quadTo(kTruckBedR, kTruckTop, kTruckBedR, kTruckTop + 1.4);
    bed.lineTo(kTruckBedR, kTruckBot);
    bed.closeSubpath();
    p.addPath(bed);

    // 驾驶室：竖直前脸 → 一道斜风挡 → 平顶（比车厢低一截）
    QPainterPath cab;
    cab.moveTo(5.8, kTruckBot);
    cab.lineTo(5.8, 29.4);
    cab.lineTo(11.4, 22.4);
    cab.lineTo(kTruckBedL, 22.4);
    cab.lineTo(kTruckBedL, kTruckBot);
    cab.closeSubpath();
    p.addPath(cab);

    p.addPath(circlePath(kTruckFw, kTruckWy, kTruckWr));
    p.addPath(circlePath(kTruckRw, kTruckWy, kTruckWr));
    return p;
}

QPainterPath truckArchRing(qreal cx)
{
    QPainterPath ring;
    ring.setFillRule(Qt::OddEvenFill);
    ring.addEllipse(QPointF(cx, kTruckWy), kTruckWr + 1.5, kTruckWr + 1.5);
    ring.addEllipse(QPointF(cx, kTruckWy), kTruckWr, kTruckWr);
    return ring;
}

} // namespace

// 画在临时透明图上：整车一次填色，再用 CompositionMode_Clear 刻线。
// 这样圆和曲线都不经过 QPainterPath 的布尔运算（布尔会把曲线拉直成多边形）。
void paintSupplyTruck(QPainter *p, const QRectF &box, const QColor &color)
{
    const qreal sc = qMin(box.width() / 100.0, box.height() / 56.0);
    if (sc <= 0) return;

    const int wpx = qMax(1, int(std::ceil(100 * sc)) + 2);
    const int hpx = qMax(1, int(std::ceil(56 * sc)) + 2);
    QImage img(wpx, hpx, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    {
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing, true);
        q.scale(sc, sc);

        // 实心平涂：整车一次填色，再用 CompositionMode_Clear 刻线。
        // 这样圆和曲线都不经过 QPainterPath 的布尔运算（布尔会把曲线拉直成多边形）。
        q.setPen(Qt::NoPen);
        q.setBrush(color);
        q.drawPath(truckBodyPath());

        q.setCompositionMode(QPainter::CompositionMode_Clear);

        q.drawPath(truckArchRing(kTruckFw));                    // 轮拱缝
        q.drawPath(truckArchRing(kTruckRw));
        q.drawEllipse(QPointF(kTruckFw, kTruckWy), 2.4, 2.4);   // 轮毂孔
        q.drawEllipse(QPointF(kTruckRw, kTruckWy), 2.4, 2.4);

        q.fillPath(rrPath(13.2, 25.2, 11.6, 6.8, 1.3), Qt::white);   // 驾驶室侧窗

        // 车厢：两道竖向压条
        for (int i = 1; i <= 2; ++i) {
            const qreal x = kTruckBedL + (kTruckBedR - kTruckBedL) * i / 3.0;
            q.fillPath(rrPath(x - 0.7, kTruckTop + 2.6, 1.4, kTruckBot - kTruckTop - 5.2, 0.5),
                       Qt::white);
        }
    }

    p->drawImage(QPointF(box.center().x() - 50 * sc, box.center().y() - 28.4 * sc), img);
}

// 线稿版：等宽白线，笔触与北约军标一致
void paintSupplyTruckLine(QPainter *p, const QRectF &box, const QColor &color)
{
    const qreal sc = qMin(box.width() / 100.0, box.height() / 56.0);
    if (sc <= 0) return;

    const int wpx = qMax(1, int(std::ceil(100 * sc)) + 2);
    const int hpx = qMax(1, int(std::ceil(56 * sc)) + 2);
    QImage img(wpx, hpx, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    {
        QPainter q(&img);
        q.setRenderHint(QPainter::Antialiasing, true);
        q.scale(sc, sc);

        // 线稿：笔触与北约军标一致（等宽白线、圆头圆角）
        q.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        q.setBrush(Qt::NoBrush);

        q.drawPath(truckBodyPath());

        // 车轮 + 轮毂
        q.drawEllipse(QPointF(kTruckFw, kTruckWy), kTruckWr, kTruckWr);
        q.drawEllipse(QPointF(kTruckRw, kTruckWy), kTruckWr, kTruckWr);
        q.drawEllipse(QPointF(kTruckFw, kTruckWy), 2.5, 2.5);
        q.drawEllipse(QPointF(kTruckRw, kTruckWy), 2.5, 2.5);

        // 驾驶室侧窗
        q.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        q.drawRoundedRect(QRectF(13.2, 25.2, 11.4, 6.6), 1.3, 1.3);

        // 篷布加强筋
        q.setPen(QPen(color, 1.9, Qt::SolidLine, Qt::RoundCap));
        for (int i = 1; i < kTruckScall; ++i) {
            const qreal x = kTruckBedL + (kTruckBedR - kTruckBedL) * i / kTruckScall;
            q.drawLine(QPointF(x, kTruckTop + 1.8), QPointF(x, kTruckBot - 1.6));
        }
    }

    p->drawImage(QPointF(box.center().x() - 50 * sc, box.center().y() - 28.4 * sc), img);
}

// ══════════════════════════════════════════════════════════════════
//  规模符号：北约梯队标记（I / II / X / XX …）
// ══════════════════════════════════════════════════════════════════
void paintEchelon(QPainter *p, const QPointF &center, qreal height,
                  const QColor &color, Echelon e)
{
    if (e == Echelon::None || height <= 0) return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    if (e == Echelon::Squad || e == Echelon::Section || e == Echelon::Platoon) {
        const int n = (e == Echelon::Squad) ? 1 : (e == Echelon::Section ? 2 : 3);
        const qreal d = height * 0.28;
        const qreal gap = height * 0.17;
        const qreal total = n * d + (n - 1) * gap;
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        for (int i = 0; i < n; ++i)
            p->drawEllipse(QPointF(center.x() - total / 2 + d / 2 + i * (d + gap), center.y()),
                           d / 2, d / 2);
        p->restore();
        return;
    }

    const QString g = echelonGlyph(e);
    QFont f;
    f.setFamilies(condensedFamilies());
    f.setWeight(QFont::Bold);
    f.setLetterSpacing(QFont::PercentageSpacing, 88);
    f.setPixelSize(int(height));

    QFontMetricsF fm(f);
    qreal px = height;
    const qreal w = fm.horizontalAdvance(g);
    if (w > height * 2.7) px = height * (height * 2.7) / w;
    f.setPixelSize(int(qMax<qreal>(1.0, px)));

    QFontMetricsF fm2(f);
    const qreal adv = fm2.horizontalAdvance(g);
    const QRectF br = fm2.boundingRect(g);

    p->setFont(f);
    p->setPen(color);
    p->drawText(QPointF(center.x() - adv / 2.0, center.y() - (br.top() + br.bottom()) / 2.0), g);

    p->restore();
}

// ══════════════════════════════════════════════════════════════════
//  堆叠圈（右上角）：只有一个圈，两级 —— 空圈 = 1，实心圈 = 2
//  不印数字：看一眼是空是实就知道占几点
// ══════════════════════════════════════════════════════════════════
void paintStackMark(QPainter *p, const QPointF &center, qreal diameter,
                    const QColor &color, int stacking)
{
    if (diameter <= 0) return;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const qreal d  = diameter;
    const qreal lw = qMax<qreal>(1.0, d * 0.16);

    if (stacking >= 2) {
        // 实心圈：直径取空圈的中线，实心不会显得比空圈更大
        const qreal r = (d - lw) * 0.5;
        p->setPen(Qt::NoPen);
        p->setBrush(color);
        p->drawEllipse(center, r, r);
    } else {
        const qreal r = (d - lw) * 0.5;
        p->setPen(QPen(color, lw, Qt::SolidLine, Qt::FlatCap));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(center, r, r);
    }

    p->restore();
}

// ══════════════════════════════════════════════════════════════════
//  数值排版：战斗力 - 行动力
// ══════════════════════════════════════════════════════════════════
static const QStringList &valueFamilies()
{
    static const QStringList f{QStringLiteral("Segoe UI Black"),
                               QStringLiteral("Segoe UI"),
                               QStringLiteral("Arial")};
    return f;
}

static void drawSingleValue(QPainter *p, const QRectF &box, const QString &text,
                            const QColor &color)
{
    QFont f;
    f.setFamilies(valueFamilies());
    f.setWeight(QFont::Black);
    f.setStretch(96);
    f.setLetterSpacing(QFont::PercentageSpacing, 100);
    f.setPixelSize(100);

    QFontMetricsF fm(f);
    const qreal tw = fm.horizontalAdvance(text);
    const qreal cap = qMax<qreal>(1.0, fm.capHeight());
    const qreal k = qMin(box.width() * 0.62 / qMax<qreal>(1.0, tw),
                         box.height() * 0.68 / cap);
    const qreal px = qBound<qreal>(12.0, 100.0 * k, 372.0);
    f.setPixelSize(int(px));

    QFontMetricsF fm2(f);
    const qreal adv = fm2.horizontalAdvance(text);
    const qreal capH = fm2.capHeight();
    p->setFont(f);
    p->setPen(color);
    p->drawText(QPointF(box.center().x() - adv / 2.0,
                        box.center().y() + capH / 2.0 - capH * 0.012), text);
}

static void drawValues(QPainter *p, const QRectF &box, const QString &left,
                       const QString &right, const QColor &color, qreal maxPx)
{
    // 以 100px 为基准量一次，再按目标宽高整体缩放，保证数字总是顶格
    QFont f;
    f.setFamilies(valueFamilies());
    f.setWeight(QFont::Black);
    f.setStretch(94);
    f.setLetterSpacing(QFont::PercentageSpacing, 99);
    f.setPixelSize(100);

    QFontMetricsF fm(f);
    const qreal wL = fm.horizontalAdvance(left);
    const qreal wR = fm.horizontalAdvance(right);
    const qreal wD = 15.0;          // 破折号长度（每 100px 字号）
    const qreal gap = 4.6;          // 数字与破折号的间距
    const qreal total = wL + gap + wD + gap + wR;
    const qreal cap = qMax<qreal>(1.0, fm.capHeight());

    const qreal k = qMin(box.width() * 0.80 / qMax<qreal>(1.0, total),
                         box.height() * 0.68 / cap);
    qreal px = qBound<qreal>(12.0, 100.0 * k, maxPx);

    f.setPixelSize(int(px));
    QFontMetricsF fm2(f);
    const qreal aL = fm2.horizontalAdvance(left);
    const qreal aR = fm2.horizontalAdvance(right);
    const qreal dW = px * 0.210;
    const qreal g2 = px * 0.046;
    const qreal full = aL + g2 + dW + g2 + aR;
    const qreal capH = fm2.capHeight();

    const qreal baseline = box.center().y() + capH / 2.0 - capH * 0.012;
    const qreal startX = box.center().x() - full / 2.0;

    p->setFont(f);
    p->setPen(color);
    p->drawText(QPointF(startX, baseline), left);
    p->drawText(QPointF(startX + aL + g2 + dW + g2, baseline), right);

    // 破折号：短、粗、方正
    const QRectF dash(startX + aL + g2,
                      baseline - capH * 0.559,
                      dW, capH * 0.118);
    p->setPen(Qt::NoPen);
    p->setBrush(color);
    p->drawRect(dash);
}

// ══════════════════════════════════════════════════════════════════
//  算子主体
// ══════════════════════════════════════════════════════════════════
void paintCounter(QPainter *p, const QRectF &rect, const CounterSpec &s, const RenderStyle &st)
{
    if (rect.width() <= 0 || rect.height() <= 0) return;

    const qreal side = qMin(rect.width(), rect.height());
    const QRectF box(rect.center().x() - side / 2.0, rect.center().y() - side / 2.0, side, side);
    const qreal radiusPx = side * qBound(0, s.cornerPercent, 40) / 100.0;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    // 纸板厚度：四周等量扩散的柔投影，只带一丁点向下的偏移
    // （旧版只有下方 +5%、左右 +4%、上方 0，投影重心偏低，方块会被看成竖长方形）
    if (st.shadow) {
        const qreal spread = side * 0.032;      // 四边同样外扩这么多
        const qreal dy = side * 0.006;          // 向下偏移，小到不改变外形比例
        for (int i = 8; i >= 1; --i) {
            const qreal g = spread * i / 8.0;
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, int(30.0 * (1.0 - i / 9.0))));
            p->drawRoundedRect(box.adjusted(-g, -g + dy, g, g + dy),
                               radiusPx + g, radiusPx + g);
        }
    }

    p->translate(box.topLeft());
    p->scale(side / 1000.0, side / 1000.0);

    const qreal SPLIT = 630.0;
    const qreal RAD = qMax<qreal>(0.0, 1000.0 * qBound(0, s.cornerPercent, 40) / 100.0);

    const QColor fc = printInk(s.formationColor.isValid() ? s.formationColor
                                                          : QColor(0x6E, 0x5A, 0x93));
    const QColor top = s.autoTopColor ? autoTopColor(fc) : printInk(s.topColor);
    const bool supply = (s.branch == Branch::Supply);

    if (st.paperBorder) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(246, 243, 235));
        p->drawRoundedRect(QRectF(-42, -42, 1084, 1084), RAD, RAD);
    }

    QPainterPath face;
    face.addRoundedRect(QRectF(0, 0, 1000, 1000), RAD, RAD);

    p->save();
    p->setClipPath(face);

    // 上部：近黑深色区
    QLinearGradient g1(0, 0, 0, SPLIT);
    g1.setColorAt(0.0, shade(top, 1.05));
    g1.setColorAt(1.0, shade(top, 0.95));
    p->fillRect(QRectF(0, 0, 1000, SPLIT + 1), g1);

    // 下部：编制色彩带
    QLinearGradient g2(0, SPLIT, 0, 1000);
    g2.setColorAt(0.0, shade(fc, 1.09));
    g2.setColorAt(0.50, fc);
    g2.setColorAt(1.0, shade(fc, 0.93));
    p->fillRect(QRectF(0, SPLIT, 1000, 1001 - SPLIT), g2);

    // 分界：一条硬边
    p->fillRect(QRectF(0, SPLIT - 1.2, 1000, 1.2), QColor(0, 0, 0, 80));

    // 极轻的纸纹与油墨不匀：有印刷感，但不做旧
    if (st.texture) {
        p->save();
        p->setOpacity(0.15);
        p->drawImage(QRectF(0, 0, 1000, 1000), paperGrainTexture());
        p->setCompositionMode(QPainter::CompositionMode_Multiply);
        p->setOpacity(0.08);
        p->drawImage(QRectF(0, 0, 1000, 1000), inkMottleTexture());
        p->restore();
    }

    const QColor ink(253, 251, 246, 252);
    const qreal CX = 500.0;

    // ── 中心图形：北约兵种符号（规模符号在它正上方）
    if (supply) {
        // 补给队：中心图形按样式选择（实心卡车 / 线稿卡车 / 北约补给符号）
        if (st.supplyStyle == 2) {
            paintNatoSymbol(p, QRectF(CX - 190, 288, 380, 270), ink, Branch::Supply);
        } else if (st.supplyStyle == 1) {
            paintSupplyTruckLine(p, QRectF(150, 206, 700, 372), QColor(0xF4, 0xF1, 0xEA, 240));
        } else {
            paintSupplyTruck(p, QRectF(150, 206, 700, 372), QColor(0xF4, 0xF1, 0xEA, 238));
        }
    } else {
        const qreal fw = 368.0;
        const qreal fh = 262.0;
        const qreal cy = s.showEchelon ? 452.0 : 436.0;
        paintNatoSymbol(p, QRectF(CX - fw / 2.0, cy - fh / 2.0, fw, fh), ink, s.branch);
        if (s.showEchelon)
            paintEchelon(p, QPointF(CX, 260), 80, ink, s.echelon);

        // ── 堆叠圈：军标正左边，与符号中线对齐
        if (s.showStackMark) {
            const qreal d = 96.0;
            const qreal gap = 34.0;   // 框线本身向外占半个线宽，留足才不会看起来贴住
            paintStackMark(p, QPointF(CX - fw / 2.0 - gap - d / 2.0, cy), d, ink, s.stacking);
        }
    }

    // ── 左侧小标记（○ / ★ / 空 …）
    if (!supply && !s.mark.trimmed().isEmpty()) {
        drawFitText(p, s.mark, QRectF(18, 392, 136, 116), cjkFamilies(), QFont::Bold,
                    QColor(252, 250, 244, 236), 104, 36, Qt::AlignCenter, 100);
    }

    // ── 上级 / 补给标签
    if (supply) {
        QFont cjk;
        cjk.setFamilies(cjkFamilies());
        cjk.setWeight(QFont::Bold);
        cjk.setLetterSpacing(QFont::PercentageSpacing, 104);
        cjk.setPixelSize(94);
        QFontMetricsF fmc(cjk);
        const QString cn = QString::fromUtf8("补给");
        const qreal wCn = fmc.horizontalAdvance(cn);
        p->setFont(cjk);
        p->setPen(QColor(250, 248, 242, 246));
        p->drawText(QPointF(54, 148), cn);

        QFont en;
        en.setFamilies(condensedFamilies());
        en.setWeight(QFont::DemiBold);
        en.setLetterSpacing(QFont::PercentageSpacing, 122);
        en.setPixelSize(44);
        p->setFont(en);
        p->setPen(QColor(250, 248, 242, 150));
        p->drawText(QPointF(54 + wCn + 24, 146), QStringLiteral("SUPPLY"));
    } else {
        QRectF tbox(54, 36, 900, 120);
        drawFitText(p, s.superior, tbox, condensedFamilies(), QFont::Bold,
                    QColor(250, 248, 242, 246), 106, 30,
                    Qt::AlignLeft | Qt::AlignVCenter, 101);
    }

    // ── 番号：右侧竖排（成品算子的常规做法）
    if (s.showDesignation && !s.designation.trimmed().isEmpty() && !supply) {
        p->save();
        p->translate(908, 452);
        p->rotate(-90);
        drawFitText(p, s.designation, QRectF(-204, -44, 408, 88), condensedFamilies(),
                    QFont::DemiBold, QColor(250, 248, 242, 220), 60, 18,
                    Qt::AlignCenter, 110);
        p->restore();
    }

    // ── 数值
    if (supply) {
        drawSingleValue(p, QRectF(34, SPLIT + 6, 932, 1000 - SPLIT - 12),
                        QString::number(s.movement), ink);
    } else {
        drawValues(p, QRectF(34, SPLIT + 6, 932, 1000 - SPLIT - 12),
                   QString::number(s.combat), QString::number(s.movement), ink, 372);
    }

    // 模切磨损
    if (st.weathering)
        paintEdgeWear(p, face);

    p->restore();  // 解除裁剪

    // ── 边缘：一条干净的暗线，内侧一点纸白
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(0, 0, 0, 122), 2.2));
    p->drawPath(face);
    p->setPen(QPen(QColor(255, 253, 246, 34), 1.1));
    QPainterPath innerEdge;
    innerEdge.addRoundedRect(QRectF(2.2, 2.2, 995.6, 995.6),
                             qMax<qreal>(0, RAD - 2.2), qMax<qreal>(0, RAD - 2.2));
    p->drawPath(innerEdge);

    p->restore();
}

// ══════════════════════════════════════════════════════════════════
//  输出
// ══════════════════════════════════════════════════════════════════
QImage counterImage(const CounterSpec &s, int px, const RenderStyle &st)
{
    px = qMax(16, px);
    QImage img(px, px, QImage::Format_ARGB32_Premultiplied);
    img.fill(st.paperBorder ? QColor(244, 240, 230) : Qt::transparent);
    QPainter p(&img);
    QRectF r(0, 0, px, px);
    if (st.paperBorder) {
        const qreal m = px * 0.035;
        r = r.adjusted(m, m, -m, -m);
    }
    paintCounter(&p, r, s, st);
    p.end();
    return img;
}

QImage sheetImage(const QVector<CounterSpec> &items, int dpi, qreal counterMm,
                  bool cutMarks, bool labels)
{
    if (items.isEmpty() || dpi <= 0) return QImage();

    const qreal mm2px = dpi / 25.4;
    const int pageW = int(210 * mm2px);
    const int pageH = int(297 * mm2px);
    QImage img(pageW, pageH, QImage::Format_ARGB32_Premultiplied);
    img.fill(QColor(252, 250, 244));

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // 整页纸纹，统一印刷质感
    p.save();
    p.setOpacity(0.5);
    p.drawImage(QRectF(0, 0, pageW, pageH), paperGrainTexture());
    p.restore();

    const qreal margin = 8 * mm2px;
    const qreal size = counterMm * mm2px;
    const qreal gap = 0.8 * mm2px;
    const qreal cell = size + gap;
    const qreal labelH = labels ? 3.2 * mm2px : 0;

    const int cols = qMax(1, int((pageW - 2 * margin + gap) / cell));
    const int rows = qMax(1, int((pageH - 2 * margin + gap) / (cell + labelH)));
    const int total = cols * rows;

    for (int i = 0; i < total; ++i) {
        const CounterSpec &s = items.at(i % items.size());
        const int r = i / cols;
        const int c = i % cols;
        const qreal x = margin + c * cell;
        const qreal y = margin + r * (cell + labelH);

        RenderStyle st;
        st.paperBorder = true;
        paintCounter(&p, QRectF(x, y, size, size), s, st);

        if (cutMarks) {
            p.setPen(QPen(QColor(176, 170, 156), 1.0));
            const qreal t = 1.6 * mm2px;
            const qreal x0 = x - gap * 0.5, y0 = y - gap * 0.5;
            const qreal x1 = x + size + gap * 0.5, y1 = y + size + gap * 0.5;
            p.drawLine(QPointF(x0 - t, y0), QPointF(x0, y0));
            p.drawLine(QPointF(x0, y0 - t), QPointF(x0, y0));
            p.drawLine(QPointF(x1, y0 - t), QPointF(x1, y0));
            p.drawLine(QPointF(x1, y0), QPointF(x1 + t, y0));
            p.drawLine(QPointF(x0 - t, y1), QPointF(x0, y1));
            p.drawLine(QPointF(x0, y1), QPointF(x0, y1 + t));
            p.drawLine(QPointF(x1, y1), QPointF(x1 + t, y1));
            p.drawLine(QPointF(x1, y1), QPointF(x1, y1 + t));
        }

        if (labels) {
            QFont f(QStringLiteral("Microsoft YaHei UI"));
            f.setPixelSize(int(2.2 * mm2px));
            p.setFont(f);
            p.setPen(QColor(142, 134, 118));
            p.drawText(QRectF(x, y + size, size, labelH), Qt::AlignCenter, s.title());
        }
    }

    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(int(2.4 * mm2px));
    p.setFont(f);
    p.setPen(QColor(160, 152, 134));
    p.drawText(QRectF(margin, pageH - margin + 1 * mm2px, pageW - 2 * margin, 6 * mm2px),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("CounterForge · %1 dpi · 算子边长 %2 mm").arg(dpi).arg(counterMm));

    p.end();
    return img;
}

QImage iconImage(const CounterSpec &s, int px)
{
    return counterImage(s, px, RenderStyle());
}

const QImage &paperGrain()
{
    return paperGrainTexture();
}

QColor formationPreset(int i)
{
    static const QColor c[] = {
        QColor(0x6E, 0x75, 0x63),  // 原野灰
        QColor(0x33, 0x56, 0x8C),  // 钢蓝
        QColor(0xAC, 0x3E, 0x36),  // 砖红
        QColor(0x5E, 0x7A, 0x45),  // 橄榄
        QColor(0xB5, 0x89, 0x3C),  // 赭黄
        QColor(0x6E, 0x5A, 0x93),  // 紫
        QColor(0x8C, 0x4A, 0x30),  // 铁锈
        QColor(0x3C, 0x3E, 0x42),  // 石墨
        QColor(0xA7, 0x8D, 0x63),  // 沙褐
        QColor(0x2F, 0x6A, 0x68),  // 深青
        QColor(0x7E, 0x2F, 0x3C),  // 酒红
        QColor(0x4C, 0x64, 0x78),  // 蓝灰
        QColor(0x8E, 0x7C, 0x54),  // 卡其
        QColor(0x6E, 0x71, 0x75),  // 烟灰
        QColor(0x56, 0x46, 0x6E),  // 暗紫
        QColor(0x4E, 0x56, 0x38),  // 暗橄榄
    };
    const int n = int(sizeof(c) / sizeof(c[0]));
    return c[((i % n) + n) % n];
}

QStringList formationPresetNames()
{
    return QStringList{"原野灰", "钢蓝", "砖红", "橄榄", "赭黄", "紫", "铁锈", "石墨",
                       "沙褐", "深青", "酒红", "蓝灰", "卡其", "烟灰", "暗紫", "暗橄榄"};
}

} // namespace cf
