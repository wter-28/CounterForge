// maprenderer.cpp —— 1940 年代参谋部地形图风格的兵棋地图渲染
//
// 版面坐标单位是毫米，整张图按纸张尺寸排；导出时按 dpi 缩放到位图。
// 所有纹理（树丛、芦苇、晕滃）都是程序化矢量，不依赖外部图片。
#include "maprenderer.h"

#include "counterrenderer.h"     // paperGrain()

#include <QFontMetricsF>
#include <QLineF>
#include <QLinearGradient>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStringList>
#include <QtMath>

namespace cf {
namespace {

const qreal kSqrt3 = 1.7320508075688772;

// ── 配色：老纸 + 低饱和印刷色，模拟 1940 年代石印地形图 ───────────
const QColor kPaper      (0xF2, 0xE8, 0xD2);
const QColor kPaperDeep  (0xE6, 0xD8, 0xBB);
const QColor kGridInk    (0x8A, 0x6E, 0x4E);
const QColor kInk        (0x3B, 0x2F, 0x24);
const QColor kLabelInk   (0x4A, 0x38, 0x28);
const QColor kRiver      (0x24, 0x4C, 0x66);
const QColor kRiverMinor (0x4E, 0x7B, 0x93);
const QColor kRail       (0x2B, 0x25, 0x1E);
const QColor kRoad       (0x8A, 0x62, 0x3E);
const QColor kBorder     (0x7A, 0x33, 0x2C);

QStringList serifFamilies()
{
    static const QStringList f{QStringLiteral("Source Han Serif SC"),
                               QStringLiteral("Songti SC"),
                               QStringLiteral("SimSun"),
                               QStringLiteral("Times New Roman"),
                               QStringLiteral("Georgia")};
    return f;
}

QStringList latinSerif()
{
    static const QStringList f{QStringLiteral("Times New Roman"),
                               QStringLiteral("Georgia"),
                               QStringLiteral("Cambria"),
                               QStringLiteral("SimSun")};
    return f;
}

// 由格坐标生成的稳定抖动，保证同一份数据每次画出来一模一样
qreal jitter(int c, int r, int salt)
{
    // 全部用无符号 32 位运算，避免 int 乘法溢出（salt 大时可能产生 NaN 喂给 Qt）
    const quint32 cc = quint32(c);
    const quint32 rr = quint32(r);
    const quint32 ss = quint32(salt);
    quint32 h = (cc * 73856093u) ^ (rr * 19349663u) ^ (ss * 83492791u);
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return qreal(h % 1000u) / 1000.0;
}

QPointF hexVertex(const QPointF &c, qreal R, qreal H, int i)
{
    switch (i) {
    case 0: return c + QPointF(R, 0);
    case 1: return c + QPointF(R / 2, -H / 2);
    case 2: return c + QPointF(R / 2, H / 2);
    case 3: return c + QPointF(-R, 0);
    case 4: return c + QPointF(-R / 2, -H / 2);
    default: return c + QPointF(-R / 2, H / 2);
    }
}

// 边的两个端点（0=N 1=NE 2=SE 3=S 4=SW 5=NW）
void edgeEnds(const QPointF &c, qreal R, qreal H, int e, QPointF *a, QPointF *b)
{
    static const int idx[6][2] = {{4, 1}, {1, 0}, {0, 2}, {2, 5}, {5, 3}, {3, 4}};
    *a = hexVertex(c, R, H, idx[e][0]);
    *b = hexVertex(c, R, H, idx[e][1]);
}

QPainterPath hexPath(const QPointF &c, qreal R, qreal H)
{
    // 顺时针：右 → 右上 → 左上 → 左 → 左下 → 右下
    static const int order[6] = {0, 1, 4, 3, 5, 2};
    QPainterPath path;
    path.moveTo(hexVertex(c, R, H, order[0]));
    for (int i = 1; i < 6; ++i) path.lineTo(hexVertex(c, R, H, order[i]));
    path.closeSubpath();
    return path;
}

// odd-q 偏移下的邻居
bool neighborOf(int cols, int rows, int c, int r, int e, int *nc, int *nr)
{
    const bool odd = (c & 1);
    int x = c, y = r;
    switch (e) {
    case 0: y = r - 1; break;                               // N
    case 1: x = c + 1; y = odd ? r : r - 1; break;           // NE
    case 2: x = c + 1; y = odd ? r + 1 : r; break;           // SE
    case 3: y = r + 1; break;                                // S
    case 4: x = c - 1; y = odd ? r + 1 : r; break;           // SW
    case 5: x = c - 1; y = odd ? r : r - 1; break;           // NW
    }
    if (x < 0 || x >= cols || y < 0 || y >= rows) return false;
    *nc = x; *nr = y;
    return true;
}

// ── 地形色 ─────────────────────────────────────────────────────────
QColor terrainWash(Terrain t)
{
    switch (t) {
    case Terrain::Farm:     return QColor(0xEA, 0xDB, 0xB0);
    case Terrain::Forest:   return QColor(0xC9, 0xD6, 0xA8);
    case Terrain::Hills:    return QColor(0xD6, 0xBC, 0x8A);
    case Terrain::Mountain: return QColor(0xC9, 0xA6, 0x76);
    case Terrain::Marsh:    return QColor(0xB8, 0xD0, 0xC6);
    case Terrain::Water:    return QColor(0xAF, 0xCB, 0xD4);
    case Terrain::Urban:    return QColor(0xD6, 0xC6, 0xAC);
    default:                return QColor(0xF2, 0xE8, 0xD2);
    }
}

void drawForestMark(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row)
{
    // 树冠：更深、更大、更密的针叶三角
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0x4A, 0x63, 0x3A, 205));
    const int n = 7 + int(jitter(col, row, 1) * 4);
    for (int i = 0; i < n; ++i) {
        const qreal a = jitter(col, row, 10 + i) * 6.283;
        const qreal rr = 0.68 * R * std::sqrt(jitter(col, row, 20 + i));
        const QPointF pt = c + QPointF(std::cos(a) * rr * 1.05, std::sin(a) * rr * 0.78);
        const qreal s = 0.62 + 0.30 * jitter(col, row, 30 + i);
        QPolygonF tri;
        tri << pt + QPointF(0, -s * 1.25) << pt + QPointF(s * 0.90, s * 0.72)
            << pt + QPointF(-s * 0.90, s * 0.72);
        p->drawPolygon(tri);
    }
    // 疏密不一的底层阴影点，避免“浮”在纸上
    p->setBrush(QColor(0x3F, 0x55, 0x30, 90));
    for (int i = 0; i < 6; ++i) {
        const qreal a = jitter(col, row, 200 + i) * 6.283;
        const qreal rr = 0.66 * R * std::sqrt(jitter(col, row, 210 + i));
        const QPointF pt = c + QPointF(std::cos(a) * rr, std::sin(a) * rr * 0.8);
        p->drawEllipse(QRectF(pt.x() - 0.22 * R, pt.y() - 0.13 * H, 0.44 * R, 0.26 * H));
    }
}

