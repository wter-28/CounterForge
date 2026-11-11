// mapspec.cpp
#include "mapspec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

namespace cf {
namespace {

const char *kTerrainNames[] = {
    "平原", "耕地", "森林", "丘陵", "山地", "沼泽", "水域", "城区"
};

// 单字符编码：手改 JSON 时最省事
QChar charOf(Terrain t)
{
    switch (t) {
    case Terrain::Plain:    return QLatin1Char('.');
    case Terrain::Farm:     return QLatin1Char(',');
    case Terrain::Forest:   return QLatin1Char('f');
    case Terrain::Hills:    return QLatin1Char('h');
    case Terrain::Mountain: return QLatin1Char('^');
    case Terrain::Marsh:    return QLatin1Char('m');
    case Terrain::Water:    return QLatin1Char('~');
    case Terrain::Urban:    return QLatin1Char('#');
    default:                return QLatin1Char('.');
    }
}

// 边列表：支持 "c,r,e; c,r,e" 字符串，也支持 [[c,r,e], ...] 数组
QVector<int> parseEdgeTriplets(const QJsonValue &v)
{
    QVector<int> out;
    auto pushTriplet = [&out](int c, int r, int e) {
        if (e < 0 || e >= EdgeCount) return;
        out << c << r << e;
    };

    if (v.isString()) {
        const QString s = v.toString();
        for (const QString &part : s.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QStringList n = part.trimmed().split(QLatin1Char(','));
            if (n.size() >= 3) pushTriplet(n[0].toInt(), n[1].toInt(), n[2].toInt());
        }
    } else if (v.isArray()) {
        for (const QJsonValue &e : v.toArray()) {
            const QJsonArray a = e.toArray();
            if (a.size() >= 3) pushTriplet(a[0].toInt(), a[1].toInt(), a[2].toInt());
        }
    }
    return out;
}

QJsonArray tripletsToJson(const QVector<int> &t)
{
    QJsonArray arr;
    for (int i = 0; i + 2 < t.size(); i += 3) {
        arr.append(QJsonArray{t.at(i), t.at(i + 1), t.at(i + 2)});
    }
    return arr;
}

} // namespace

QString terrainName(Terrain t)
{
    const int i = int(t);
    if (i < 0 || i >= int(Terrain::Count)) return QString();
    return QString::fromUtf8(kTerrainNames[i]);
}

QChar terrainChar(Terrain t) { return charOf(t); }

Terrain terrainFromChar(QChar c)
{
    switch (c.unicode()) {
    case '.': return Terrain::Plain;
    case ',': return Terrain::Farm;
    case 'f': case 'F': return Terrain::Forest;
    case 'h': case 'H': return Terrain::Hills;
    case '^': return Terrain::Mountain;
    case 'm': case 'M': return Terrain::Marsh;
    case '~': case '=': return Terrain::Water;
    case '#': return Terrain::Urban;
    default:  return Terrain::Plain;
    }
}

bool MapSpec::isValid() const
{
    return cols > 0 && rows > 0 && hexes.size() == cols * rows && hexMm > 1.0;
}

QJsonObject MapSpec::toJson() const
{
    QJsonObject o;
    o["format"]   = format;
    o["title"]    = title;
    o["subtitle"] = subtitle;
    o["date"]     = date;
    o["note"]     = note;
    o["cols"]     = cols;
    o["rows"]     = rows;
    o["hexMm"]    = hexMm;
    o["marginMm"] = marginMm;
    o["titleMm"]  = titleMm;
    o["legendMm"] = legendMm;
    o["kmPerHex"] = kmPerHex;
    o["paper"]    = paper;

    QJsonArray terr;
    for (int r = 0; r < rows; ++r) {
        QString line;
        for (int c = 0; c < cols; ++c) line += terrainChar(at(c, r).terrain);
        terr.append(line);
    }
    o["terrain"] = terr;

    QVector<int> rivers, rails, roads, borders;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const MapHex &h = at(c, r);
            for (int e = 0; e < EdgeCount; ++e) {
                if (h.river & (1u << e))  rivers  << c << r << e;
                if (h.rail & (1u << e))   rails   << c << r << e;
                if (h.road & (1u << e))   roads   << c << r << e;
                if (h.border & (1u << e)) borders << c << r << e;
            }
        }
    }
    o["rivers"]  = tripletsToJson(rivers);
    o["rail"]    = tripletsToJson(rails);
    o["roads"]   = tripletsToJson(roads);
    o["borders"] = tripletsToJson(borders);

    // 主干河流单独存一份，渲染时线更粗
    QVector<int> major;
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            for (int e = 0; e < EdgeCount; ++e)
                if (at(c, r).riverMajor & (1u << e)) major << c << r << e;
    o["riverMajor"] = tripletsToJson(major);

    QJsonArray cArr;
    for (const MapCity &city : cities) {
        QJsonObject c{{"name", city.name}, {"col", city.col}, {"row", city.row},
                      {"size", city.size}};
        if (!city.nameAlt.isEmpty()) c["alt"] = city.nameAlt;
        if (city.port)     c["port"] = true;
        if (city.fortress) c["fortress"] = true;
        if (!city.side.isEmpty()) c["side"] = city.side;
        cArr.append(c);
    }
    o["cities"] = cArr;

    QJsonArray lArr;
    for (const MapLabel &l : labels) {
        QJsonObject j{{"text", l.text}, {"col", l.col}, {"row", l.row}, {"size", l.sizeMm}};
        if (!qFuzzyIsNull(l.angle)) j["angle"] = l.angle;
        if (l.italic)      j["italic"] = true;
        if (!l.caps)       j["caps"] = false;
        lArr.append(j);
    }
    o["labels"] = lArr;
    return o;
}

