// jsonapi.cpp —— 命令行 JSON 接口实现
#include "jsonapi.h"

#include "counterrenderer.h"
#include "counterspec.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPainter>
#include <QRegularExpression>
#include <QStringList>
#include <functional>
#include <cstdlib>

#ifdef Q_OS_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <cstdio>
#endif

namespace cf {
namespace {

// ══════════════════════════════════════════════════════════════════
//  标准输出：GUI 子系统程序默认没有控制台，从管道/终端调用时要把 stdio 接上
// ══════════════════════════════════════════════════════════════════
void ensureStdio()
{
#ifdef Q_OS_WIN
    const HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) {
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE *f = nullptr;
            freopen_s(&f, "CONOUT$", "w", stdout);
            freopen_s(&f, "CONOUT$", "w", stderr);
        }
    }
#endif
}

void printJson(const QJsonObject &o)
{
    QByteArray s = QJsonDocument(o).toJson(QJsonDocument::Compact);
    s.append('\n');

#ifdef Q_OS_WIN
    // 直接写句柄：GUI 子系统下 CRT 的 stdout 未必挂在管道上，WriteFile 才稳
    ensureStdio();
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, s.constData(), DWORD(s.size()), &written, nullptr);
    FlushFileBuffers(h);
#else
    fwrite(s.constData(), 1, size_t(s.size()), stdout);
    fflush(stdout);
#endif
}

[[noreturn]] void fail(const QString &message, const QString &path = QString())
{
    QJsonObject err{{QStringLiteral("message"), message}};
    if (!path.isEmpty()) err[QStringLiteral("path")] = path;
    printJson(QJsonObject{{QStringLiteral("ok"), false},
                          {QStringLiteral("api"), QStringLiteral("counterforge.json/1")},
                          {QStringLiteral("error"), err}});
    ::exit(2);
}

QString argValue(const QStringList &args, const QString &key, const QString &fallback = QString())
{
    const int i = args.indexOf(key);
    return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : fallback;
}

// ══════════════════════════════════════════════════════════════════
//  取值解析：枚举既可写索引，也可写中文名 / 英文名 / 北约缩写
// ══════════════════════════════════════════════════════════════════
int resolveEnum(const QJsonValue &v, int count,
                const std::function<QString(int)> &name,
                const std::function<QString(int)> &alt,
                int def, const QString &path)
{
    if (v.isUndefined() || v.isNull()) return def;

    if (v.isDouble()) {
        const int i = v.toInt();
        if (i >= 0 && i < count) return i;
        fail(QStringLiteral("索引超出范围 0..%1").arg(count - 1), path);
    }

    const QString s = v.toString().trimmed();
    for (int i = 0; i < count; ++i) {
        if (s.compare(name(i), Qt::CaseInsensitive) == 0) return i;
        if (alt && s.compare(alt(i), Qt::CaseInsensitive) == 0) return i;
    }

    QStringList known;
    for (int i = 0; i < count; ++i) {
        const QString n = name(i);
        if (!n.isEmpty()) known << n;
    }
    fail(QStringLiteral("未知取值 \"%1\"，可用：%2").arg(s, known.join(QStringLiteral(" / "))), path);
    return def;
}

bool resolveColor(const QJsonValue &v, QColor *out)
{
    if (v.isUndefined() || v.isNull()) return true;

    const QStringList names = formationPresetNames();

    if (v.isDouble()) {
        const int i = v.toInt();
        if (i >= 0 && i < names.size()) { *out = formationPreset(i); return true; }
        return false;
    }

    const QString s = v.toString().trimmed();
    if (s.isEmpty()) return true;

    for (int i = 0; i < names.size(); ++i)
        if (names.at(i) == s) { *out = formationPreset(i); return true; }

    const QColor c(s);
    if (c.isValid()) { *out = c; return true; }
    return false;
}

QString slugOf(const CounterSpec &s, int index)
{
    QString base = s.superior.trimmed();
    if (base.isEmpty()) base = s.designation.trimmed();
    if (base.isEmpty()) base = branchName(s.branch);
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\s]+")),
                 QStringLiteral("_"));
    if (base.isEmpty()) base = QStringLiteral("counter");
    return QStringLiteral("%1_%2").arg(base).arg(index + 1, 2, 10, QLatin1Char('0'));
}

