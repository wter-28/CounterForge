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
#include <algorithm>

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

// ── 平铺色块风配色（参考实体兵棋地图：冷灰米纸 + 低饱和印刷色）────
const QColor kFlatPaper    (0xDC, 0xD8, 0xCE);
const QColor kFlatPaperDeep(0xD1, 0xCD, 0xC2);
const QColor kFlatGrid     (0x77, 0x74, 0x6C);
const QColor kFlatInk      (0x2B, 0x29, 0x25);
const QColor kFlatSubInk   (0x5B, 0x56, 0x4D);
const QColor kFlatFarm     (0xD5, 0xD0, 0xC1);
const QColor kFlatHills    (0xC2, 0xBB, 0xAA);
const QColor kFlatMountain (0xAC, 0xA2, 0x91);
const QColor kFlatMarsh    (0xB3, 0xBF, 0xBB);
const QColor kFlatWater    (0x9C, 0xB0, 0xB9);
const QColor kFlatForest   (0x4C, 0x53, 0x47);
const QColor kFlatUrban    (0x84, 0x82, 0x7B);
const QColor kFlatRiverEdge(0x63, 0x82, 0x91);
const QColor kFlatRiverFill(0xB8, 0xCC, 0xD3);

QStringList sansFamilies()
{
    static const QStringList f{QStringLiteral("Microsoft YaHei"),
                               QStringLiteral("Noto Sans CJK SC"),
                               QStringLiteral("Source Han Sans SC"),
                               QStringLiteral("SimHei"),
                               QStringLiteral("Arial")};
    return f;
}

