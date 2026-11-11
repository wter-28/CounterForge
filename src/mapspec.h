// mapspec.h —— 兵棋地图数据模型（六角格 + 地形 + 河流/铁路/道路 + 城市/标注）
//
// 设计原则：地图数据是纯文本 JSON，可手改、可脚本生成、可进版本库；
// 渲染层只读这份数据，不藏隐式状态，保证同一份 JSON 永远画出同一张图。
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace cf {

// ── 地形 ───────────────────────────────────────────────────────────
enum class Terrain {
    Plain = 0,   // 平原 / 荒原
    Farm,        // 耕地、草原（浅色纹理）
    Forest,      // 森林
    Hills,       // 丘陵
    Mountain,    // 山地
    Marsh,       // 沼泽
    Water,       // 海 / 湖 / 大河面
    Urban,       // 城市建成区
    Count
};

QString terrainName(Terrain t);
QChar   terrainChar(Terrain t);      // JSON 里的单字符编码
Terrain terrainFromChar(QChar c);

// ── 六角格 ─────────────────────────────────────────────────────────
// 采用 odd-q 偏移：竖列、尖顶朝上；边编号 0=N 1=NE 2=SE 3=S 4=SW 5=NW
enum HexEdge { EdgeN = 0, EdgeNE, EdgeSE, EdgeS, EdgeSW, EdgeNW, EdgeCount };

struct MapHex {
    Terrain terrain = Terrain::Plain;
    quint8  river = 0;        // 位掩码：该边有河（区分主干/支流见 riverMajor）
    quint8  riverMajor = 0;   // 位掩码：主干河流
    quint8  rail = 0;         // 位掩码：该边有铁路
    quint8  road = 0;         // 位掩码：该边有公路
    quint8  border = 0;       // 位掩码：国界 / 战线
    bool    hasRiverBit(int e) const { return river & (1u << e); }
    bool    hasRailBit(int e) const { return rail & (1u << e); }
};

// ── 城市 ───────────────────────────────────────────────────────────
struct MapCity {
    QString name;          // 主要地名（中文）
    QString nameAlt;       // 副名（原文/拉丁转写）
    int     col = 0, row = 0;
    int     size = 2;      // 1 村镇 2 小城 3 中等 4 大城市 5 特大
    bool    port = false;
    bool    fortress = false;
    QString side;          // 归属标记（可空）
};

// ── 文字标注（沼泽名、地区名、国名）────────────────────────────────
struct MapLabel {
    QString text;
    int     col = 0, row = 0;   // 以格为单位的小数坐标
    qreal   sizeMm = 5.0;
    qreal   angle = 0.0;
    bool    italic = false;
    bool    caps = true;
    qreal   letterSpace = 1.04;
};

// ── 整张图 ─────────────────────────────────────────────────────────
struct MapSpec {
    QString format = QStringLiteral("counterforge.map/1");
    QString title;              // 中文主标题
    QString subtitle;           // 外文副标题
    QString date;               // 日期标注
    QString note;               // 图例脚注

    int   cols = 34;
    int   rows = 17;
    qreal hexMm = 18.0;         // 单格对边距（毫米）
    qreal marginMm = 12.0;      // 四周留白
    qreal titleMm = 34.0;       // 顶部标题带高度
    qreal legendMm = 22.0;      // 底部图例/比例尺带高度
    qreal kmPerHex = 30.0;      // 单格代表多少公里（画比例尺用）
    QString paper = QStringLiteral("A2");  // 纸张规格标注（A2/A1/A3）

    QVector<MapHex>  hexes;     // 行优先：index = row*cols + col
    QVector<MapCity> cities;
    QVector<MapLabel> labels;

    int idx(int col, int row) const { return row * cols + col; }
    bool inside(int col, int row) const { return col >= 0 && col < cols && row >= 0 && row < rows; }
    MapHex &at(int col, int row) { return hexes[idx(col, row)]; }
    const MapHex &at(int col, int row) const { return hexes[idx(col, row)]; }

    bool isValid() const;
    QJsonObject toJson() const;
    static MapSpec fromJson(const QJsonObject &o);
};

} // namespace cf