// ══════════════════════════════════════════════════════════════════
//  从 JSON 造一枚算子
// ══════════════════════════════════════════════════════════════════
CounterSpec specFromJson(const QJsonObject &o, const QString &path)
{
    CounterSpec s;

    auto str = [&](const char *key) {
        const QJsonValue v = o.value(QLatin1String(key));
        return v.isUndefined() || v.isNull() ? QString() : v.toString();
    };
    auto num = [&](const char *key, int def, int lo, int hi) {
        const QJsonValue v = o.value(QLatin1String(key));
        if (v.isUndefined() || v.isNull()) return def;
        const int n = v.toInt(def);
        if (n < lo || n > hi)
            fail(QStringLiteral("%1 应在 %2..%3 之间，收到 %4")
                     .arg(QString::fromLatin1(key)).arg(lo).arg(hi).arg(n),
                 path + QLatin1Char('.') + QString::fromLatin1(key));
        return n;
    };
    auto flag = [&](const char *key, bool def) {
        const QJsonValue v = o.value(QLatin1String(key));
        return v.isUndefined() || v.isNull() ? def : v.toBool(def);
    };

    if (o.contains(QStringLiteral("superior")))     s.superior = str("superior");
    if (o.contains(QStringLiteral("designation")))  s.designation = str("designation");
    if (o.contains(QStringLiteral("mark")))         s.mark = str("mark");

    s.echelon = Echelon(resolveEnum(o.value(QStringLiteral("echelon")), int(Echelon::Count),
                                    [](int i) { return echelonName(Echelon(i)); },
                                    [](int i) { return echelonGlyph(Echelon(i)); },
                                    int(s.echelon), path + QStringLiteral(".echelon")));

    s.branch = Branch(resolveEnum(o.value(QStringLiteral("branch")), int(Branch::Count),
                                  [](int i) { return branchName(Branch(i)); },
                                  [](int i) { return branchEnglish(Branch(i)); },
                                  int(s.branch), path + QStringLiteral(".branch")));

    s.combat   = num("combat", s.combat, 0, 99);
    s.movement = num("movement", s.movement, 0, 99);
    s.stacking = num("stacking", s.stacking, 1, 2);

    QColor c;
    if (!resolveColor(o.value(QStringLiteral("formationColor")), &c))
        fail(QStringLiteral("编制色无法识别，可写 #RRGGBB 或 16 色预设名"),
             path + QStringLiteral(".formationColor"));
    if (c.isValid()) s.formationColor = c;

    s.autoTopColor = flag("autoTopColor", s.autoTopColor);
    if (!resolveColor(o.value(QStringLiteral("topColor")), &c))
        fail(QStringLiteral("上部底色无法识别"), path + QStringLiteral(".topColor"));
    if (o.contains(QStringLiteral("topColor")) && c.isValid()) s.topColor = c;

    s.cornerPercent   = num("cornerPercent", s.cornerPercent, 0, 30);
    s.showEchelon     = flag("showEchelon", s.showEchelon);
    s.showStackMark   = flag("showStackMark", s.showStackMark);
    s.showDesignation = flag("showDesignation", s.showDesignation);
    s.paperBorder     = flag("paperBorder", s.paperBorder);

    return s;
}

RenderStyle styleFromJson(const QJsonObject &o)
{
    RenderStyle st;
    if (o.isEmpty()) return st;
    st.shadow     = o.value(QStringLiteral("shadow")).toBool(st.shadow);
    st.texture    = o.value(QStringLiteral("texture")).toBool(st.texture);
    st.weathering = o.value(QStringLiteral("weathering")).toBool(st.weathering);
    st.supplyStyle = qBound(0, o.value(QStringLiteral("supplyStyle")).toInt(st.supplyStyle), 2);
    return st;
}

