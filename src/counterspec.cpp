#include "counterspec.h"

namespace cf {

static const char *kEchelonNames[] = {
    "无", "组/班", "班", "分队", "排", "连", "营", "团",
    "旅", "师", "军", "集团军", "方面军/集团军群", "战斗群"
};

static const char *kEchelonGlyphs[] = {
    "", "Ø", "•", "••", "•••", "I", "II", "III",
    "X", "XX", "XXX", "XXXX", "XXXXX", "KG"
};

static const char *kBranchNames[] = {
    "步兵", "摩托化步兵", "装甲步兵", "装甲兵", "骑兵", "炮兵",
    "火箭炮", "迫击炮", "反坦克", "防空", "工兵", "侦察",
    "装甲侦察", "通信", "指挥部", "空降", "山地", "海军陆战队",
    "补给队", "运输"
};

static const char *kBranchEnglish[] = {
    "INFANTRY", "MOTORIZED", "MECHANIZED", "ARMOR", "CAVALRY", "ARTILLERY",
    "ROCKET", "MORTAR", "ANTI-TANK", "ANTI-AIR", "ENGINEER", "RECON",
    "ARM. RECON", "SIGNAL", "HEADQUARTERS", "AIRBORNE", "MOUNTAIN", "MARINE",
    "SUPPLY", "TRANSPORT"
};

static const char *kSilhouetteNames[] = {
    "无", "坦克", "装甲车", "卡车", "摩托车", "半履带车", "自行火炮", "直升机"
};

QString echelonName(Echelon e)
{
    const int i = int(e);
    if (i < 0 || i >= int(Echelon::Count)) return QString();
    return QString::fromUtf8(kEchelonNames[i]);
}

QString echelonGlyph(Echelon e)
{
    const int i = int(e);
    if (i < 0 || i >= int(Echelon::Count)) return QString();
    return QString::fromUtf8(kEchelonGlyphs[i]);
}

QString stackName(int v)
{
    return v >= 2 ? QStringLiteral("实心圈") : QStringLiteral("空圈");
}

QString branchName(Branch b)
{
    const int i = int(b);
    if (i < 0 || i >= int(Branch::Count)) return QString();
    return QString::fromUtf8(kBranchNames[i]);
}

QString branchEnglish(Branch b)
{
    const int i = int(b);
    if (i < 0 || i >= int(Branch::Count)) return QString();
    return QString::fromLatin1(kBranchEnglish[i]);
}

QString silhouetteName(Silhouette s)
{
    const int i = int(s);
    if (i < 0 || i >= int(Silhouette::Count)) return QString();
    return QString::fromUtf8(kSilhouetteNames[i]);
}

QStringList echelonNames()
{
    QStringList l;
    for (int i = 0; i < int(Echelon::Count); ++i) l << echelonName(Echelon(i));
    return l;
}

QStringList branchNames()
{
    QStringList l;
    for (int i = 0; i < int(Branch::Count); ++i) l << branchName(Branch(i));
    return l;
}

QStringList silhouetteNames()
{
    QStringList l;
    for (int i = 0; i < int(Silhouette::Count); ++i) l << silhouetteName(Silhouette(i));
    return l;
}

Silhouette suggestedSilhouette(Branch b)
{
    switch (b) {
    case Branch::Armor:
    case Branch::ArmoredRecon:  return Silhouette::Tank;
    case Branch::Mechanized:    return Silhouette::Halftrack;
    case Branch::Motorized:
    case Branch::Transport:     return Silhouette::Truck;
    case Branch::Recon:
    case Branch::HQ:            return Silhouette::ArmoredCar;
    case Branch::Cavalry:       return Silhouette::Motorcycle;
    case Branch::Artillery:
    case Branch::Rocket:
    case Branch::Mortar:
    case Branch::AntiTank:
    case Branch::AntiAir:       return Silhouette::SPG;
    case Branch::Airborne:      return Silhouette::Helicopter;
    case Branch::Supply:        return Silhouette::Truck;
    default:                    return Silhouette::None;
    }
}

QJsonObject CounterSpec::toJson() const
{
    QJsonObject o;
    o["superior"]         = superior;
    o["designation"]      = designation;
    o["mark"]             = mark;
    o["echelon"]          = int(echelon);
    o["branch"]           = int(branch);
    o["combat"]           = combat;
    o["movement"]         = movement;
    o["stacking"]         = qBound(1, stacking, 2);
    o["formationColor"]   = formationColor.name(QColor::HexArgb);
    o["autoTopColor"]     = autoTopColor;
    o["topColor"]         = topColor.name(QColor::HexArgb);
    o["silhouette"]       = int(silhouette);
    o["cornerPercent"]    = cornerPercent;
    o["showEchelon"]      = showEchelon;
    o["showStackMark"]    = showStackMark;
    o["showDesignation"]  = showDesignation;
    o["paperBorder"]      = paperBorder;
    return o;
}

CounterSpec CounterSpec::fromJson(const QJsonObject &o)
{
    CounterSpec s;
    s.superior        = o.value("superior").toString(s.superior);
    s.designation     = o.value("designation").toString();
    s.mark            = o.value("mark").toString();
    s.echelon         = Echelon(qBound(0, o.value("echelon").toInt(int(s.echelon)),
                                       int(Echelon::Count) - 1));
    s.stacking        = qBound(1, o.value("stacking").toInt(s.stacking), 2);
    s.branch          = Branch(qBound(0, o.value("branch").toInt(int(s.branch)), int(Branch::Count) - 1));
    s.combat          = qBound(0, o.value("combat").toInt(s.combat), 99);
    s.movement        = qBound(0, o.value("movement").toInt(s.movement), 99);
    s.formationColor  = QColor(o.value("formationColor").toString(s.formationColor.name(QColor::HexArgb)));
    s.autoTopColor    = o.value("autoTopColor").toBool(s.autoTopColor);
    s.topColor        = QColor(o.value("topColor").toString(s.topColor.name(QColor::HexArgb)));
    s.silhouette      = Silhouette(qBound(0, o.value("silhouette").toInt(int(s.silhouette)), int(Silhouette::Count) - 1));
    s.cornerPercent   = qBound(0, o.value("cornerPercent").toInt(s.cornerPercent), 40);
    s.showEchelon     = o.value("showEchelon").toBool(s.showEchelon);
    s.showStackMark   = o.value("showStackMark").toBool(
                            o.value("showStackBadge").toBool(s.showStackMark));
    s.showDesignation = o.value("showDesignation").toBool(s.showDesignation);
    s.paperBorder     = o.value("paperBorder").toBool(s.paperBorder);
    return s;
}

QString CounterSpec::title() const
{
    QString t = superior.trimmed();
    if (t.isEmpty()) t = designation.trimmed();
    if (t.isEmpty()) t = branchName(branch);
    return QString("%1  %2·%3").arg(t, echelonName(echelon), branchName(branch));
}

} // namespace cf