QStringList latinSans()
{
    static const QStringList f{QStringLiteral("Arial"),
                               QStringLiteral("Helvetica"),
                               QStringLiteral("Segoe UI"),
                               QStringLiteral("SimHei")};
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

// ── 值噪声（2D 整数格点上的稳定值噪声，用于高程场）───────────────
// 返回 [0,1)，同一格坐标每次结果一致；经平滑插值可得连续高程。
qreal valueNoise(int x, int y, int seed)
{
    const quint32 sx = quint32(x);
    const quint32 sy = quint32(y);
    const quint32 ss = quint32(seed);
    quint32 h = (sx * 374761393u) ^ (sy * 668265263u) ^ (ss * 2246822519u);
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return qreal(h & 0xFFFFFFu) / qreal(0x1000000u);
}

// 平滑双线性值噪声
qreal smoothNoise(qreal x, qreal y, int seed)
{
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const qreal fx = x - x0, fy = y - y0;
    const qreal sx = fx * fx * (3.0 - 2.0 * fx);   // smoothstep
    const qreal sy = fy * fy * (3.0 - 2.0 * fy);
    const qreal n00 = valueNoise(x0, y0, seed);
    const qreal n10 = valueNoise(x0 + 1, y0, seed);
    const qreal n01 = valueNoise(x0, y0 + 1, seed);
    const qreal n11 = valueNoise(x0 + 1, y0 + 1, seed);
    const qreal nx0 = n00 + (n10 - n00) * sx;
    const qreal nx1 = n01 + (n11 - n01) * sx;
    return nx0 + (nx1 - nx0) * sy;
}

// 分形布朗运动（多层噪声叠加），octaves 层、lacunarity 频率倍增
qreal fbm(qreal x, qreal y, int seed, int octaves = 5)
{
    qreal amp = 0.5, freq = 1.0, sum = 0.0, norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * smoothNoise(x * freq, y * freq, seed + i * 101);
        norm += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    return sum / norm;
}

// 每格高程（归一化 0..1），叠加地形类型修正：山地高、水域/沼泽低
qreal elevationFor(const MapHex &hx, int c, int r, int seed)
{
    qreal base = fbm(c * 0.85 + 0.3, r * 0.85 - 0.2, seed, 5);
    switch (hx.terrain) {
    case Terrain::Mountain:
        // 山地：高海拔 + 大起伏（山脊/山谷分明）
        base = 0.55 + base * 0.45;
        break;
    case Terrain::Hills:
        base = 0.42 + base * 0.42;
        break;
    case Terrain::Forest:   base = 0.32 + base * 0.42; break;
    case Terrain::Farm:     base = 0.16 + base * 0.30; break;
    case Terrain::Marsh:    base = 0.08 + base * 0.18; break;
    case Terrain::Water:    base = 0.0 + base * 0.05;  break;
    case Terrain::Urban:    base = 0.20 + base * 0.30; break;
    default:                base = 0.20 + base * 0.38; break;
    }
    return qBound(0.0, base, 1.0);
}

// 卫星风地形底色（低饱和、协调的军事地形图配色）
QColor satelliteColor(const MapHex &hx, int c, int r, int seed)
{
    const qreal e = elevationFor(hx, c, r, seed);
    const qreal n = fbm(c * 1.7 + 11.3, r * 1.7 + 7.1, seed + 500, 4); // 覆盖纹理噪声
    QColor base;
    switch (hx.terrain) {
    case Terrain::Forest:
        // 深绿（森林）
        base = QColor(int(92 + 18 * n), int(116 + 16 * n), int(72 + 12 * n));
        break;
    case Terrain::Marsh:
        // 沼泽蓝绿
        base = QColor(int(108 + 12 * n), int(126 + 12 * n), int(104 + 10 * n));
        break;
    case Terrain::Farm:
        // 耕地浅黄褐
        base = QColor(int(196 + 14 * n), int(182 + 12 * n), int(140 + 10 * n));
        break;
    case Terrain::Hills:
        // 丘陵黄褐
        base = QColor(int(176 + 16 * n), int(158 + 14 * n), int(120 + 12 * n));
        break;
    case Terrain::Mountain:
        // 山地深褐
        base = QColor(int(150 + 18 * n), int(132 + 16 * n), int(104 + 14 * n));
        break;
    case Terrain::Water:
        base = QColor(int(80 + 10 * n), int(118 + 12 * n), int(134 + 10 * n));
        break;
    case Terrain::Urban:
        base = QColor(int(168 + 12 * n), int(158 + 10 * n), int(144 + 10 * n));
        break;
    default:
        // 平原米色
        base = QColor(int(196 + 16 * n), int(188 + 12 * n), int(156 + 10 * n));
        break;
    }
    // 山体阴影因子（高程越高越暗，模拟光照）
    const qreal light = 1.0 - 0.30 * e;
    return QColor(int(base.red() * light), int(base.green() * light),
                  int(base.blue() * light));
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

// ══════════════════════════════════════════════════════════════════
//  卫星地形风（程序化高程 + 山体阴影 + 冠层 / 水域渐变）
// ══════════════════════════════════════════════════════════════════

// 格内径向渐变：模拟光照（左上受光、右下背光），叠加高程阴影
void fillSatelliteHex(QPainter *p, const QPointF &c, qreal R, qreal H,
                      const MapHex &hx, int col, int row, int seed,
                      qreal elevNw, qreal elevSe)
{
    const QColor base = satelliteColor(hx, col, row, seed);
    const qreal e = elevationFor(hx, col, row, seed);

    // 光照方向：左上 45°。亮部在上，暗部在下。
    QLinearGradient g(c.x() - R, c.y() - H * 0.5, c.x() + R, c.y() + H * 0.5);
    const qreal lift = 0.14 - 0.10 * e;   // 山地更强调明暗
    g.setColorAt(0.0, base.lighter(int(112 + 60 * e)));
    g.setColorAt(0.5, base);
    g.setColorAt(1.0, base.darker(int(112 + 70 * e)));
    p->setPen(Qt::NoPen);
    p->setBrush(g);
    p->drawPath(hexPath(c, R, H));

    // ── 山体阴影：与西北/东南邻居的高程差 → 在格边缘画阴影带 ──
    // 东南侧高（坡向东南）→ 本格西北受光更亮；西北侧高 → 东南投下阴影
    const qreal dNw = e - elevNw;   // 本格比西北邻居高多少
    const qreal dSe = e - elevSe;   // 本格比东南邻居高多少
    if (dSe > 0.03) {
        // 东南邻居更低 → 本格东南缘是陡坡，投阴影
        const int a = int(qBound(0.0, dSe, 1.0) * 90);
        QLinearGradient sh(c.x() + R * 0.3, c.y() + H * 0.2,
                           c.x() + R, c.y() + H * 0.5);
        sh.setColorAt(0.0, QColor(30, 24, 18, 0));
        sh.setColorAt(1.0, QColor(30, 24, 18, a));
        p->setBrush(sh);
        p->drawPath(hexPath(c, R, H));
    }
    if (dNw > 0.03) {
        // 西北邻居更低 → 本格西北缘受光更亮
        const int a = int(qBound(0.0, dNw, 1.0) * 60);
        QLinearGradient hl(c.x() - R, c.y() - H * 0.5,
                           c.x() - R * 0.3, c.y() - H * 0.2);
        hl.setColorAt(0.0, QColor(255, 248, 230, a));
        hl.setColorAt(1.0, QColor(255, 248, 230, 0));
        p->setBrush(hl);
        p->drawPath(hexPath(c, R, H));
    }

    // 覆盖一层低幅度噪声斑点，打破纯渐变，更接近真实地表
    const int spots = 6 + int(jitter(col, row, 300) * 6);
    p->setPen(Qt::NoPen);
    for (int i = 0; i < spots; ++i) {
        const qreal a = jitter(col, row, 310 + i) * 6.283;
        const qreal rr = 0.75 * R * std::sqrt(jitter(col, row, 320 + i));
        const QPointF pt = c + QPointF(std::cos(a) * rr, std::sin(a) * rr * 0.78);
        const qreal s = 0.35 + 0.45 * jitter(col, row, 330 + i);
        const bool dark = jitter(col, row, 340 + i) < 0.5;
        p->setBrush(dark ? QColor(0, 0, 0, 22) : QColor(255, 255, 255, 16));
        p->drawEllipse(QRectF(pt.x() - s, pt.y() - s * 0.8, s * 2, s * 1.6));
    }
}

// 森林冠层：密集的圆冠颗粒，深绿带明暗，模拟航拍树冠
void fillForestCanopy(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row)
{
    const int n = 22 + int(jitter(col, row, 400) * 14);
    for (int i = 0; i < n; ++i) {
        const qreal a = jitter(col, row, 410 + i) * 6.283;
        const qreal rr = 0.80 * R * std::sqrt(jitter(col, row, 420 + i));
        const QPointF pt = c + QPointF(std::cos(a) * rr, std::sin(a) * rr * 0.78);
        const qreal s = 0.30 + 0.34 * jitter(col, row, 430 + i);
        // 深浅两色叠加，模拟树冠受光
        const qreal shade = jitter(col, row, 440 + i);
        const int gv = int(92 + 40 * shade);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(int(52 + 24 * shade), gv, int(40 + 18 * shade), 200));
        p->drawEllipse(QRectF(pt.x() - s, pt.y() - s * 0.82, s * 2, s * 1.64));
    }
}

// 山地等高线 + 山脊阴影
void fillMountainRelief(QPainter *p, const QPointF &c, qreal R, qreal H,
                        int col, int row, int seed)
{
    // 山脊阴影：沿西北—东南方向的深色条带
    const qreal e = elevationFor(MapHex{Terrain::Mountain}, col, row, seed);
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(60, 48, 38, int(40 + 70 * e)));
    const int ridges = 3 + int(jitter(col, row, 500) * 3);
    for (int i = 0; i < ridges; ++i) {
        const qreal y = c.y() - H * 0.35 + jitter(col, row, 510 + i) * H * 0.7;
        const qreal x0 = c.x() - R * 0.75;
        const qreal w = R * (0.5 + 0.5 * jitter(col, row, 520 + i));
        p->drawEllipse(QRectF(x0, y, w, 0.35 * H));
    }
    // 细等高线
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(0x6B, 0x50, 0x3A, 90), 0.14));
    const int lines = 4 + int(jitter(col, row, 530) * 3);
    for (int i = 0; i < lines; ++i) {
        const qreal t = jitter(col, row, 540 + i);
        const qreal y = c.y() - H * 0.30 + t * H * 0.6;
        const qreal half = R * (0.22 + 0.30 * jitter(col, row, 550 + i));
        p->drawArc(QRectF(c.x() - half, y, half * 2, H * 0.30), 200 * 16, 140 * 16);
    }
}