QString csvOf(const QVector<CounterSpec> &items)
{
    auto q = [](const QString &t) { return QString("\"%1\"").arg(QString(t).replace('"', "\"\"")); };
    QString out = QStringLiteral("上级,番号,左标记,规模,兵种,战斗力,行动力,堆叠值,编制色\n");
    for (const auto &s : items) {
        out += QStringList{
            q(s.superior), q(s.designation), q(s.mark), q(echelonName(s.echelon)),
            q(branchName(s.branch)), QString::number(s.combat),
            QString::number(s.movement), QString::number(s.stacking),
            s.formationColor.name().toUpper()
        }.join(',') + QLatin1Char('\n');
    }
    return out;
}

QString projectOf(const QVector<CounterSpec> &items)
{
    QJsonArray arr;
    for (const auto &s : items) arr.append(s.toJson());
    const QJsonObject root{{QStringLiteral("app"), QStringLiteral("CounterForge")},
                           {QStringLiteral("version"), 1},
                           {QStringLiteral("counters"), arr}};
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool saveImage(const QImage &img, const QString &path, QString *why)
{
    if (img.isNull()) { *why = QStringLiteral("渲染结果为空"); return false; }
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!img.save(path)) { *why = QStringLiteral("写入失败"); return false; }
    return true;
}

// ══════════════════════════════════════════════════════════════════
//  机器可读的字段表：--schema
// ══════════════════════════════════════════════════════════════════
QJsonObject enumTable(int count,
                      const std::function<QString(int)> &name,
                      const std::function<QString(int)> &alt)
{
    QJsonArray arr;
    for (int i = 0; i < count; ++i) {
        QJsonObject e{{QStringLiteral("index"), i}, {QStringLiteral("name"), name(i)}};
        if (alt) {
            const QString a = alt(i);
            if (!a.isEmpty()) e[QStringLiteral("alt")] = a;
        }
        arr.append(e);
    }
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("enum")},
                       {QStringLiteral("values"), arr}};
}

