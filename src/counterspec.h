// counterspec.h —— 兵棋算子数据模型
#pragma once

#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace cf {

// ── 规模（北约梯队符号）────────────────────────────────────────────
enum class Echelon {
    None = 0,
    Team,        // 组/班   Ø
    Squad,       // 班      •
    Section,     // 分队    ••
    Platoon,     // 排      •••
    Company,     // 连      I
    Battalion,   // 营      II
    Regiment,    // 团      III
    Brigade,     // 旅      X
    Division,    // 师      XX
    Corps,       // 军      XXX
    Army,        // 集团军  XXXX
    ArmyGroup,   // 方面军  XXXXX
    Kampfgruppe, // 战斗群  KG
    Count
};

// ── 兵种（北约军兵种符号）──────────────────────────────────────────
enum class Branch {
    Infantry = 0,   // 步兵
    Motorized,      // 摩托化步兵
    Mechanized,     // 装甲步兵 / 机械化
    Armor,          // 装甲兵
    Cavalry,        // 骑兵
    Artillery,      // 炮兵
    Rocket,         // 火箭炮兵
    Mortar,         // 迫击炮
    AntiTank,       // 反坦克
    AntiAir,        // 防空
    Engineer,       // 工兵
    Recon,          // 侦察
    ArmoredRecon,   // 装甲侦察
    Signal,         // 通信
    HQ,             // 指挥部
    Airborne,       // 空降
    Mountain,       // 山地
    Marine,         // 海军陆战队
    Supply,         // 补给队（特殊版式）
    Transport,      // 运输
    Count
};

// ── 剪影（车辆侧影）────────────────────────────────────────────────
enum class Silhouette {
    None = 0,
    Tank,
    ArmoredCar,
    Truck,
    Motorcycle,
    Halftrack,
    SPG,
    Helicopter,
    Count
};

QString echelonName(Echelon e);        // 中文名
QString echelonGlyph(Echelon e);       // 北约符号文字（I/II/X/XX/KG…）
QString branchName(Branch b);          // 中文名
QString branchEnglish(Branch b);       // 英文名
QString silhouetteName(Silhouette s);  // 中文名

QStringList echelonNames();
QStringList branchNames();
QStringList silhouetteNames();

QString stackName(int v);              // 堆叠值的中文说明（空圈 / 实心圈）

Silhouette suggestedSilhouette(Branch b);

// ── 单枚算子 ───────────────────────────────────────────────────────
struct CounterSpec {
    QString superior     = QStringLiteral("72A.K");  // 上级（算子上部文字）
    QString designation;                              // 部队番号/代号（可选，右侧竖排）
    QString mark;                                     // 左侧小标记（可选，如 ○ / ★ / 空）
    Echelon echelon      = Echelon::Division;         // 规模
    Branch  branch       = Branch::Armor;             // 兵种
    int     combat       = 1;                         // 战斗力
    int     movement     = 8;                         // 行动力
    int     stacking     = 1;                         // 堆叠值：1 = 空圈，2 = 实心圈

    QColor  formationColor = QColor(0x6B, 0x58, 0x78); // 编制色
    bool    autoTopColor   = true;                     // 上部底色自动取编制色暗调
    QColor  topColor       = QColor(0x45, 0x45, 0x4A); // 自定义上部底色

    Silhouette silhouette = Silhouette::ArmoredCar;   // 剪影

    int  cornerPercent   = 12;    // 圆角（占边长百分比，默认 12%，0 = 直角）
    bool showEchelon     = true;  // 显示规模符号
    bool showStackMark   = true;  // 显示右上角堆叠圈
    bool showDesignation = false; // 显示番号
    bool paperBorder     = false; // 白色裁切边（印刷出血）

    QJsonObject toJson() const;
    static CounterSpec fromJson(const QJsonObject &o);

    QString title() const;  // 用于列表显示
};

} // namespace cf