// 水域：岸边浅、中心深的径向渐变，模拟湖泊/海深浅
// depth：到最近陆地的格数（0=岸，越大越深）
void fillWaterBody(QPainter *p, const QPointF &c, qreal R, qreal H, int col, int row, int depth)
{
    const qreal t = qBound(0.0, depth / 6.0, 1.0);   // 6 格以上就到最深
    const int r0 = int(88 - 34 * t);   // 浅岸亮蓝
    const int g0 = int(126 - 40 * t);
    const int b0 = int(142 - 34 * t);
    const int r1 = int(58 - 22 * t);   // 深水暗蓝
    const int g1 = int(94 - 28 * t);
    const int b1 = int(112 - 24 * t);
    QRadialGradient g(c, R * 1.1);
    g.setColorAt(0.0, QColor(r1, g1, b1));
    g.setColorAt(0.6, QColor((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2));
    g.setColorAt(1.0, QColor(r0, g0, b0));
    p->setPen(Qt::NoPen);
    p->setBrush(g);
    p->drawPath(hexPath(c, R, H));
    // 水面细波纹
    p->setPen(QPen(QColor(170, 200, 208, 36), 0.12));
    for (int i = 0; i < 3; ++i) {
        const qreal y = c.y() - H * 0.2 + i * H * 0.2 + (jitter(col, row, 600 + i) - 0.5) * 0.5;
        p->drawLine(QPointF(c.x() - R * 0.6, y), QPointF(c.x() + R * 0.6, y));
    }
}

// 纸张：平铺冷灰米底 + 极轻的明暗过渡，纹理压到几乎看不见，避免噪点感
void paintPaper(QPainter *p, const QSizeF &size, bool texture)
{
    QLinearGradient g(0, 0, size.width() * 0.3, size.height());
    g.setColorAt(0.0, kFlatPaper.lighter(101));
    g.setColorAt(0.55, kFlatPaper);
    g.setColorAt(1.0, kFlatPaperDeep);
    p->fillRect(QRectF(QPointF(0, 0), size), g);

    if (!texture) return;
    p->save();
    p->setOpacity(0.07);
    p->drawImage(QRectF(QPointF(0, 0), size), paperGrain());
    p->restore();
}

// ── 城市符号 ───────────────────────────────────────────────────────
void drawCitySymbol(QPainter *p, const QPointF &c, const MapCity &city, qreal scale)
{
    const qreal s = (0.55 + 0.20 * city.size) * scale;
    // 白色晕圈：让符号在深色底图上也清晰（先画一圈略大的白边）
    const qreal haloR = s * (city.size >= 4 ? 1.45 : 1.30);
    p->setPen(QPen(QColor(255, 252, 244, 220), 0.34 * scale));
    p->setBrush(QColor(255, 252, 244, 200));
    p->drawEllipse(c, haloR, haloR);

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

    // 先铺一层白色晕边，让深色地名在森林/山地/水面上也清晰可读
    p->setFont(nf);
    if (city.size >= 2) {
        QPainterPath halo;
        halo.addText(QPointF(tx, ty), nf, city.name);
        p->setPen(QPen(QColor(255, 252, 244, 230), 0.75, Qt::SolidLine, Qt::RoundCap,
                       Qt::RoundJoin));
        p->setBrush(Qt::NoBrush);
        p->drawPath(halo);
    }
    p->setPen(QColor(0x2A, 0x20, 0x18));
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

// ══════════════════════════════════════════════════════════════════
//  平铺色块风：有机平滑地形块 + 清晰格网 + 成品化地图版面
//  参考实体兵棋地图：干净的低饱和色块，不用逐格纹理与山体阴影。
// ══════════════════════════════════════════════════════════════════

QColor flatColor(Terrain t)
{
    switch (t) {
    case Terrain::Farm:     return kFlatFarm;
    case Terrain::Hills:    return kFlatHills;
    case Terrain::Mountain: return kFlatMountain;
    case Terrain::Marsh:    return kFlatMarsh;
    case Terrain::Water:    return kFlatWater;
    case Terrain::Forest:   return kFlatForest;
    case Terrain::Urban:    return kFlatUrban;
    default:                return kFlatPaper;
    }
}

// 同类地形 → 一个 QPainterPath（WindingFill 下重叠子路径自然取并集）。
// 椭圆略大于格，相邻同类格充分重叠连成片；抖动让边界呈有机圆弧，
// 全程矢量、无位图掩膜、无纹理噪点，正是实体兵棋地图的平铺色块观感。
void paintFlatTerrain(QPainter *p, const MapSpec &m, const QSizeF &page)
{
    struct Layer { Terrain t; QColor c; };
    static const Layer layers[] = {
        {Terrain::Farm,     kFlatFarm},
        {Terrain::Hills,    kFlatHills},
        {Terrain::Mountain, kFlatMountain},
        {Terrain::Marsh,    kFlatMarsh},
        {Terrain::Urban,    kFlatUrban},
        {Terrain::Water,    kFlatWater},
        {Terrain::Forest,   kFlatForest}
    };
    const qreal R = hexRadiusMm(m), Hh = m.hexMm;
    // 色块只落在格区里，不越界污染标题带 / 行列号 / 底部信息带
    const QRectF area(m.marginMm, m.marginMm + m.titleMm,
                      page.width() - 2 * m.marginMm,
                      m.rows * Hh + Hh * 0.5);
    p->save();
    p->setClipRect(area);
    for (const Layer &L : layers) {
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        const int salt = 900 + int(L.t) * 977;
        for (int r = 0; r < m.rows; ++r) {
            for (int c = 0; c < m.cols; ++c) {
                if (m.at(c, r).terrain != L.t) continue;
                const QPointF ctr = hexCenterMm(m, c, r);
                const qreal j1 = jitter(c, r, salt + 11);
                const qreal j2 = jitter(c, r, salt + 23);
                const qreal j3 = jitter(c, r, salt + 37);
                const qreal rx = R * 1.16 * (0.87 + 0.26 * j1);
                const qreal ry = Hh * 0.70 * (0.87 + 0.26 * j2);
                const QPointF off((j3 - 0.5) * R * 0.20, (j1 - 0.5) * Hh * 0.11);
                path.addEllipse(QRectF(ctr.x() + off.x() - rx, ctr.y() + off.y() - ry,
                                      2 * rx, 2 * ry));
            }
        }
        if (path.isEmpty()) continue;
        // 关掉抗锯齿：否则每个椭圆边缘的半透明像素会在重叠处叠加，
        // 深色块里出现一圈圈"葡萄串"状叠痕。
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);
        p->fillPath(path, QBrush(L.c));
        p->restore();
    }
    p->restore();
}

// 大城市建成区：几块重叠的灰斑，模拟实体地图上不规则的深灰城区
void paintUrbanBlobs(QPainter *p, const MapSpec &m)
{
    const qreal R = hexRadiusMm(m), Hh = m.hexMm;
    for (const MapCity &city : m.cities) {
        if (city.size < 4 || !m.inside(city.col, city.row)) continue;
        const QPointF ctr = hexCenterMm(m, city.col, city.row);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0x6E, 0x6D, 0x66, 165));
        for (int i = 0; i < 6; ++i) {
            const qreal a = jitter(city.col, city.row, 1500 + i * 7) * 6.283;
            const qreal d = 0.45 * R * std::sqrt(jitter(city.col, city.row, 1510 + i * 7));
            const qreal rx = R * (0.38 + 0.34 * jitter(city.col, city.row, 1520 + i * 7));
            const qreal ry = Hh * (0.26 + 0.22 * jitter(city.col, city.row, 1530 + i * 7));
            const QPointF q = ctr + QPointF(std::cos(a) * d, std::sin(a) * d * 0.8);
            p->drawEllipse(QRectF(q.x() - rx, q.y() - ry, 2 * rx, 2 * ry));
        }
    }
}