MapSpec MapSpec::fromJson(const QJsonObject &o)
{
    MapSpec m;
    m.format   = o.value("format").toString(m.format);
    m.title    = o.value("title").toString();
    m.subtitle = o.value("subtitle").toString();
    m.date     = o.value("date").toString();
    m.note     = o.value("note").toString();
    m.cols     = qMax(1, o.value("cols").toInt(m.cols));
    m.rows     = qMax(1, o.value("rows").toInt(m.rows));
    m.hexMm    = qBound(6.0, o.value("hexMm").toDouble(m.hexMm), 60.0);
    m.marginMm = qBound(0.0, o.value("marginMm").toDouble(m.marginMm), 60.0);
    m.titleMm  = qBound(0.0, o.value("titleMm").toDouble(m.titleMm), 120.0);
    m.legendMm = qBound(0.0, o.value("legendMm").toDouble(m.legendMm), 80.0);
    m.kmPerHex = qBound(1.0, o.value("kmPerHex").toDouble(m.kmPerHex), 500.0);
    m.paper    = o.value("paper").toString(m.paper);

    m.hexes.resize(m.cols * m.rows);

    const QJsonArray terr = o.value("terrain").toArray();
    for (int r = 0; r < m.rows && r < terr.size(); ++r) {
        const QString line = terr.at(r).toString();
        for (int c = 0; c < m.cols && c < line.size(); ++c)
            m.hexes[r * m.cols + c].terrain = terrainFromChar(line.at(c));
    }

    auto applyEdges = [&m](const QJsonValue &v, int which) {
        const QVector<int> t = parseEdgeTriplets(v);
        for (int i = 0; i + 2 < t.size(); i += 3) {
            const int c = t.at(i), r = t.at(i + 1), e = t.at(i + 2);
            if (!m.inside(c, r)) continue;
            MapHex &h = m.at(c, r);
            const quint8 bit = quint8(1u << e);
            switch (which) {
            case 0: h.river |= bit; break;
            case 1: h.rail |= bit; break;
            case 2: h.road |= bit; break;
            case 3: h.border |= bit; break;
            case 4: h.river |= bit; h.riverMajor |= bit; break;
            }
        }
    };
    applyEdges(o.value("rivers"), 0);
    applyEdges(o.value("rail"), 1);
    applyEdges(o.value("roads"), 2);
    applyEdges(o.value("borders"), 3);
    applyEdges(o.value("riverMajor"), 4);

    for (const QJsonValue &cv : o.value("cities").toArray()) {
        const QJsonObject c = cv.toObject();
        MapCity city;
        city.name     = c.value("name").toString();
        city.nameAlt  = c.value("alt").toString();
        city.col      = c.value("col").toInt();
        city.row      = c.value("row").toInt();
        city.size     = qBound(1, c.value("size").toInt(2), 5);
        city.port     = c.value("port").toBool(false);
        city.fortress = c.value("fortress").toBool(false);
        city.side     = c.value("side").toString();
        if (m.inside(city.col, city.row)) m.cities.append(city);
    }

    for (const QJsonValue &lv : o.value("labels").toArray()) {
        const QJsonObject l = lv.toObject();
        MapLabel label;
        label.text    = l.value("text").toString();
        label.col     = l.value("col").toInt();
        label.row     = l.value("row").toInt();
        label.sizeMm  = qBound(2.0, l.value("size").toDouble(5.0), 24.0);
        label.angle   = l.value("angle").toDouble(0.0);
        label.italic  = l.value("italic").toBool(false);
        label.caps    = l.value("caps").toBool(true);
        label.letterSpace = l.value("space").toDouble(1.04);
        if (!label.text.isEmpty()) m.labels.append(label);
    }

    return m;
}

} // namespace cf