void drawMarshMark(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row)
{
    // 沼泽：横向水纹 + 竖立芦苇，蓝绿调更深
    p->setPen(QPen(QColor(0x3E, 0x67, 0x6E, 165), 0.18));
    const int rows = 4;
    for (int i = 0; i < rows; ++i) {
        const qreal y = c.y() - 0.42 * H + i * (0.30 * H) + (jitter(col, row, 40 + i) - 0.5) * 0.5;
        const qreal x0 = c.x() - 0.52 * R + jitter(col, row, 50 + i) * 1.2;
        const qreal x1 = x0 + 0.40 * R + jitter(col, row, 60 + i) * 0.5 * R;
        p->drawLine(QPointF(x0, y), QPointF(x1, y));
        p->drawLine(QPointF(x0 + 0.18 * R, y + 0.6), QPointF(x0 + 0.18 * R, y + 1.0));
    }
    // 几丛芦苇（短竖线簇）
    p->setPen(QPen(QColor(0x35, 0x5A, 0x52, 120), 0.14));
    for (int i = 0; i < 5; ++i) {
        const qreal a = jitter(col, row, 220 + i) * 6.283;
        const qreal rr = 0.55 * R * std::sqrt(jitter(col, row, 230 + i));
        const QPointF pt = c + QPointF(std::cos(a) * rr, std::sin(a) * rr * 0.8);
        p->drawLine(pt, pt + QPointF(0, 0.5));
    }
}