// 白描边文字：深色地形块上也能读清
void drawHaloText(QPainter *p, const QPointF &base, const QString &text,
                  const QFont &f, const QColor &ink, qreal haloMm)
{
    if (text.isEmpty()) return;
    p->setFont(f);
    QPainterPath path;
    path.addText(base, f, text);
    p->setPen(QPen(QColor(255, 253, 247, 240), haloMm, Qt::SolidLine,
                   Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);
    p->drawPath(path);
    p->setPen(ink);
    p->drawText(base, text);
}

void drawFlatCitySymbol(QPainter *p, const QPointF &c, const MapCity &city)
{
    const qreal s = 0.62 + 0.28 * city.size;
    p->setPen(QPen(QColor(255, 253, 247, 235), 0.42, Qt::SolidLine, Qt::RoundCap));
    p->setBrush(kFlatInk);
    p->drawEllipse(c, s, s);
    if (city.size >= 4) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 253, 247, 245));
        p->drawEllipse(c, s * 0.34, s * 0.34);
    }
    if (city.fortress) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kFlatInk, 0.24));
        p->drawEllipse(c, s * 1.42, s * 1.42);
    }
    if (city.port) {
        p->setPen(QPen(kFlatInk, 0.20));
        p->drawLine(c + QPointF(-s * 0.7, s * 1.15), c + QPointF(s * 0.7, s * 1.15));
    }
}

// 城市名：中英双行 + 白晕边；按城市等级降序布局，自动避开已占用文字框
void paintFlatCities(QPainter *p, const MapSpec &m, const QSizeF &page, bool labels)
{
    QVector<const MapCity *> cs;
    QVector<QRectF> used;
    for (const MapCity &c : m.cities) {
        if (!m.inside(c.col, c.row)) continue;
        cs << &c;
        const QPointF ctr = hexCenterMm(m, c.col, c.row);
        const qreal s = 0.62 + 0.28 * c.size;
        used << QRectF(ctr.x() - s - 0.7, ctr.y() - s - 0.7, 2 * (s + 0.7), 2 * (s + 0.7));
    }
    for (const MapLabel &l : m.labels) {
        QFont lf;
        lf.setFamilies(sansFamilies());
        lf.setPixelSize(qRound(l.sizeMm));
        lf.setWeight(QFont::DemiBold);
        lf.setLetterSpacing(QFont::PercentageSpacing, l.letterSpace * 100.0);
        QFontMetricsF fm(lf);
        const QString text = l.caps ? l.text.toUpper() : l.text;
        const qreal w = fm.horizontalAdvance(text);
        const QPointF ctr = hexCenterMm(m, l.col, l.row);
        used << QRectF(ctr.x() - w / 2 - 1.0, ctr.y() - l.sizeMm * 0.75,
                       w + 2.0, l.sizeMm * 1.5);
    }
    std::sort(cs.begin(), cs.end(), [](const MapCity *a, const MapCity *b) {
        return a->size > b->size;
    });

    // 符号统一先画，文字后画（都带白描边，避免被深色城区吞掉）
    for (const MapCity *cp : cs) {
        const QPointF ctr = hexCenterMm(m, cp->col, cp->row);
        drawFlatCitySymbol(p, ctr, *cp);
    }
    if (!labels) return;

    const QRectF bounds(m.marginMm + 2.0, m.marginMm + m.titleMm,
                        page.width() - 2 * m.marginMm - 4.0,
                        page.height() - 2 * m.marginMm - m.titleMm - m.legendMm - 4.0);

    for (const MapCity *cp : cs) {
        const MapCity &city = *cp;
        const QPointF ctr = hexCenterMm(m, city.col, city.row);

        const qreal nm = 3.25 + 0.83 * city.size;      // 中文字号（mm）
        const qreal am = 2.05 + 0.22 * city.size;      // 英文字号（mm）
        QFont nf;
        nf.setFamilies(sansFamilies());
        nf.setWeight(QFont::Bold);
        nf.setPixelSize(qRound(nm));
        nf.setLetterSpacing(QFont::PercentageSpacing, 104);
        QFont af;
        af.setFamilies(latinSans());
        af.setWeight(QFont::DemiBold);
        af.setPixelSize(qRound(am));
        af.setLetterSpacing(QFont::PercentageSpacing, 116);

        const QString alt = city.nameAlt.toUpper();
        QFontMetricsF fnm(nf), fam(af);
        const qreal wName = fnm.horizontalAdvance(city.name);
        const qreal wAlt = alt.isEmpty() ? 0.0 : fam.horizontalAdvance(alt);
        const qreal w = qMax(wName, wAlt);
        const qreal h = nm * 1.02 + (wAlt > 0 ? am * 1.32 : 0.0);
        const qreal gap = 1.35 + 0.22 * city.size;

        // 8 个候选方位：右 / 左 / 上 / 下 / 四个对角
        const QPointF cand[8] = {
            QPointF( gap,        -h * 0.5),
            QPointF(-gap - w,    -h * 0.5),
            QPointF(-w * 0.5,    -gap - h),
            QPointF(-w * 0.5,     gap),
            QPointF( gap,        -gap - h),
            QPointF(-gap - w,    -gap - h),
            QPointF( gap,         gap),
            QPointF(-gap - w,     gap)
        };
        QRectF best;
        qreal bestScore = 1e18;
        for (int i = 0; i < 8; ++i) {
            const QRectF box(ctr + cand[i], QSizeF(w, h));
            qreal penalty = 0.0;
            if (!bounds.contains(box)) penalty += 1000.0;
            for (const QRectF &u : used) {
                const QRectF ov = box.intersected(u);
                if (!ov.isEmpty()) penalty += ov.width() * ov.height() + 4.0;
            }
            if (penalty < bestScore) { bestScore = penalty; best = box; }
            if (penalty < 0.01) break;
        }
        used << best;

        const QPointF nameBase(best.left(), best.top() + nm * 0.92);
        drawHaloText(p, nameBase, city.name, nf, kFlatInk, nm * 0.20);
        if (wAlt > 0) {
            const QPointF altBase(best.left(), best.top() + nm * 1.02 + am * 1.02);
            drawHaloText(p, altBase, alt, af, kFlatSubInk, am * 0.30);
        }
    }
}