QJsonObject schemaObject()
{
    QJsonObject f;

    f[QStringLiteral("superior")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QStringLiteral("72A.K")},
        {QStringLiteral("note"), QStringLiteral("算子左上角编号/上级代号")}};
    f[QStringLiteral("designation")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("note"), QStringLiteral("右侧竖排番号，配合 showDesignation 使用")}};
    f[QStringLiteral("mark")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("note"), QStringLiteral("左侧小标记，1–2 个字符，如 ○ / ★")}};
    f[QStringLiteral("echelon")] = enumTable(int(Echelon::Count),
        [](int i) { return echelonName(Echelon(i)); },
        [](int i) { return echelonGlyph(Echelon(i)); });
    f[QStringLiteral("branch")] = enumTable(int(Branch::Count),
        [](int i) { return branchName(Branch(i)); },
        [](int i) { return branchEnglish(Branch(i)); });
    f[QStringLiteral("combat")]   = QJsonObject{{QStringLiteral("type"), QStringLiteral("int")},
                                                {QStringLiteral("range"), QStringLiteral("0..99")},
                                                {QStringLiteral("default"), 1}};
    f[QStringLiteral("movement")] = QJsonObject{{QStringLiteral("type"), QStringLiteral("int")},
                                                {QStringLiteral("range"), QStringLiteral("0..99")},
                                                {QStringLiteral("default"), 8}};
    f[QStringLiteral("stacking")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("int")},
        {QStringLiteral("range"), QStringLiteral("1..2")},
        {QStringLiteral("default"), 1},
        {QStringLiteral("note"), QStringLiteral("1 = 军标左侧空圈，2 = 实心圈")}};

    QJsonArray presets;
    const QStringList pn = formationPresetNames();
    for (int i = 0; i < pn.size(); ++i) {
        presets.append(QJsonObject{{QStringLiteral("index"), i},
                                   {QStringLiteral("name"), pn.at(i)},
                                   {QStringLiteral("hex"), formationPreset(i).name().toUpper()}});
    }
    f[QStringLiteral("formationColor")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("color")},
        {QStringLiteral("note"), QStringLiteral("#RRGGBB / #AARRGGBB，或预设名/索引")},
        {QStringLiteral("presets"), presets}};

    f[QStringLiteral("autoTopColor")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("bool")}, {QStringLiteral("default"), true},
        {QStringLiteral("note"), QStringLiteral("true 时上部底色自动取编制色暗调")}};
    f[QStringLiteral("topColor")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("color")},
        {QStringLiteral("note"), QStringLiteral("autoTopColor=false 时生效")}};
    f[QStringLiteral("cornerPercent")] = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("int")},
        {QStringLiteral("range"), QStringLiteral("0..30")},
        {QStringLiteral("default"), 12}};
    f[QStringLiteral("showEchelon")]     = QJsonObject{{QStringLiteral("type"), QStringLiteral("bool")},
                                                       {QStringLiteral("default"), true}};
    f[QStringLiteral("showStackMark")]   = QJsonObject{{QStringLiteral("type"), QStringLiteral("bool")},
                                                       {QStringLiteral("default"), true}};
    f[QStringLiteral("showDesignation")] = QJsonObject{{QStringLiteral("type"), QStringLiteral("bool")},
                                                       {QStringLiteral("default"), false}};
    f[QStringLiteral("paperBorder")]     = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("bool")}, {QStringLiteral("default"), false},
        {QStringLiteral("note"), QStringLiteral("白色裁切边（印刷出血）")}};

    QJsonObject outputs;
    outputs[QStringLiteral("png")] = QJsonObject{
        {QStringLiteral("fields"), QJsonArray{
             QStringLiteral("index(可选，不写则导出全部，写 all 同效)"),
             QStringLiteral("file(可选)"), QStringLiteral("dir(可选)"),
             QStringLiteral("size(int, 16..4000, 默认 1200)"),
             QStringLiteral("transparent(bool, 默认 true)"),
             QStringLiteral("bleed(bool, 白色裁切边)")}}};
    outputs[QStringLiteral("sheet")] = QJsonObject{
        {QStringLiteral("fields"), QJsonArray{
             QStringLiteral("file"), QStringLiteral("dpi(150/300/600, 默认 300)"),
             QStringLiteral("mm(算子边长, 默认 22)"),
             QStringLiteral("cutMarks(bool, 默认 true)"),
             QStringLiteral("labels(bool, 默认 false)")}}};
    outputs[QStringLiteral("csv")] = QJsonObject{
        {QStringLiteral("fields"), QJsonArray{QStringLiteral("file")}}};
    outputs[QStringLiteral("project")] = QJsonObject{
        {QStringLiteral("fields"), QJsonArray{QStringLiteral("file")}},
        {QStringLiteral("note"), QStringLiteral("写回 CounterForge 工程文件（.cfproj）")}};

    return QJsonObject{
        {QStringLiteral("api"), QStringLiteral("counterforge.json/1")},
        {QStringLiteral("usage"),
         QStringLiteral("CounterForge.exe --json spec.json [--out 目录] [--result result.json]")},
        {QStringLiteral("topLevel"),
         QJsonObject{{QStringLiteral("counters"), QStringLiteral("算子数组（必填）")},
                     {QStringLiteral("style"), QStringLiteral("渲染样式，可省略")},
                     {QStringLiteral("outputs"), QStringLiteral("导出清单，省略则每枚导出一张 PNG")}}},
        {QStringLiteral("counterFields"), f},
        {QStringLiteral("styleFields"),
         QJsonObject{{QStringLiteral("shadow"), QStringLiteral("bool, 默认 true")},
                     {QStringLiteral("texture"), QStringLiteral("bool, 默认 true")},
                     {QStringLiteral("weathering"), QStringLiteral("bool, 默认 true")},
                     {QStringLiteral("supplyStyle"), QStringLiteral("int 0..2, 补给队中心图形")}}},
        {QStringLiteral("outputTypes"), outputs}};
}

