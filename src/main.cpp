// main.cpp
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTimer>

#include "counterrenderer.h"
#include "jsonapi.h"
#include "legenddialog.h"
#include "maprenderer.h"
#include "mapspec.h"
#include "mainwindow.h"

// 诊断/自检用的样例算子
static QVector<cf::CounterSpec> demoCounters()
{
    auto mk = [](const QString &sup, cf::Echelon e, cf::Branch b, int c, int m,
                 int stk, const QColor &col, cf::Silhouette sil) {
        cf::CounterSpec s;
        s.superior = sup; s.echelon = e; s.branch = b;
        s.combat = c; s.movement = m; s.stacking = stk;
        s.formationColor = col; s.silhouette = sil; s.cornerPercent = 12;
        return s;
    };
    QVector<cf::CounterSpec> v;
    v << mk("72A.K", cf::Echelon::Division, cf::Branch::Armor, 4, 6, 2, cf::formationPreset(5), cf::Silhouette::ArmoredCar);
    v << mk("9Ge.K", cf::Echelon::Brigade, cf::Branch::Infantry, 3, 4, 1, cf::formationPreset(1), cf::Silhouette::None);
    v << mk("46A", cf::Echelon::Corps, cf::Branch::Infantry, 3, 4, 2, cf::formationPreset(2), cf::Silhouette::None);
    v << mk("补给", cf::Echelon::None, cf::Branch::Supply, 0, 5, 1, cf::formationPreset(7), cf::Silhouette::Truck);
    v << mk("70d.A", cf::Echelon::Regiment, cf::Branch::Rocket, 5, 3, 1, cf::formationPreset(3), cf::Silhouette::SPG);
    v << mk("3Uk.F", cf::Echelon::ArmyGroup, cf::Branch::Motorized, 7, 8, 2, cf::formationPreset(4), cf::Silhouette::Truck);
    v << mk("Pape", cf::Echelon::Kampfgruppe, cf::Branch::Mechanized, 10, 6, 2, cf::formationPreset(6), cf::Silhouette::Halftrack);
    v << mk("SS1Pz.K", cf::Echelon::Battalion, cf::Branch::Cavalry, 1, 8, 1, cf::formationPreset(5), cf::Silhouette::Motorcycle);
    v << mk("4GdA", cf::Echelon::Battalion, cf::Branch::HQ, 2, 4, 1, cf::formationPreset(12), cf::Silhouette::None);
    return v;
}

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CounterForge"));
    app.setApplicationDisplayName(QStringLiteral("CounterForge"));
    app.setOrganizationName(QStringLiteral("CounterForge"));
    app.setApplicationVersion(QStringLiteral("1.1.0"));

    if (QStyleFactory::keys().contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // 深色基调（QSS 负责细节，这里只保证原生控件跟随）
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(0x12, 0x10, 0x0C));
    pal.setColor(QPalette::WindowText, QColor(0xDC, 0xD2, 0xC0));
    pal.setColor(QPalette::Base, QColor(0x1E, 0x19, 0x13));
    pal.setColor(QPalette::AlternateBase, QColor(0x24, 0x1E, 0x17));
    pal.setColor(QPalette::Text, QColor(0xE4, 0xDA, 0xC8));
    pal.setColor(QPalette::Button, QColor(0x24, 0x1E, 0x17));
    pal.setColor(QPalette::ButtonText, QColor(0xDC, 0xD2, 0xC0));
    pal.setColor(QPalette::Highlight, QColor(0xC2, 0x90, 0x3F));
    pal.setColor(QPalette::HighlightedText, QColor(0x1C, 0x15, 0x08));
    pal.setColor(QPalette::ToolTipBase, QColor(0x24, 0x1E, 0x17));
    pal.setColor(QPalette::ToolTipText, QColor(0xE4, 0xDA, 0xC8));
    app.setPalette(pal);

    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    app.setFont(f);

    // 用一枚默认算子作为应用图标
    cf::CounterSpec iconSpec;
    iconSpec.superior = QStringLiteral("CF");
    iconSpec.branch = cf::Branch::Armor;
    iconSpec.echelon = cf::Echelon::Division;
    iconSpec.combat = 4;
    iconSpec.movement = 6;
    app.setWindowIcon(QIcon(QPixmap::fromImage(cf::iconImage(iconSpec, 128))));

    const QStringList args = app.arguments();
    auto argValue = [&args](const QString &key, const QString &fallback) {
        const int i = args.indexOf(key);
        return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : fallback;
    };

    // ── JSON 接口（供 Agent / 脚本调用，无界面）
    if (args.contains(QStringLiteral("--json")) || args.contains(QStringLiteral("--schema")))
        return cf::runJsonApi(args);

    // ── 地图：渲染一张战区图（--map 数据.json [--mapout 输出.png] [--dpi 300]）
    if (args.contains(QStringLiteral("--map"))) {
        const QString specPath = argValue(QStringLiteral("--map"), QString());
        QFile f(specPath);
        if (!f.open(QIODevice::ReadOnly)) {
            fprintf(stderr, "打不开地图数据：%s\n", qPrintable(specPath));
            return 2;
        }
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
        if (doc.isNull()) {
            fprintf(stderr, "地图 JSON 解析失败：%s\n", qPrintable(perr.errorString()));
            return 2;
        }
        const cf::MapSpec m = cf::MapSpec::fromJson(doc.object());
        if (!m.isValid()) {
            fprintf(stderr, "地图数据不完整（cols/rows/terrain）\n");
            return 2;
        }
        const int dpi = qBound(72, argValue(QStringLiteral("--dpi"), QStringLiteral("300")).toInt(), 600);
        QString out = argValue(QStringLiteral("--mapout"), QString());
        if (out.isEmpty()) out = QFileInfo(specPath).absolutePath() + QLatin1Char('/')
                                 + QFileInfo(specPath).completeBaseName() + QStringLiteral(".png");
        cf::MapStyle st;
        const QString styleArg = argValue(QStringLiteral("--mapstyle"), QString());
        st.satellite = (styleArg == QLatin1String("satellite"));
        QImage img = cf::mapImage(m, dpi, st);
        if (!img.save(out)) {
            fprintf(stderr, "写入失败：%s\n", qPrintable(out));
            return 2;
        }
        printf("%s  %dx%d px  %.0fx%.0f mm\n", qPrintable(out), img.width(), img.height(),
               cf::mapSizeMm(m).width(), cf::mapSizeMm(m).height());
        return 0;
    }

    // ── 自检：样张拼图
    if (args.contains(QStringLiteral("--selftest"))) {
        const QVector<cf::CounterSpec> demo = demoCounters();
        const int cell = 260, cols = 3, rows = (demo.size() + cols - 1) / cols;
        QImage sheet(cols * cell, rows * cell, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(QColor(0x1A, 0x1C, 0x21));
        QPainter sp(&sheet);
        sp.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 0; i < demo.size(); ++i) {
            const int r = i / cols, c = i % cols;
            cf::RenderStyle st;
            st.shadow = true;
            cf::paintCounter(&sp, QRectF(c * cell + 26, r * cell + 26, cell - 52, cell - 52),
                             demo[i], st);
        }
        sp.end();
        sheet.save(argValue(QStringLiteral("--selftest"), QStringLiteral("selftest.png")));
        return 0;
    }

    // ── 自检：字体对照
    if (args.contains(QStringLiteral("--fonttest"))) {
        const QStringList fams{"Arial Black", "Segoe UI Black", "Arial", "Segoe UI",
                               "Bahnschrift", "Impact", "Microsoft YaHei UI", "Franklin Gothic Medium"};
        QImage img(760, 132 * fams.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1B, 0x19, 0x16));
        QPainter fp(&img);
        fp.setRenderHint(QPainter::Antialiasing, true);
        fp.setRenderHint(QPainter::TextAntialiasing, true);
        for (int i = 0; i < fams.size(); ++i) {
            const QRectF band(0, i * 132, 760, 122);
            fp.fillRect(band, QColor(0x6E, 0x5A, 0x93));
            QFont f;
            f.setFamilies(QStringList{fams[i], "Arial"});
            f.setWeight(QFont::Black);
            f.setPixelSize(96);
            f.setLetterSpacing(QFont::PercentageSpacing, 99);
            fp.setFont(f);
            fp.setPen(QColor(253, 251, 246));
            QFontMetricsF fm(f);
            fp.drawText(QPointF(30, i * 132 + 52 + fm.capHeight() / 2), QStringLiteral("4-6"));
            fp.drawText(QPointF(330, i * 132 + 52 + fm.capHeight() / 2), QStringLiteral("10-6"));
            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(13);
            fp.setFont(lf);
            fp.setPen(QColor(0x24, 0x20, 0x18));
            fp.drawText(QPointF(560, i * 132 + 46), fams[i]);
        }
        fp.end();
        img.save(argValue(QStringLiteral("--fonttest"), QStringLiteral("fonts.png")));
        return 0;
    }

    // ── 自检：军标对照表
    if (args.contains(QStringLiteral("--syms"))) {
        const int cell = 240, cols = 5;
        const int n = int(cf::Branch::Count);
        const int rows = (n + cols - 1) / cols;
        QImage img(cols * cell, rows * cell, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1B, 0x19, 0x16));
        QPainter sp(&img);
        sp.setRenderHint(QPainter::Antialiasing, true);
        sp.setRenderHint(QPainter::TextAntialiasing, true);
        for (int i = 0; i < n; ++i) {
            const int r = i / cols, c = i % cols;
            const QRectF box(c * cell + 16, r * cell + 12, cell - 32, cell - 56);
            sp.setPen(Qt::NoPen);
            sp.setBrush(QColor(0x2C, 0x28, 0x22));
            sp.drawRoundedRect(box, 8, 8);
            cf::paintNatoSymbol(&sp, box.adjusted(24, 20, -24, -20),
                                QColor(250, 248, 242, 245), cf::Branch(i));
            QFont f(QStringLiteral("Microsoft YaHei UI"));
            f.setPixelSize(15);
            sp.setFont(f);
            sp.setPen(QColor(0xD6, 0xC8, 0xAE));
            sp.drawText(QRectF(box.left(), box.bottom() + 6, box.width(), 22),
                        Qt::AlignCenter, cf::branchName(cf::Branch(i)));
        }
        sp.end();
        img.save(argValue(QStringLiteral("--syms"), QStringLiteral("syms.png")));
        return 0;
    }

    // ── 自检：字体对照
    if (args.contains(QStringLiteral("--fonttest"))) {
        const QStringList fams{"Arial Black", "Segoe UI Black", "Arial", "Segoe UI",
                               "Bahnschrift", "Impact", "Microsoft YaHei UI", "Franklin Gothic Medium"};
        QImage img(760, 132 * fams.size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1B, 0x19, 0x16));
        QPainter fp(&img);
        fp.setRenderHint(QPainter::Antialiasing, true);
        fp.setRenderHint(QPainter::TextAntialiasing, true);
        for (int i = 0; i < fams.size(); ++i) {
            const QRectF band(0, i * 132, 760, 122);
            fp.fillRect(band, QColor(0x6E, 0x5A, 0x93));
            QFont f;
            f.setFamilies(QStringList{fams[i], "Arial"});
            f.setWeight(QFont::Black);
            f.setPixelSize(96);
            f.setLetterSpacing(QFont::PercentageSpacing, 99);
            fp.setFont(f);
            fp.setPen(QColor(253, 251, 246));
            QFontMetricsF fm(f);
            fp.drawText(QPointF(30, i * 132 + 52 + fm.capHeight() / 2), QStringLiteral("4-6"));
            fp.drawText(QPointF(330, i * 132 + 52 + fm.capHeight() / 2), QStringLiteral("10-6"));
            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(13);
            fp.setFont(lf);
            fp.setPen(QColor(0x24, 0x20, 0x18));
            fp.drawText(QPointF(560, i * 132 + 46), fams[i]);
        }
        fp.end();
        img.save(argValue(QStringLiteral("--fonttest"), QStringLiteral("fonts.png")));
        return 0;
    }

    // ── 自检：军标对照表
    if (args.contains(QStringLiteral("--syms"))) {
        const int cell = 240, cols = 5;
        const int n = int(cf::Branch::Count);
        const int rows = (n + cols - 1) / cols;
        QImage img(cols * cell, rows * cell, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1B, 0x19, 0x16));
        QPainter sp(&img);
        sp.setRenderHint(QPainter::Antialiasing, true);
        sp.setRenderHint(QPainter::TextAntialiasing, true);
        for (int i = 0; i < n; ++i) {
            const int r = i / cols, c = i % cols;
            const QRectF box(c * cell + 16, r * cell + 12, cell - 32, cell - 56);
            sp.setPen(Qt::NoPen);
            sp.setBrush(QColor(0x2C, 0x28, 0x22));
            sp.drawRoundedRect(box, 8, 8);
            cf::paintNatoSymbol(&sp, box.adjusted(24, 20, -24, -20),
                                QColor(250, 248, 242, 245), cf::Branch(i));
            QFont f(QStringLiteral("Microsoft YaHei UI"));
            f.setPixelSize(15);
            sp.setFont(f);
            sp.setPen(QColor(0xD6, 0xC8, 0xAE));
            sp.drawText(QRectF(box.left(), box.bottom() + 6, box.width(), 22),
                        Qt::AlignCenter, cf::branchName(cf::Branch(i)));
        }
        sp.end();
        img.save(argValue(QStringLiteral("--syms"), QStringLiteral("syms.png")));
        return 0;
    }

    // ── 自检：补给算子三种中心图形
    if (args.contains(QStringLiteral("--supoptions"))) {
        cf::CounterSpec sp;
        sp.superior = QString::fromUtf8("补给");
        sp.branch = cf::Branch::Supply;
        sp.echelon = cf::Echelon::None;
        sp.showEchelon = false;
        sp.showStackMark = false;
        sp.combat = 0;
        sp.movement = 5;
        sp.formationColor = cf::formationPreset(13);
        const int cell = 430, pad = 26;
        QImage img(cell * 3 + pad * 4, cell + pad * 2 + 34,
                   QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1A, 0x17, 0x13));
        QPainter op(&img);
        op.setRenderHint(QPainter::Antialiasing, true);
        const QStringList names{QString::fromUtf8("A 实心卡车"),
                                QString::fromUtf8("B 线稿卡车"),
                                QString::fromUtf8("C 北约补给符号")};
        for (int i = 0; i < 3; ++i) {
            cf::RenderStyle st;
            st.shadow = true;
            st.supplyStyle = i;
            cf::paintCounter(&op, QRectF(pad + i * (cell + pad), pad, cell, cell), sp, st);
            QFont f(QStringLiteral("Microsoft YaHei UI"));
            f.setPixelSize(17);
            op.setFont(f);
            op.setPen(QColor(0xD6, 0xC8, 0xAE));
            op.drawText(QRectF(pad + i * (cell + pad), pad + cell + 6, cell, 26),
                        Qt::AlignCenter, names.at(i));
        }
        op.end();
        img.save(argValue(QStringLiteral("--supoptions"), QStringLiteral("supoptions.png")));
        return 0;
    }

    // ── 自检：补给卡车图标
    if (args.contains(QStringLiteral("--truck"))) {
        const int W = 1000, H = 560;
        QImage img(W, H, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x22, 0x1F, 0x1B));
        QPainter tp(&img);
        tp.setRenderHint(QPainter::Antialiasing, true);
        cf::paintSupplyTruck(&tp, QRectF(40, 40, W - 80, H - 80),
                             QColor(0xF2, 0xEF, 0xE8, 240));
        tp.end();
        img.save(argValue(QStringLiteral("--truck"), QStringLiteral("truck.png")));
        return 0;
    }

    // ── 自检：单枚大图（可用 --br 指定兵种名，如 --br 骑兵）
    if (args.contains(QStringLiteral("--one"))) {
        cf::CounterSpec s;
        s.superior = QStringLiteral("72A.K");
        s.echelon = cf::Echelon::Division;
        s.branch = cf::Branch::Armor;
        s.combat = 4; s.movement = 6; s.stacking = 2;
        s.formationColor = cf::formationPreset(5);
        s.silhouette = cf::Silhouette::ArmoredCar;
        const QString wantBr = argValue(QStringLiteral("--br"), QString());
        if (!wantBr.isEmpty()) {
            for (int i = 0; i < int(cf::Branch::Count); ++i) {
                if (cf::branchName(cf::Branch(i)) == wantBr) { s.branch = cf::Branch(i); break; }
            }
        }
        const int n = 720;
        QImage img(n, n, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(0x1B, 0x17, 0x12));
        QPainter op(&img);
        op.setRenderHint(QPainter::Antialiasing, true);
        cf::RenderStyle st; st.shadow = true;
        cf::paintCounter(&op, QRectF(44, 44, n - 88, n - 88), s, st);
        op.end();
        img.save(argValue(QStringLiteral("--one"), QStringLiteral("one.png")));
        return 0;
    }

    // ── 自检：A4 拼版
    if (args.contains(QStringLiteral("--sheet"))) {
        const QImage img = cf::sheetImage(demoCounters(), 300, 22.0, true, false);
        img.save(argValue(QStringLiteral("--sheet"), QStringLiteral("sheet.png")));
        return 0;
    }

    // ── 自检：图例窗口
    if (args.contains(QStringLiteral("--legend"))) {
        auto *dlg = new LegendDialog;
        const QString out = argValue(QStringLiteral("--legend"), QStringLiteral("legend.png"));
        dlg->show();
        QTimer::singleShot(600, [dlg, out]() {
            if (auto *c = dlg->findChild<QWidget *>(QStringLiteral("LegendCanvas")))
                c->grab().save(out);
            else
                dlg->grab().save(out);
            qApp->quit();
        });
        return app.exec();
    }

    MainWindow w;

    // 命令行携带的项目文件（也支持从资源管理器拖到 exe 上）
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        if (!a.startsWith(QLatin1Char('-'))
            && (a.endsWith(QStringLiteral(".cfproj"), Qt::CaseInsensitive)
                || a.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))) {
            w.openPath(a);
            break;
        }
    }

    // ── 自检：主界面截图
    if (args.contains(QStringLiteral("--shot"))) {
        const QString out = argValue(QStringLiteral("--shot"), QStringLiteral("ui.png"));
        w.show();
        QTimer::singleShot(700, [&w, out]() {
            w.grab().save(out);
            const QString base = out.left(out.size() - 4);
            if (auto *side = w.findChild<QWidget *>(QStringLiteral("SideBar")))
                side->grab().save(base + QStringLiteral("_left.png"));
            if (auto *lib = w.findChild<QWidget *>(QStringLiteral("LibraryPanel")))
                lib->grab().save(base + QStringLiteral("_lib.png"));
            qApp->quit();
        });
    }

    w.show();
    return app.exec();
}