// 地图版面：图廓、行列号、标题带、右上图例、底部回合轨 / 效果表 / 标题牌
void paintFlatFurniture(QPainter *p, const MapSpec &m, const QSizeF &page, const MapStyle &st)
{
    const qreal W = page.width(), H = page.height();
    const qreal fx = m.marginMm * 0.5;
    const QRectF outer(fx, fx, W - 2 * fx, H - 2 * fx);
    const QRectF inner = outer.adjusted(1.4, 1.4, -1.4, -1.4);
    const qreal topLine = m.marginMm + m.titleMm - 1.0;
    const qreal gridBottom = m.marginMm + m.titleMm + m.rows * m.hexMm + m.hexMm * 0.5;

    if (st.frame) {
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kFlatInk, 0.85));
        p->drawRect(outer);
        p->setPen(QPen(kFlatInk, 0.24));
        p->drawRect(inner);
        p->setPen(QPen(kFlatInk, 0.36));
        p->drawLine(QLineF(inner.left(), topLine, inner.right(), topLine));
        p->drawLine(QLineF(inner.left(), gridBottom, inner.right(), gridBottom));

        // 左侧行号 / 顶部列号
        QFont nf;
        nf.setFamilies(latinSans());
        nf.setPixelSize(3);
        nf.setWeight(QFont::DemiBold);
        p->setFont(nf);
        p->setPen(QColor(0x4F, 0x4C, 0x45, 215));
        for (int r = 0; r < m.rows; ++r) {
            const qreal y = hexCenterMm(m, 0, r).y();
            p->drawText(QRectF(inner.left() + 0.6, y - 2.0, 13.2, 4.0),
                        Qt::AlignCenter, QStringLiteral("%1").arg(r + 1, 2, 10, QLatin1Char('0')));
        }
        for (int c = 0; c < m.cols; ++c) {
            const qreal x = hexCenterMm(m, c, 0).x();
            p->drawText(QRectF(x - 6.9, topLine - 5.2, 13.8, 4.0),
                        Qt::AlignCenter, QStringLiteral("%1").arg(c + 1, 2, 10, QLatin1Char('0')));
        }

        // 标题带
        const qreal tx = inner.left() + 4.0;
        QFont tf;
        tf.setFamilies(sansFamilies());
        tf.setWeight(QFont::Bold);
        tf.setPixelSize(15);
        tf.setLetterSpacing(QFont::PercentageSpacing, 106);
        p->setFont(tf);
        p->setPen(kFlatInk);
        p->drawText(QPointF(tx, fx + 22.0), m.title);

        QFont sf;
        sf.setFamilies(latinSans());
        sf.setPixelSize(5);
        sf.setWeight(QFont::DemiBold);
        sf.setLetterSpacing(QFont::PercentageSpacing, 118);
        p->setFont(sf);
        p->setPen(kFlatSubInk);
        p->drawText(QPointF(tx + 1.0, fx + 31.0), m.subtitle.toUpper());

        QFont df;
        df.setFamilies(sansFamilies());
        df.setPixelSize(3);
        p->setFont(df);
        p->setPen(kFlatSubInk);
        p->drawText(QPointF(tx + 1.0, fx + 36.6), m.date);
    }

    if (st.legend) {
        // 右上：地形图例框
        const qreal lw = 210.0, lh = 27.0;
        const QRectF box(inner.right() - lw - 2.0, fx + 3.0, lw, lh);
        p->setBrush(QColor(0xE8, 0xE4, 0xDA, 238));
        p->setPen(QPen(kFlatInk, 0.34));
        p->drawRect(box);
        QFont hf;
        hf.setFamilies(sansFamilies());
        hf.setPixelSize(3);
        hf.setWeight(QFont::Bold);
        p->setFont(hf);
        p->setPen(kFlatInk);
        p->drawText(QPointF(box.left() + 3.0, box.top() + 5.0),
                    QStringLiteral("地形 / TERRAIN KEY"));
        p->setPen(QPen(kFlatInk, 0.16));
        p->drawLine(QLineF(box.left() + 2.5, box.top() + 6.4,
                           box.right() - 2.5, box.top() + 6.4));

        struct Item { Terrain t; const char *name; };
        static const Item items[] = {
            {Terrain::Plain, "平原"}, {Terrain::Forest, "森林"}, {Terrain::Hills, "丘陵"},
            {Terrain::Mountain, "山地"}, {Terrain::Marsh, "沼泽"}, {Terrain::Water, "水域"}
        };
        QFont lf;
        lf.setFamilies(sansFamilies());
        lf.setPixelSize(3);
        for (int i = 0; i < 6; ++i) {
            const qreal x = box.left() + 3.0 + (i % 3) * (lw - 6.0) / 3.0;
            const qreal y = box.top() + 8.0 + (i / 3) * 9.0;
            p->setPen(QPen(QColor(0x55, 0x52, 0x4B, 130), 0.14));
            p->setBrush(flatColor(items[i].t));
            p->drawRect(QRectF(x, y, 9.0, 5.4));
            p->setPen(kFlatInk);
            p->setFont(lf);
            p->drawText(QPointF(x + 11.0, y + 4.4), QString::fromUtf8(items[i].name));
        }

        // 指北针（标题带中部）
        const QPointF nc(box.left() - 26.0, fx + 17.0);
        p->setPen(QPen(kFlatInk, 0.26));
        p->setBrush(Qt::NoBrush);
        p->drawEllipse(nc, 7.2, 7.2);
        p->setBrush(kFlatInk);
        p->setPen(Qt::NoPen);
        QPolygonF arrow;
        arrow << nc + QPointF(0, -6.6) << nc + QPointF(2.0, 1.0)
              << nc + QPointF(0, -0.6) << nc + QPointF(-2.0, 1.0);
        p->drawPolygon(arrow);
        QFont nf2;
        nf2.setFamilies(latinSans());
        nf2.setPixelSize(3);
        nf2.setWeight(QFont::Bold);
        p->setFont(nf2);
        p->setPen(kFlatInk);
        p->drawText(QRectF(nc.x() - 4, nc.y() + 1.5, 8, 4), Qt::AlignCenter,
                    QStringLiteral("N"));

        // 底部：回合记录轨
        const qreal by = gridBottom + 1.6;
        const qreal plateW = 152.0;
        const qreal tx0 = inner.left() + 2.0;
        const qreal tx1 = inner.right() - plateW - 4.0;
        const int turns = 12;
        const qreal tw = (tx1 - tx0) / turns;
        QFont turnF;
        turnF.setFamilies(latinSans());
        turnF.setPixelSize(3);
        turnF.setWeight(QFont::Bold);
        QFont smallF;
        smallF.setFamilies(sansFamilies());
        smallF.setPixelSize(2);
        for (int i = 0; i < turns; ++i) {
            const QRectF cell(tx0 + i * tw, by, tw, 10.2);
            p->setBrush(i % 2 ? QColor(0xE2, 0xDE, 0xD4) : QColor(0xCF, 0xCB, 0xC1));
            p->setPen(QPen(kFlatInk, 0.20));
            p->drawRect(cell);
            p->setPen(kFlatInk);
            p->setFont(turnF);
            p->drawText(QRectF(cell.left(), cell.top() + 1.0, cell.width(), 4.6),
                        Qt::AlignCenter, QStringLiteral("%1.TURN").arg(i + 1));
            p->setFont(smallF);
            p->setPen(kFlatSubInk);
            p->drawText(QRectF(cell.left(), cell.top() + 5.7, cell.width(), 3.6),
                        Qt::AlignCenter, QStringLiteral("第 %1 回合").arg(i + 1));
        }

        // 左下：地形效果表
        const qreal tabX = tx0, tabY = by + 12.2, rowH = 2.45, tabW = 182.0;
        static const char *rows[][4] = {
            {"地形", "移动", "防御", "补给"},
            {"平原 / 耕地", "1", "×1", "可"},
            {"森林", "2", "×2", "可"},
            {"丘陵", "2", "×2", "可"},
            {"山地", "3", "×3", "否"},
            {"沼泽", "3", "×1", "否"},
            {"水域", "—", "—", "—"}
        };
        const qreal cw[4] = {tabW * 0.42, tabW * 0.18, tabW * 0.18, tabW * 0.22};
        QFont tableF;
        tableF.setFamilies(sansFamilies());
        tableF.setPixelSize(2);
        p->setFont(tableF);
        for (int r = 0; r < 7; ++r) {
            const qreal yy = tabY + r * rowH;
            if (r == 0) {
                p->setBrush(QColor(0xC9, 0xC5, 0xBB));
                p->setPen(Qt::NoPen);
                p->drawRect(QRectF(tabX, yy, tabW, rowH));
            }
            p->setPen(QPen(QColor(0x55, 0x52, 0x4B, 120), 0.12));
            p->setBrush(Qt::NoBrush);
            p->drawRect(QRectF(tabX, yy, tabW, rowH));
            qreal cx = tabX;
            for (int k = 0; k < 4; ++k) {
                if (k) p->drawLine(QLineF(cx, yy, cx, yy + rowH));
                p->setPen(kFlatInk);
                p->drawText(QRectF(cx + 0.8, yy, cw[k] - 1.6, rowH),
                            Qt::AlignVCenter | (k ? Qt::AlignHCenter : Qt::AlignLeft),
                            QString::fromUtf8(rows[r][k]));
                cx += cw[k];
            }
        }

        // 比例尺
        const qreal sx = tabX + tabW + 12.0, sy = by + 17.0;
        const qreal seg = 25.0 * m.hexMm / m.kmPerHex;
        QFont scF;
        scF.setFamilies(latinSans());
        scF.setPixelSize(2);
        p->setFont(scF);
        p->setPen(kFlatInk);
        p->drawText(QPointF(sx, sy - 2.0), QStringLiteral("公里 KM"));
        for (int i = 0; i < 4; ++i) {
            p->setBrush(i % 2 ? Qt::NoBrush : QBrush(kFlatInk));
            p->setPen(QPen(kFlatInk, 0.18));
            p->drawRect(QRectF(sx + i * seg, sy, seg, 1.8));
            p->setBrush(Qt::NoBrush);
            p->setPen(kFlatInk);
            p->drawText(QRectF(sx + i * seg - 6, sy + 2.1, 12, 3.2),
                        Qt::AlignHCenter, QString::number(i * 25));
        }
        p->drawText(QRectF(sx + 4 * seg - 6, sy + 2.1, 12, 3.2),
                    Qt::AlignHCenter, QStringLiteral("100"));
        if (!m.note.isEmpty()) {
            QFont noteF;
            noteF.setFamilies(sansFamilies());
            noteF.setPixelSize(2);
            p->setFont(noteF);
            p->setPen(kFlatSubInk);
            p->drawText(QPointF(sx, sy + 7.0), m.note);
        }

        // 右下：深色标题牌
        const QRectF plate(tx1 + 4.0, by, plateW, outer.bottom() - by - 1.0);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(0x2B, 0x29, 0x25));
        p->drawRect(plate);
        p->setPen(QPen(QColor(0xD9, 0xD5, 0xCB, 170), 0.18));
        p->setBrush(Qt::NoBrush);
        p->drawRect(plate.adjusted(1.2, 1.2, -1.2, -1.2));

        QFont pt;
        pt.setFamilies(sansFamilies());
        pt.setPixelSize(8);
        pt.setWeight(QFont::Bold);
        pt.setLetterSpacing(QFont::PercentageSpacing, 108);
        p->setFont(pt);
        p->setPen(QColor(0xF3, 0xEF, 0xE6));
        p->drawText(QPointF(plate.left() + 5.0, plate.top() + 11.0),
                    QStringLiteral("巴巴罗萨行动"));
        QFont pe;
        pe.setFamilies(latinSans());
        pe.setPixelSize(4);
        pe.setWeight(QFont::Bold);
        pe.setLetterSpacing(QFont::PercentageSpacing, 130);
        p->setFont(pe);
        p->setPen(QColor(0xD8, 0xC9, 0xA8));
        p->drawText(QPointF(plate.left() + 5.4, plate.top() + 17.2),
                    QStringLiteral("BARBAROSSA 1941"));
        QFont ps;
        ps.setFamilies(sansFamilies());
        ps.setPixelSize(2);
        p->setFont(ps);
        p->setPen(QColor(0xC9, 0xC5, 0xBB));
        p->drawText(QPointF(plate.left() + 5.4, plate.top() + 21.4),
                    QStringLiteral("全线战场 · 波罗的海 — 黑海"));
        p->drawText(QPointF(plate.left() + 5.4, plate.bottom() - 2.2),
                    QStringLiteral("COUNTERFORGE  ·  WTER"));
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
//  卫星风连续底图：逐像素（每格约 16px）采样高程/地形/水深，
//  用连续高程场 + hillshade + 水域深浅生成接近卫星影像的地表。
// ══════════════════════════════════════════════════════════════════
static void renderSatelliteBase(QPainter *p, const MapSpec &m, const QSizeF &page)
{
    const int cols = m.cols, rows = m.rows;
    const qreal R = hexRadiusMm(m);
    const qreal H = m.hexMm;
    const int seed = 20260622;
    const int PPX = 32;   // 每格采样像素

    // 每格高程 + 地形 + 水域深度
    QVector<qreal> elev(rows * cols);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            elev[r * cols + c] = elevationFor(m.at(c, r), c, r, seed);

    QVector<int> depth(rows * cols, 0);
    {
        QVector<int> q;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                if (m.at(c, r).terrain != Terrain::Water)
                    q << (r * cols + c);
        int head = 0;
        while (head < q.size()) {
            const int idx = q[head++];
            const int cr = idx / cols, cc = idx % cols;
            for (int e = 0; e < EdgeCount; ++e) {
                int nc, nr;
                if (!neighborOf(cols, rows, cc, cr, e, &nc, &nr)) continue;
                const int ni = nr * cols + nc;
                if (depth[ni] != 0 || m.at(nc, nr).terrain != Terrain::Water) continue;
                depth[ni] = depth[idx] + 1;
                q << ni;
            }
        }
    }

    // 逐像素生成底图：连续高程场 + 地形色 + hillshade + 水域深浅
    const int W = cols * PPX, Hh = rows * PPX;
    QImage base(W, Hh, QImage::Format_RGB32);

    // 每像素对应的格坐标（含分数部分用于连续插值）
    for (int py = 0; py < Hh; ++py) {
        const qreal fr = py / qreal(PPX);   // 连续行坐标
        const int r0 = int(fr);
        for (int px = 0; px < W; ++px) {
            const qreal fc = px / qreal(PPX);   // 连续列坐标
            const int c0 = int(fc);

            const int c = qBound(0, c0, cols - 1);
            const int r = qBound(0, r0, rows - 1);
            const MapHex &hx = m.at(c, r);

            // 连续高程：双线性插值格心高程，得到平滑起伏
            const qreal e00 = elev[r * cols + c];
            qreal e = e00;

            // 地形色（基于该格类型 + 连续噪声）
            QColor col = satelliteColor(hx, c, r, seed);

            // 用连续 fbm 给每像素加地表纹理（打破纯色，模拟卫星地表）
            const qreal tex = fbm(fc * 0.9 + 0.3, fr * 0.9 - 0.2, seed + 900, 5);

            int rr2, gg, bb;
            if (hx.terrain == Terrain::Water) {
                // 水域：离岸距离决定深浅 + 连续微纹理
                const qreal t = qBound(0.0, depth[r * cols + c] / 8.0, 1.0);
                const qreal w = (tex - 0.5) * 12.0;
                // 浅岸：亮蓝绿；深水：暗蓝
                rr2 = int(150 - 96 * t + w);
                gg = int(168 - 84 * t + w);
                bb = int(176 - 66 * t + w);
            } else {
                // 陆地：地形色 + hillshade（高程梯度）+ 纹理
                const qreal n = tex;
                // hillshade：用高程梯度近似（对格内做西北光照明暗）
                const qreal gradX = (elev[r * cols + qMin(cols - 1, c + 1)]
                                     - elev[r * cols + qMax(0, c - 1)]);
                const qreal gradY = (elev[qMin(rows - 1, r + 1) * cols + c]
                                     - elev[qMax(0, r - 1) * cols + c]);
                qreal shade = 1.0 - 0.75 * (gradX + gradY);
                shade = qBound(0.58, shade, 1.28);

                // 森林：冠层绿色调 + 颗粒；山地：棕褐 + 岩质感
                rr2 = int((col.red() * (0.94 + 0.12 * n)) * shade);
                gg = int((col.green() * (0.94 + 0.12 * n)) * shade);
                bb = int((col.blue() * (0.94 + 0.12 * n)) * shade);

                // 森林冠层颗粒（细密暗点）
                if (hx.terrain == Terrain::Forest) {
                    const qreal cn = fbm(fc * 3.1, fr * 3.1, seed + 1100, 3);
                    if (cn > 0.62) {
                        const int dark = int((cn - 0.62) * 120.0);
                        rr2 = qMax(0, rr2 - dark);
                        gg = qMax(0, gg - int(dark * 0.7));
                        bb = qMax(0, bb - int(dark * 0.5));
                    }
                }
                // 山地岩质条纹
                if (hx.terrain == Terrain::Mountain || hx.terrain == Terrain::Hills) {
                    const qreal rn = fbm(fc * 2.4, fr * 0.6, seed + 1200, 3);
                    const int ridge = int((rn - 0.5) * 46);
                    rr2 += ridge; gg += ridge; bb += ridge;
                }
            }

            base.setPixel(px, py, qRgb(qBound(0, rr2, 255),
                                       qBound(0, gg, 255),
                                       qBound(0, bb, 255)));
        }
    }

    // 平滑放大到整页（每格从 16px 放大，保留足够的纹理细节）
    QImage big = base.scaled(int(page.width() * 4.0), int(page.height() * 4.0),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->drawImage(QRectF(QPointF(0, 0), page), big);

    // 森林冠层：在连续底图之上，对森林格叠加更清晰的冠层颗粒
    p->save();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (m.at(c, r).terrain != Terrain::Forest) continue;
            const QPointF ctr = hexCenterMm(m, c, r);
            p->save();
            p->setClipPath(hexPath(ctr, R, H));
            fillForestCanopy(p, ctr, R, H, c, r);
            p->restore();
        }
    }
    p->restore();
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

    // ── 卫星风：连续底图（高程 hillshade + 水域深浅 + 森林冠层），之后只叠格线
    if (st.shade && st.satellite) {
        renderSatelliteBase(p, m, page);
    }

    // ── 默认：平铺色块风（有机地形块 + 大城市建成区）────────────
    if (st.shade && !st.satellite && !st.classic) {
        paintFlatTerrain(p, m, page);
        paintUrbanBlobs(p, m);
    }

    // ── 可选：旧式逐格纹理地形（--mapstyle classic）──────────────
    if (st.shade && !st.satellite && st.classic) {
        const int seed = 20260622;
        // 预计算每格高程（山体阴影用）与水域深度
        QVector<qreal> elev(rows * cols);
        QVector<int> wdepth(rows * cols, 0);
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                elev[r * cols + c] = elevationFor(m.at(c, r), c, r, seed);
        {
            QVector<int> q;
            for (int r = 0; r < rows; ++r)
                for (int c = 0; c < cols; ++c)
                    if (m.at(c, r).terrain != Terrain::Water)
                        q << (r * cols + c);
            int head = 0;
            while (head < q.size()) {
                const int idx = q[head++];
                const int cr = idx / cols, cc = idx % cols;
                for (int e = 0; e < EdgeCount; ++e) {
                    int nc, nr;
                    if (!neighborOf(cols, rows, cc, cr, e, &nc, &nr)) continue;
                    const int ni = nr * cols + nc;
                    if (wdepth[ni] != 0 || m.at(nc, nr).terrain != Terrain::Water) continue;
                    wdepth[ni] = wdepth[idx] + 1;
                    q << ni;
                }
            }
        }
        // 六角格邻居高程（用于山体阴影方向）
        auto eNbr = [&](int c, int r, int edge) {
            int nc, nr;
            if (!neighborOf(cols, rows, c, r, edge, &nc, &nr)) return 0.0;
            return elev[nr * cols + nc];
        };

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const MapHex &hx = m.at(c, r);
                const QPointF ctr = hexCenterMm(m, c, r);
                const QPainterPath hp = hexPath(ctr, R, H);

                p->save();
                p->setClipPath(hp);

                if (hx.terrain == Terrain::Water) {
                    // 水域：离岸深浅渐变
                    const qreal t = qBound(0.0, wdepth[r * cols + c] / 8.0, 1.0);
                    QColor sh(168 - 80 * t, 186 - 76 * t, 190 - 66 * t);
                    QColor dp(96 - 40 * t, 120 - 40 * t, 138 - 34 * t);
                    QRadialGradient wg(ctr, R * 1.1);
                    wg.setColorAt(0.0, dp);
                    wg.setColorAt(1.0, sh);
                    p->setPen(Qt::NoPen);
                    p->setBrush(wg);
                    p->drawPath(hp);
                } else {
                    // 地形底色
                    const QColor wash = terrainWash(hx.terrain);
                    p->setPen(Qt::NoPen);
                    p->setBrush(wash);
                    p->drawPath(hp);

                    // 山体阴影：光照自西北，比较东南(2)与西北(5)邻居的高程差
                    const qreal e = elev[r * cols + c];
                    const qreal dSe = e - eNbr(c, r, EdgeSE);   // 本格高于东南邻居→东南缘背光
                    const qreal dNw = e - eNbr(c, r, EdgeNW);   // 本格高于西北邻居→西北缘受光
                    if (dSe > 0.03) {
                        const int a = int(qBound(0.0, dSe, 1.0) * 130);
                        QLinearGradient shg(ctr.x() + R * 0.2, ctr.y() + H * 0.1,
                                            ctr.x() + R, ctr.y() + H * 0.5);
                        shg.setColorAt(0.0, QColor(38, 30, 22, 0));
                        shg.setColorAt(1.0, QColor(38, 30, 22, a));
                        p->setBrush(shg);
                        p->drawPath(hp);
                    }
                    if (dNw > 0.03) {
                        const int a = int(qBound(0.0, dNw, 1.0) * 80);
                        QLinearGradient hlg(ctr.x() - R, ctr.y() - H * 0.5,
                                            ctr.x() - R * 0.2, ctr.y() - H * 0.1);
                        hlg.setColorAt(0.0, QColor(255, 248, 230, a));
                        hlg.setColorAt(1.0, QColor(255, 248, 230, 0));
                        p->setBrush(hlg);
                        p->drawPath(hp);
                    }

                    // 地形纹理符号
                    switch (hx.terrain) {
                    case Terrain::Forest: drawForestMark(p, ctr, R, H, c, r); break;
                    case Terrain::Marsh:  drawMarshMark(p, ctr, R, H, c, r); break;
                    case Terrain::Hills:  drawHillHachure(p, ctr, R, H, c, r, false); break;
                    case Terrain::Mountain: drawHillHachure(p, ctr, R, H, c, r, true); break;
                    case Terrain::Urban:  drawUrbanBlocks(p, ctr, R, H, c, r); break;
                    default: break;
                    }
                }
                p->restore();
            }
        }
    }

    // ── 海岸线：旧风格按格边描蓝；平铺色块风的边界自带圆滑轮廓
    if (st.shade && (st.classic || st.satellite)) {
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

    // ── 六角格线：深色主线 + 细亮高光，浅底与深色森林块上都看得见
    if (st.grid) {
        auto strokeGrid = [&](const QColor &ink, qreal width, const QPointF &shift) {
            p->setPen(QPen(ink, width));
            for (int r = 0; r < rows; ++r) {
                for (int c = 0; c < cols; ++c) {
                    const QPointF ctr = hexCenterMm(m, c, r);
                    for (int e = 0; e < EdgeCount; ++e) {
                        int nc, nr;
                        if (neighborOf(cols, rows, c, r, e, &nc, &nr)
                            && (nr * cols + nc) < (r * cols + c)) continue;
                        QPointF a, b;
                        edgeEnds(ctr, R, H, e, &a, &b);
                        p->drawLine(a + shift, b + shift);
                    }
                }
            }
        };
        strokeGrid(QColor(kFlatGrid.red(), kFlatGrid.green(), kFlatGrid.blue(), 185),
                   0.16, QPointF());
        strokeGrid(QColor(0xE8, 0xE4, 0xDA, 105), 0.075, QPointF(0.10, 0.10));
    }

    // ── 河流：外层深蓝灰 + 内层浅蓝，干净的带状水系（QPainterPath 串联避免断点）
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
        const qreal outer = major ? 1.05 : 0.58;
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(kFlatRiverEdge, outer, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPath(path);
        p->setPen(QPen(kFlatRiverFill, outer * 0.62, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
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
                        p->setPen(QPen(kFlatInk, 0.26, Qt::SolidLine, Qt::RoundCap));
                        p->drawLine(a, b);
                        // 稀疏枕木：只在长边上点几根，避免密纹造成噪点
                        const QPointF n(-d.y(), d.x());
                        const qreal len = QLineF(a, b).length();
                        for (qreal t = 1.2; t < len - 0.4; t += 2.6) {
                            const QPointF q = a + d * t;
                            p->setPen(QPen(kFlatInk, 0.13));
                            p->drawLine(q - n * 0.40, q + n * 0.40);
                        }
                    } else if (which == 2) {
                        p->setPen(QPen(kFlatInk, 0.29, Qt::SolidLine, Qt::RoundCap));
                        p->drawLine(a, b);
                    } else {
                        p->setPen(QPen(kBorder, 0.44, Qt::DashDotLine, Qt::FlatCap));
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
        f.setFamilies(latinSans());
        f.setPixelSize(2);
        f.setWeight(QFont::DemiBold);
        p->setFont(f);
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                const QPointF ctr = hexCenterMm(m, c, r);
                const QString code = QStringLiteral("%1%2")
                    .arg(c + 1, 2, 10, QLatin1Char('0')).arg(r + 1, 2, 10, QLatin1Char('0'));
                const QRectF rc(ctr.x() - R * 0.6, ctr.y() + H * 0.16, R * 1.2, H * 0.24);
                p->setPen(QColor(0xE9, 0xE5, 0xDB, 195));
                p->drawText(rc.translated(0.12, 0.12), Qt::AlignHCenter | Qt::AlignVCenter, code);
                p->setPen(QColor(0x4B, 0x48, 0x41, 195));
                p->drawText(rc, Qt::AlignHCenter | Qt::AlignVCenter, code);
            }
        }
    }

    // ── 城市符号 + 中英双行标注（自动避让，保证每座城市都读得清）
    paintFlatCities(p, m, page, st.labels);

    // ── 地区名 / 水域名：白晕边保证在深色森林与水面色块上可读
    if (st.labels) {
        for (const MapLabel &l : m.labels) {
            QFont f;
            f.setFamilies(sansFamilies());
            f.setPixelSize(qRound(l.sizeMm));
            f.setWeight(QFont::DemiBold);
            f.setLetterSpacing(QFont::PercentageSpacing, l.letterSpace * 100.0);
            const QString text = l.caps ? l.text.toUpper() : l.text;
            QFontMetricsF fm(f);
            const qreal w = fm.horizontalAdvance(text);
            const QPointF ctr = hexCenterMm(m, l.col, l.row);
            p->save();
            p->translate(ctr);
            if (!qFuzzyIsNull(l.angle)) p->rotate(l.angle);
            drawHaloText(p, QPointF(-w / 2, fm.capHeight() / 2), text, f,
                         QColor(0x45, 0x42, 0x3C, 225), l.sizeMm * 0.13);
            p->restore();
        }
    }

    // ── 图廓、行列号、标题带、地形图例、回合轨、效果表与标题牌
    if (st.frame || st.legend)
        paintFlatFurniture(p, m, page, st);

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