// ══════════════════════════════════════════════════════════════════
//  主流程
// ══════════════════════════════════════════════════════════════════
int runSpec(const QString &specPath, const QString &outDir, const QString &resultPath)
{
    QByteArray raw;
    if (specPath == QLatin1String("-")) {
        QFile in;
        if (!in.open(stdin, QIODevice::ReadOnly))
            fail(QStringLiteral("无法读取标准输入"));
        raw = in.readAll();
    } else {
        QFile in(specPath);
        if (!in.open(QIODevice::ReadOnly))
            fail(QStringLiteral("打不开 spec 文件：%1").arg(specPath));
        raw = in.readAll();
    }

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
    if (doc.isNull())
        fail(QStringLiteral("JSON 解析失败：%1（第 %2 字节）")
                 .arg(perr.errorString()).arg(perr.offset));

    QJsonObject root;
    if (doc.isArray())       root[QStringLiteral("counters")] = doc.array();
    else if (doc.isObject()) root = doc.object();
    else                     fail(QStringLiteral("spec 必须是对象或数组"));

    QJsonArray arr = root.value(QStringLiteral("counters")).toArray();
    if (arr.isEmpty() && root.value(QStringLiteral("counter")).isObject())
        arr.append(root.value(QStringLiteral("counter")));
    if (arr.isEmpty())
        fail(QStringLiteral("缺少 counters 数组（也可以只给一个 counter 对象）"),
             QStringLiteral("counters"));

    QVector<CounterSpec> items;
    for (int i = 0; i < arr.size(); ++i) {
        if (!arr.at(i).isObject())
            fail(QStringLiteral("第 %1 项不是对象").arg(i), QStringLiteral("counters[%1]").arg(i));
        items.append(specFromJson(arr.at(i).toObject(),
                                  QStringLiteral("counters[%1]").arg(i)));
    }

    const RenderStyle st = styleFromJson(root.value(QStringLiteral("style")).toObject());

    QVector<QString> slugs;
    for (int i = 0; i < items.size(); ++i) {
        QString slug = slugOf(items.at(i), i);
        int n = 1;
        while (slugs.contains(slug))
            slug = slugOf(items.at(i), i) + QStringLiteral("_%1").arg(++n);
        slugs << slug;
    }

    const QString base = outDir.isEmpty() ? QStringLiteral("out") : outDir;
    QJsonArray written;

    // 相对路径一律以 --out（默认 ./out）为基准，绝对路径原样使用
    auto resolve = [&](const QString &p) {
        return QFileInfo(p).isRelative() ? base + QLatin1Char('/') + p : p;
    };
    auto outPath = [&](const QJsonObject &o, const QString &fallbackName) {
        const QString f = o.value(QStringLiteral("file")).toString();
        if (!f.isEmpty()) return resolve(f);
        const QString d = o.value(QStringLiteral("dir")).toString();
        return (d.isEmpty() ? base : resolve(d)) + QLatin1Char('/') + fallbackName;
    };

    // 没写 outputs：每枚一张 PNG，最省事
    QJsonArray outs = root.value(QStringLiteral("outputs")).toArray();
    if (outs.isEmpty())
        outs.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("png")}});

    for (int k = 0; k < outs.size(); ++k) {
        const QJsonObject o = outs.at(k).toObject();
        const QString type = o.value(QStringLiteral("type")).toString(QStringLiteral("png"));

        if (type == QLatin1String("png")) {
            int px = qBound(16, o.value(QStringLiteral("size")).toInt(1200), 4000);
            const bool transparent = o.value(QStringLiteral("transparent")).toBool(true);
            RenderStyle s2 = st;
            s2.paperBorder = o.value(QStringLiteral("bleed")).toBool(false);

            int index = -1;
            if (o.contains(QStringLiteral("index")) && !o.value(QStringLiteral("index")).isNull()) {
                const QJsonValue iv = o.value(QStringLiteral("index"));
                if (!(iv.isString() && iv.toString() == QLatin1String("all"))) index = iv.toInt(-1);
                if (index < 0 || index >= items.size())
                    fail(QStringLiteral("png.index 越界：%1").arg(index),
                         QStringLiteral("outputs[%1].index").arg(k));
            }

            const bool one = index >= 0;
            const int from = one ? index : 0;
            const int to   = one ? index : items.size() - 1;

            // 一次导多枚时，路径按目录处理：优先 dir，其次 file 所在目录，最后 --out
            QString dirForAll = o.value(QStringLiteral("dir")).toString();
            if (!dirForAll.isEmpty()) {
                dirForAll = resolve(dirForAll);
            } else {
                const QString f = o.value(QStringLiteral("file")).toString();
                dirForAll = f.isEmpty() ? base : QFileInfo(resolve(f)).absolutePath();
            }

            for (int i = from; i <= to; ++i) {
                QImage img = cf::counterImage(items.at(i), px, s2);
                if (!transparent) {
                    QImage bg(px, px, QImage::Format_ARGB32_Premultiplied);
                    bg.fill(QColor(255, 255, 255));
                    QPainter bp(&bg);
                    bp.drawImage(0, 0, img);
                    bp.end();
                    img = bg;
                }
                const QString path = one
                    ? outPath(o, slugs.at(i) + QStringLiteral(".png"))
                    : dirForAll + QLatin1Char('/') + slugs.at(i) + QStringLiteral(".png");
                QString why;
                if (!saveImage(img, path, &why))
                    fail(QStringLiteral("%1：%2").arg(why, path));
                written.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("png")},
                                           {QStringLiteral("index"), i},
                                           {QStringLiteral("path"), QFileInfo(path).absoluteFilePath()},
                                           {QStringLiteral("px"), px}});
            }
        } else if (type == QLatin1String("sheet")) {
            const int dpi = qBound(72, o.value(QStringLiteral("dpi")).toInt(300), 1200);
            const qreal mm = qBound(6.0, o.value(QStringLiteral("mm")).toDouble(22.0), 80.0);
            const bool cutMarks = o.value(QStringLiteral("cutMarks")).toBool(true);
            const bool labels = o.value(QStringLiteral("labels")).toBool(false);
            const QImage img = cf::sheetImage(items, dpi, mm, cutMarks, labels);
            const QString path = outPath(o, QStringLiteral("sheet_%1dpi.png").arg(dpi));
            QString why;
            if (!saveImage(img, path, &why))
                fail(QStringLiteral("%1：%2").arg(why, path));
            written.append(QJsonObject{{QStringLiteral("type"), QStringLiteral("sheet")},
                                       {QStringLiteral("path"), QFileInfo(path).absoluteFilePath()},
                                       {QStringLiteral("dpi"), dpi},
                                       {QStringLiteral("mm"), mm}});
        } else if (type == QLatin1String("csv") || type == QLatin1String("project")) {
            const bool csv = (type == QLatin1String("csv"));
            const QString path = outPath(o, csv ? QStringLiteral("counters.csv")
                                                : QStringLiteral("counters.cfproj"));
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                fail(QStringLiteral("写入失败：%1").arg(path));
            if (csv) {
                f.write("\xEF\xBB\xBF");
                f.write(csvOf(items).toUtf8());
            } else {
                f.write(projectOf(items).toUtf8());
            }
            f.close();
            written.append(QJsonObject{{QStringLiteral("type"), type},
                                       {QStringLiteral("path"), QFileInfo(path).absoluteFilePath()}});
        } else {
            fail(QStringLiteral("未知导出类型 \"%1\"（可用 png / sheet / csv / project）").arg(type),
                 QStringLiteral("outputs[%1].type").arg(k));
        }
    }

    QJsonObject result{
        {QStringLiteral("ok"), true},
        {QStringLiteral("api"), QStringLiteral("counterforge.json/1")},
        {QStringLiteral("counters"), items.size()},
        {QStringLiteral("written"), written}};

    if (!resultPath.isEmpty()) {
        QDir().mkpath(QFileInfo(resultPath).absolutePath());
        QFile f(resultPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    }
    printJson(result);
    return 0;
}

} // namespace

int runJsonApi(const QStringList &args)
{
    if (args.contains(QStringLiteral("--schema"))) {
        printJson(schemaObject());
        return 0;
    }

    const QString spec = argValue(args, QStringLiteral("--json"));
    if (spec.isEmpty())
        fail(QStringLiteral("用法：CounterForge.exe --json spec.json [--out 目录] [--result result.json]"));

    return runSpec(spec,
                   argValue(args, QStringLiteral("--out")),
                   argValue(args, QStringLiteral("--result")));
}

} // namespace cf