void drawHillHachure(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row,
                     bool steep)
{
    p->setPen(QPen(QColor(0x8A, 0x63, 0x3C, steep ? 150 : 110), steep ? 0.22 : 0.16,
                   Qt::SolidLine, Qt::RoundCap));
    const int n = steep ? 11 : 7;
    for (int i = 0; i < n; ++i) {
        const qreal t = qreal(i) / qreal(n - 1);
        const qreal y = c.y() - 0.40 * H + t * 0.80 * H;
        const qreal xc = c.x() + (jitter(col, row, 70) - 0.5) * 0.5 * R;
        const qreal half = (0.30 + 0.18 * std::sin(t * 3.14159)) * R;
        p->drawArc(QRectF(xc - half, y - 0.10 * H, half * 2, 0.22 * H), 20 * 16, 140 * 16);
    }
}

void drawUrbanBlocks(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row)
{
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0x6B, 0x5C, 0x4B, 120));
    for (int i = 0; i < 4; ++i) {
        const qreal x = c.x() - 0.45 * R + jitter(col, row, 80 + i) * 0.9 * R;
        const qreal y = c.y() - 0.35 * H + jitter(col, row, 90 + i) * 0.7 * H;
        const qreal w = 0.10 * R + jitter(col, row, 100 + i) * 0.10 * R;
        const qreal h = 0.06 * H + jitter(col, row, 110 + i) * 0.06 * H;
        p->drawRect(QRectF(x, y, w, h));
    }
}

// 纸张：底色 + 纤维 + 极淡污渍
void paintPaper(QPainter *p, const QSizeF &size, bool texture)
{
    QLinearGradient g(0, 0, size.width(), size.height());
    g.setColorAt(0.0, kPaper.lighter(102));
    g.setColorAt(0.55, kPaper);
    g.setColorAt(1.0, kPaperDeep);
    p->fillRect(QRectF(QPointF(0, 0), size), g);

    if (!texture) return;
    p->save();
    p->setOpacity(0.30);
    p->drawImage(QRectF(QPointF(0, 0), size), paperGrain());
    p->setOpacity(0.05);
    p->setCompositionMode(QPainter::CompositionMode_Multiply);
    p->drawImage(QRectF(QPointF(0, 0), size), paperGrain());
    p->restore();
}

// ── 城市符号 ───────────────────────────────────────────────────────
void drawCitySymbol(QPainter *p, const QPointF &c, const MapCity &city, qreal scale)
{
    const qreal s = (0.55 + 0.20 * city.size) * scale;
    p->setPen(QPen(kInk, 0.22 * scale));
    p->setBrush(QColor(0x2E, 0x25, 0x1C));

    switch (city.size) {
    case 1: p->drawEllipse(c, s * 0.55, s * 0.55); break;
    case 2:
        p->drawEllipse(c, s * 0.85, s * 0.85);
        p->drawEllipse(c, s * 0.34, s * 0.34);
        break;
    case 3:
        p->drawEllipse(c, s * 1.05, s * 1.05);
        p->drawEllipse(c, s * 0.46, s * 0.46);
        p->setBrush(QColor(0x2E, 0x25, 0x1C));
        break;
    default:
        p->drawEllipse(c, s * 1.25, s * 1.25);
        p->drawEllipse(c, s * 0.62, s * 0.62);
        break;
    }

    if (city.fortress) {
        // 要塞：外圈加四个小刺
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kInk, 0.18 * scale));
        const qreal r = s * 1.75;
        for (int i = 0; i < 4; ++i) {
            const qreal a = i * 1.5708 + 0.785;
            p->drawLine(c + QPointF(std::cos(a) * r, std::sin(a) * r),
                        c + QPointF(std::cos(a) * (r + s * 0.55), std::sin(a) * (r + s * 0.55)));
        }
    }
}

void drawCityName(QPainter *p, const QPointF &c, const MapCity &city, qreal pageRight, qreal scale)
{
    QFont f;
    f.setFamilies(serifFamilies());
    f.setWeight(city.size >= 4 ? QFont::Bold : QFont::DemiBold);
    f.setPixelSize(1);              // 实际字号下面用 mm 换算
    const qreal nameMm = (1.85 + 0.42 * city.size) * scale;
    const qreal altMm  = (1.10 + 0.12 * city.size) * scale;

    const qreal sx = (0.55 + 0.20 * city.size) * scale + 0.6;
    bool left = false;
    QFont nf = f;
    nf.setPixelSize(qRound(nameMm));     // 1pt = 0.3528mm
    nf.setLetterSpacing(QFont::PercentageSpacing, 104);
    QFontMetricsF fm(nf);
    const qreal wName = fm.horizontalAdvance(city.name);
    if (c.x() + sx + wName > pageRight - 2.0) left = true;

    const qreal tx = left ? c.x() - sx - wName : c.x() + sx;
    const qreal ty = c.y() + nameMm * 0.34;

    // 先铺一层纸色晕边，让深色地名在森林/山地格上也清晰可读
    p->setFont(nf);
    if (city.size >= 3) {
        QPainterPath halo;
        halo.addText(QPointF(tx, ty), nf, city.name);
        p->setPen(QPen(QColor(0xF2, 0xE8, 0xD2, 210), 0.7, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(halo);
    }
    p->setPen(kLabelInk);
    p->drawText(QPointF(tx, ty), city.name);

    if (!city.nameAlt.isEmpty()) {
        QFont af = nf;
        af.setPixelSize(qRound(altMm));
        af.setWeight(QFont::Normal);
        af.setItalic(true);
        af.setLetterSpacing(QFont::PercentageSpacing, 112);
        p->setFont(af);
        p->setPen(QColor(0x6A, 0x54, 0x3E, 220));
        p->drawText(QPointF(tx, ty + altMm * 1.25), city.nameAlt);
    }
}

} // namespace

// ══════════════════════════════════════════════════════════════════
qreal hexRadiusMm(const MapSpec &m) { return m.hexMm / kSqrt3; }
qreal hexHeightMm(const MapSpec &m) { return m.hexMm; }

QPointF hexCenterMm(const MapSpec &m, int col, int row)
{
    const qreal R = hexRadiusMm(m);
    return QPointF(m.marginMm + R + col * 1.5 * R,
                   m.marginMm + m.titleMm + m.hexMm * 0.5 + row * m.hexMm
                       + ((col & 1) ? m.hexMm * 0.5 : 0.0));
}

QSizeF mapSizeMm(const MapSpec &m)
{
    const qreal R = hexRadiusMm(m);
    const qreal w = 2 * m.marginMm + 2 * R + (m.cols - 1) * 1.5 * R;
    const qreal h = 2 * m.marginMm + m.titleMm + m.legendMm + m.rows * m.hexMm + m.hexMm * 0.5;
    return QSizeF(w, h);
}

bool hexAtMm(const MapSpec &m, const QPointF &ptMm, int *col, int *row)
{
    const qreal R = hexRadiusMm(m);
    const qreal H = m.hexMm;
    const qreal localX = ptMm.x() - m.marginMm - R;
    const qreal localY = ptMm.y() - m.marginMm - m.titleMm;

    const int c0 = int(std::floor(localX / (1.5 * R))) - 1;
    for (int c = c0; c <= c0 + 2; ++c) {
        if (c < 0 || c >= m.cols) continue;
        const int r0 = int(std::floor((localY - ((c & 1) ? H * 0.5 : 0.0)) / H)) - 1;
        for (int r = r0; r <= r0 + 2; ++r) {
            if (r < 0 || r >= m.rows) continue;
            const QPointF d = ptMm - hexCenterMm(m, c, r);
            // 尖顶六边形的内部判定：|x| <= R 且 |y| <= H/2 且 R*H/2 约束
            const qreal ax = std::fabs(d.x()), ay = std::fabs(d.y());
            if (ay <= H * 0.5 && (ax + ay * R / H) <= R) {
                *col = c; *row = r;
                return true;
            }
        }
    }
    return false;
}

// ══════════════════════════════════════════════════════════════════
void paintMap(QPainter *p, const MapSpec &m, const MapStyle &st)
{
    if (!m.isValid()) return;

    const QSizeF page = mapSizeMm(m);
    const qreal R = hexRadiusMm(m);
    const qreal H = m.hexMm;
    const int cols = m.cols, rows = m.rows;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    paintPaper(p, page, st.texture);

    // ── 地形底色与纹理
    if (st.shade) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const MapHex &hx = m.at(c, r);
                const QPointF ctr = hexCenterMm(m, c, r);
                const QPainterPath hp = hexPath(ctr, R, H);

                p->save();
                p->setClipPath(hp);
                const QColor wash = terrainWash(hx.terrain);
                if (hx.terrain != Terrain::Plain) {
                    p->setPen(Qt::NoPen);
                    p->setBrush(wash);
                    p->drawRect(hp.boundingRect());
                }
                switch (hx.terrain) {
                case Terrain::Forest: drawForestMark(p, ctr, R, H, c, r); break;
                case Terrain::Marsh:  drawMarshMark(p, ctr, R, H, c, r); break;
                case Terrain::Hills:  drawHillHachure(p, ctr, R, H, c, r, false); break;
                case Terrain::Mountain: drawHillHachure(p, ctr, R, H, c, r, true); break;
                case Terrain::Urban:  drawUrbanBlocks(p, ctr, R, H, c, r); break;
                case Terrain::Water: {
                    p->setPen(QPen(QColor(0x6E, 0x93, 0xA2, 90), 0.14));
                    for (int i = 0; i < 3; ++i) {
                        const qreal y = ctr.y() - H * 0.22 + i * H * 0.22;
                        p->drawLine(QPointF(ctr.x() - R * 0.6, y), QPointF(ctr.x() + R * 0.6, y));
                    }
                    break;
                }
                default: break;
                }
                p->restore();
            }
        }
    }

    // ── 海岸线：水格与非水格的公共边描蓝
    if (st.shade) {
        p->setPen(QPen(QColor(0x3C, 0x66, 0x7C, 190), 0.22, Qt::SolidLine, Qt::RoundCap));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (m.at(c, r).terrain != Terrain::Water) continue;
                const QPointF ctr = hexCenterMm(m, c, r);
                for (int e = 0; e < EdgeCount; ++e) {
                    int nc, nr;
                    if (neighborOf(cols, rows, c, r, e, &nc, &nr)
                        && m.at(nc, nr).terrain == Terrain::Water) continue;
                    QPointF a, b;
                    edgeEnds(ctr, R, H, e, &a, &b);
                    p->drawLine(a, b);
                }
            }
        }
    }

    // ── 六角格线
    if (st.grid) {
        p->setPen(QPen(QColor(kGridInk.red(), kGridInk.green(), kGridInk.blue(), 66), 0.12));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const QPointF ctr = hexCenterMm(m, c, r);
                for (int e = 0; e < EdgeCount; ++e) {
                    int nc, nr;
                    if (neighborOf(cols, rows, c, r, e, &nc, &nr)
                        && (nr * cols + nc) < (r * cols + c)) continue;
                    QPointF a, b;
                    edgeEnds(ctr, R, H, e, &a, &b);
                    p->drawLine(a, b);
                }
            }
        }
    }

    // ── 河流（边的集合；用 QPainterPath 串联相邻河段，顶点处自然咬合不豁口）
    auto drawRiverSet = [&](bool major) {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const MapHex &hx = m.at(c, r);
                const quint8 bits = major ? hx.riverMajor
                                          : quint8(hx.river & ~hx.riverMajor);
                if (!bits) continue;
                const QPointF ctr = hexCenterMm(m, c, r);
                // 只画“下半”方向的边，避免与邻居重复（0..2 与 3..5 互为对边）
                for (int e = 0; e < EdgeCount; ++e) {
                    if (!(bits & (1u << e))) continue;
                    int nc, nr;
                    if (neighborOf(cols, rows, c, r, e, &nc, &nr)
                        && (nr * cols + nc) < (r * cols + c)) continue;
                    QPointF a, b;
                    edgeEnds(ctr, R, H, e, &a, &b);
                    path.moveTo(a);
                    path.lineTo(b);
                }
            }
        }
        p->setPen(QPen(kRiver, major ? 0.74 : 0.40, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(path);
    };
    drawRiverSet(false);   // 支流
    drawRiverSet(true);    // 主干

    // ── 铁路 / 公路 / 国界（边的集合，去重后画）
    auto drawEdgeSet = [&](int which) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const MapHex &hx = m.at(c, r);
                const quint8 bits = (which == 1 ? hx.rail : (which == 2 ? hx.road : hx.border));
                for (int e = 0; e < EdgeCount; ++e) {
                    if (!(bits & (1u << e))) continue;
                    int nc, nr;
                    if (neighborOf(cols, rows, c, r, e, &nc, &nr)
                        && (nr * cols + nc) < (r * cols + c)) continue;
                    const QPointF ctr = hexCenterMm(m, c, r);
                    QPointF a, b;
                    edgeEnds(ctr, R, H, e, &a, &b);
                    const QPointF d = (b - a) / QLineF(a, b).length();
                    if (which == 1) {
                        p->setPen(QPen(kRail, 0.32, Qt::SolidLine, Qt::FlatCap));
                        p->drawLine(a, b);
                        // 枕木
                        const QPointF n(-d.y(), d.x());
                        const qreal len = QLineF(a, b).length();
                        for (qreal t = 0.6; t < len; t += 1.35) {
                            const QPointF q = a + d * t;
                            p->setPen(QPen(kRail, 0.18));
                            p->drawLine(q - n * 0.46, q + n * 0.46);
                        }
                    } else if (which == 2) {
                        p->setPen(QPen(kRoad, 0.40, Qt::SolidLine, Qt::RoundCap));
                        p->drawLine(a, b);
                    } else {
                        p->setPen(QPen(kBorder, 0.52, Qt::DashDotLine, Qt::FlatCap));
                        p->drawLine(a, b);
                    }
                }
            }
        }
    };
    drawEdgeSet(2);   // 公路
    drawEdgeSet(1);   // 铁路
    drawEdgeSet(3);   // 国界 / 战线

    // ── 格号
    if (st.hexNumbers) {
        QFont f;
        f.setFamilies(latinSerif());
        f.setPixelSize(qRound(1.05));
        p->setFont(f);
        p->setPen(QColor(kGridInk.red(), kGridInk.green(), kGridInk.blue(), 150));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const QPointF ctr = hexCenterMm(m, c, r);
                const QString code = QStringLiteral("%1%2")
                    .arg(c + 1, 2, 10, QLatin1Char('0')).arg(r + 1, 2, 10, QLatin1Char('0'));
                p->drawText(QRectF(ctr.x() - R * 0.6, ctr.y() + H * 0.16, R * 1.2, H * 0.24),
                            Qt::AlignHCenter | Qt::AlignVCenter, code);
            }
        }
    }

    // ── 城市
    for (const MapCity &city : m.cities) {
        if (!m.inside(city.col, city.row)) continue;
        const QPointF ctr = hexCenterMm(m, city.col, city.row);
        drawCitySymbol(p, ctr, city, 1.0);
        if (st.labels)
            drawCityName(p, ctr, city, page.width() - m.marginMm, 1.0);
    }

    // ── 地区名
    if (st.labels) {
        for (const MapLabel &l : m.labels) {
            QFont f;
            f.setFamilies(serifFamilies());
            f.setPixelSize(qRound(l.sizeMm));
            f.setItalic(l.italic);
            f.setWeight(QFont::DemiBold);
            f.setLetterSpacing(QFont::PercentageSpacing, l.letterSpace * 100.0);
            p->setFont(f);
            p->setPen(QColor(0x6B, 0x50, 0x39, 175));

            const QPointF ctr = hexCenterMm(m, l.col, l.row);
            p->save();
            p->translate(ctr);
            if (!qFuzzyIsNull(l.angle)) p->rotate(l.angle);
            const QString text = l.caps ? l.text.toUpper() : l.text;
            QFontMetricsF fm(f);
            const qreal w = fm.horizontalAdvance(text);
            p->drawText(QPointF(-w / 2, fm.capHeight() / 2), text);
            p->restore();
        }
    }

    // ── 图廓
    if (st.frame) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kInk, 0.9));
        p->drawRect(QRectF(m.marginMm * 0.5, m.marginMm * 0.5,
                           page.width() - m.marginMm, page.height() - m.marginMm));
        p->setPen(QPen(kInk, 0.24));
        p->drawRect(QRectF(m.marginMm * 0.5 + 1.5, m.marginMm * 0.5 + 1.5,
                           page.width() - m.marginMm - 3.0, page.height() - m.marginMm - 3.0));
    }

    // ── 标题
    {
        const qreal x = m.marginMm + 1.0;
        const qreal y = m.marginMm * 0.5 + 2.5;

        QFont tf;
        tf.setFamilies(serifFamilies());
        tf.setWeight(QFont::Bold);
        tf.setPixelSize(qRound(7.4));
        tf.setLetterSpacing(QFont::PercentageSpacing, 108);
        p->setFont(tf);
        p->setPen(kInk);
        p->drawText(QPointF(x, y + 7.2), m.title);

        QFontMetricsF tfm(tf);
        const qreal titleW = tfm.horizontalAdvance(m.title);

        if (!m.subtitle.isEmpty()) {
            QFont sf;
            sf.setFamilies(latinSerif());
            sf.setItalic(true);
            sf.setPixelSize(qRound(3.3));
            sf.setLetterSpacing(QFont::PercentageSpacing, 130);
            p->setFont(sf);
            p->setPen(QColor(0x5E, 0x46, 0x32));
            p->drawText(QPointF(x + 1.0, y + 13.0), m.subtitle.toUpper());
        }

        if (!m.date.isEmpty()) {
            QFont df;
            df.setFamilies(serifFamilies());
            df.setPixelSize(qRound(2.9));
            df.setLetterSpacing(QFont::PercentageSpacing, 108);
            p->setFont(df);
            p->setPen(QColor(0x5E, 0x46, 0x32));
            p->drawText(QPointF(x + 1.0, y + 18.0), m.date);
        }

        // 右上：比例尺文字 + 指北针
        QFont kf;
        kf.setFamilies(latinSerif());
        kf.setPixelSize(qRound(2.6));
        kf.setLetterSpacing(QFont::PercentageSpacing, 112);
        p->setFont(kf);
        p->setPen(QColor(0x5E, 0x46, 0x32));
        p->drawText(QRectF(page.width() - m.marginMm - 88, y + 1.0, 76, 5),
                    Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("%1 km / 格").arg(m.kmPerHex, 0, 'f', 0));

        const QPointF c(page.width() - m.marginMm - 18, y + 10.0);
        Q_UNUSED(titleW);
        p->setPen(QPen(kInk, 0.22));
        p->drawLine(c + QPointF(0, -5.4), c + QPointF(0, 5.4));
        p->drawLine(c + QPointF(-3.6, 0), c + QPointF(3.6, 0));
        QPolygonF north;
        north << c + QPointF(0, -6.4) << c + QPointF(1.5, -3.2) << c + QPointF(-1.5, -3.2);
        p->setBrush(kInk);
        p->setPen(Qt::NoPen);
        p->drawPolygon(north);
        p->setPen(kInk);
        QFont ff;
        ff.setFamilies(latinSerif());
        ff.setPixelSize(qRound(2.0));
        p->setFont(ff);
        p->drawText(QPointF(c.x() - 1.2, c.y() + 8.4), QStringLiteral("N"));
    }

    // ── 图例与比例尺
    if (st.legend) {
        const qreal y = page.height() - m.marginMm * 0.5 - m.legendMm + 6.0;
        const qreal x = m.marginMm + 1.0;

        // 比例尺
        const qreal mmPerKm = m.hexMm / m.kmPerHex;
        QFont lf;
        lf.setFamilies(latinSerif());
        lf.setPixelSize(qRound(2.2));
        p->setFont(lf);
        p->setPen(kInk);

        p->setPen(QPen(kInk, 0.28));
        const qreal barY = y + 9.0;
        const qreal seg = 25.0 * mmPerKm;
        for (int i = 0; i < 4; ++i) {
            p->setBrush(i % 2 ? Qt::NoBrush : QBrush(kInk));
            p->drawRect(QRectF(x + i * seg, barY, seg, 1.6));
        }
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kInk, 0.12));
        for (int i = 0; i <= 4; ++i) {
            p->drawLine(QPointF(x + i * seg, barY - 0.8), QPointF(x + i * seg, barY + 2.4));
        }
        p->setPen(kInk);
        for (int i = 0; i <= 4; i += 2) {
            p->drawText(QRectF(x + i * seg - 8, barY + 2.6, 16, 4),
                        Qt::AlignHCenter, QString::number(i * 25));
        }
        p->drawText(QPointF(x, barY - 2.0), QStringLiteral("公里 KM"));

        // 地形图例
        struct Item { Terrain t; const char *name; };
        const Item items[] = {{Terrain::Forest, "森林"}, {Terrain::Marsh, "沼泽"},
                              {Terrain::Hills, "丘陵"}, {Terrain::Mountain, "山地"},
                              {Terrain::Water, "水域"}, {Terrain::Urban, "城市"}};
        qreal lx = x + 4 * seg + 16.0;
        for (const Item &it : items) {
            const QRectF sw(lx, y + 5.2, 6.0, 4.4);
            p->setPen(QPen(QColor(0x8A, 0x6E, 0x4E, 140), 0.12));
            p->setBrush(terrainWash(it.t));
            p->drawRect(sw);
            p->setPen(kInk);
            p->setBrush(Qt::NoBrush);
            QFont f2;
            f2.setFamilies(serifFamilies());
            f2.setPixelSize(2.15);
            p->setFont(f2);
            p->drawText(QPointF(lx, y + 9.0), QString::fromUtf8(it.name));
            lx += 20.0;
        }

        if (!m.note.isEmpty()) {
            QFont nf;
            nf.setFamilies(serifFamilies());
            nf.setPixelSize(qRound(2.0));
            p->setFont(nf);
            p->setPen(QColor(0x6A, 0x54, 0x3E, 200));
            p->drawText(QRectF(lx + 6, y + 3.0, page.width() - lx - 24, 8),
                        Qt::AlignLeft | Qt::AlignVCenter, m.note);
        }
    }

    p->restore();
}

QImage mapImage(const MapSpec &m, int dpi, const MapStyle &st)
{
    const QSizeF mm = mapSizeMm(m);
    const qreal pxPerMm = dpi / 25.4;
    const int w = int(std::lround(mm.width() * pxPerMm));
    const int h = int(std::lround(mm.height() * pxPerMm));

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);

    QPainter p(&img);
    p.scale(pxPerMm, pxPerMm);      // 之后一律按毫米画
    paintMap(&p, m, st);
    p.end();
    return img;
}

} // namespace cf
