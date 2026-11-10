// mainwindow.cpp
#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QImage>
#include <QLineEdit>
#include <QPainter>
#include <QPolygonF>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "counterrenderer.h"
#include "legenddialog.h"
#include "widgets.h"

// ══════════════════════════════════════════════════════════════════
static const char *kStyle = R"(
QMainWindow, QWidget#Root { background: #12100C; }
QMenuBar { background: #12100C; color: #A2957E; border-bottom: 1px solid #241E16; }
QMenuBar::item { padding: 5px 12px; background: transparent; }
QMenuBar::item:selected { background: #241E17; color: #EDE3CE; }
QMenu { background: #1B1710; color: #D6CBB6; border: 1px solid #332A20; padding: 5px; }
QMenu::item { padding: 6px 26px 6px 18px; border-radius: 4px; }
QMenu::item:selected { background: #2F271C; color: #F0E6D2; }
QMenu::separator { height: 1px; background: #332A20; margin: 5px 8px; }

QToolBar { background: #0D0B08; border: none; border-bottom: 1px solid #241E16; padding: 5px 10px; spacing: 4px; }
QToolBar QToolButton { color: #AFA28A; padding: 5px 11px; border-radius: 5px; font-size: 12px; }
QToolBar QToolButton:hover { background: #241E17; color: #F1E7D3; }
QToolBar QToolButton:pressed { background: #1A150F; }
QLabel#Brand { color: #D2A24A; font-weight: 700; letter-spacing: 0.6px; font-size: 14px; }

QScrollArea#SideBar { background: #12100C; border: none; }
QWidget#SideInner { background: #12100C; }
QWidget#LibraryPanel { background: #0F0D0A; border-left: 1px solid #241E16; }

QGroupBox { border: 1px solid #2A241B; border-radius: 7px; margin-top: 12px;
            padding: 12px 10px 9px 10px; color: #9A8D78; font-size: 11px; background: #191510; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 11px;
                   padding: 0 5px; color: #C2903F; font-size: 11px; letter-spacing: 2px; }

QLineEdit, QSpinBox, QComboBox {
    background: #1E1913; border: 1px solid #322A20; border-radius: 5px;
    padding: 4px 6px; color: #E4DAC8; font-size: 12px; selection-background-color: #4A3C28;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: #C2903F; }
QLineEdit:disabled, QSpinBox:disabled, QComboBox:disabled { color: #5E5343; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox::down-arrow { image: url("@ASSETS@/chev_down.png"); width: 12px; height: 8px; margin-right: 8px; }
QComboBox QAbstractItemView { background: #1E1913; color: #E4DAC8; border: 1px solid #322A20;
                              selection-background-color: #3A3021; padding: 3px; outline: none; }
QSpinBox::up-button, QSpinBox::down-button { width: 15px; background: #262017; border: none; }
QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #322A1E; }
QSpinBox::up-arrow { image: url("@ASSETS@/spin_up.png"); width: 9px; height: 6px; }
QSpinBox::down-arrow { image: url("@ASSETS@/spin_down.png"); width: 9px; height: 6px; }

QPushButton { background: #241E17; border: 1px solid #362D21; border-radius: 5px;
              padding: 7px 12px; color: #DCD2C0; font-size: 12px; }
QPushButton:hover { background: #2E261C; border-color: #4A3D2C; }
QPushButton:pressed { background: #1A150F; }
QPushButton#Primary { background: #C2903F; border: none; color: #1C1508; font-weight: bold; }
QPushButton#Primary:hover { background: #D2A24A; }
QPushButton#Color { font-family: Consolas; font-size: 11px; }

QCheckBox, QRadioButton { color: #BFB49F; font-size: 12px; spacing: 7px; }
QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid #453927;
                       border-radius: 3px; background: #1E1913; }
QCheckBox::indicator:hover { border-color: #C2903F; }
QCheckBox::indicator:checked { background: #C2903F; border-color: #D9A94F;
                               image: url("@ASSETS@/check.png"); }
QRadioButton::indicator { width: 13px; height: 13px; border-radius: 7px;
                          border: 1px solid #453927; background: #1E1913; }
QRadioButton::indicator:checked { border: 4px solid #C2903F; background: #12100C; }

QSlider::groove:horizontal { height: 4px; background: #2E271D; border-radius: 2px; }
QSlider::handle:horizontal { background: #C2903F; width: 12px; height: 12px;
                             margin: -5px 0; border-radius: 6px; }
QSlider::handle:horizontal:hover { background: #D9A94F; }

QListWidget { background: #0F0D0A; border: 1px solid #241E16; border-radius: 7px; outline: none; }
QListWidget::item { border-radius: 7px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #3A3024; border-radius: 5px; min-height: 26px; }
QScrollBar::handle:vertical:hover { background: #4A3D2C; }
QScrollBar:horizontal { background: transparent; height: 9px; margin: 2px; }
QScrollBar::handle:horizontal { background: #3A3024; border-radius: 4px; min-width: 26px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QStatusBar { background: #0D0B08; color: #7A6E5B; border-top: 1px solid #241E16; font-size: 11px; }
QStatusBar::item { border: none; }
QToolTip { background: #241E17; color: #E4DAC8; border: 1px solid #453927; padding: 4px 6px; }
)";

// ══════════════════════════════════════════════════════════════════
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    applyStyle();
    buildUi();

    cf::CounterSpec first;
    m_items.append(first);

    rebuildList(0);
    m_library->setCurrentRow(0);
    loadToUi(m_items[0]);
    refreshPreview();
    updateStatus();
    setDirty(false);
}

// ══════════════════════════════════════════════════════════════════
// 生成 QSS 需要的小图标（箭头、勾选），避免依赖外部资源文件
static QString buildUiAssets()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + QStringLiteral("/CounterForgeAssets");
    QDir().mkpath(dir);

    auto makeArrow = [&](const QString &name, int w, int h, bool up, const QColor &c) {
        const int s = 2;
        QImage img(w * s, h * s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(s, s);
        QPolygonF tri;
        if (up) tri << QPointF(0.6, h - 1.2) << QPointF(w - 0.6, h - 1.2) << QPointF(w / 2.0, 1.0);
        else    tri << QPointF(0.6, 1.2) << QPointF(w - 0.6, 1.2) << QPointF(w / 2.0, h - 1.0);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(tri);
        p.end();
        img.save(dir + QLatin1Char('/') + name);
    };

    makeArrow(QStringLiteral("chev_down.png"), 12, 8, false, QColor(0x9E, 0xA8, 0xB6));
    makeArrow(QStringLiteral("chev_up.png"), 12, 8, true, QColor(0x9E, 0xA8, 0xB6));
    makeArrow(QStringLiteral("spin_up.png"), 9, 6, true, QColor(0x8B, 0x95, 0xA2));
    makeArrow(QStringLiteral("spin_down.png"), 9, 6, false, QColor(0x8B, 0x95, 0xA2));

    {   // 勾选标记
        const int w = 12, s = 2;
        QImage img(w * s, w * s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.scale(s, s);
        p.setPen(QPen(QColor(0x1C, 0x16, 0x06), 1.9, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPolygonF tick;
        tick << QPointF(2.4, 6.2) << QPointF(4.9, 8.6) << QPointF(9.4, 3.5);
        p.drawPolyline(tick);
        p.end();
        img.save(dir + QStringLiteral("/check.png"));
    }

    return dir;
}

void MainWindow::applyStyle()
{
    const QString assets = buildUiAssets();
    QString css = QString::fromUtf8(kStyle);
    css.replace(QStringLiteral("@ASSETS@"), QDir::fromNativeSeparators(assets));
    qApp->setStyleSheet(css);
}

void MainWindow::buildUi()
{
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1460, avail.width() - 60), qMin(920, avail.height() - 70));
    setMinimumSize(1040, 680);

    // ── 菜单
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));

    auto mkAct = [this](const QString &text, const QKeySequence &key, auto slot) {
        auto *a = new QAction(text, this);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, slot);
        return a;
    };

    QAction *aNew   = mkAct(QStringLiteral("新建项目"), QKeySequence(QStringLiteral("Ctrl+Shift+N")), &MainWindow::newProject);
    QAction *aOpen  = mkAct(QStringLiteral("打开项目…"), QKeySequence::Open, &MainWindow::openProject);
    QAction *aSave  = mkAct(QStringLiteral("保存"), QKeySequence::Save, &MainWindow::saveProject);
    QAction *aSaveAs= mkAct(QStringLiteral("另存为…"), QKeySequence::SaveAs, &MainWindow::saveProjectAs);
    QAction *aPng   = mkAct(QStringLiteral("导出单枚 PNG…"), QKeySequence(QStringLiteral("Ctrl+E")), &MainWindow::exportPng);
    QAction *aSheet = mkAct(QStringLiteral("导出印刷拼版…"), QKeySequence(QStringLiteral("Ctrl+Shift+E")), &MainWindow::exportSheet);
    QAction *aCsv   = mkAct(QStringLiteral("导出数据表 CSV…"), QKeySequence(), &MainWindow::exportCsv);
    QAction *aLeg   = mkAct(QStringLiteral("符号图例"), QKeySequence(QStringLiteral("F1")), &MainWindow::showLegend);
    QAction *aQuit  = mkAct(QStringLiteral("退出"), QKeySequence::Quit, [this] { close(); });

    fileMenu->addAction(aNew);
    fileMenu->addAction(aOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(aSave);
    fileMenu->addAction(aSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(aPng);
    fileMenu->addAction(aSheet);
    fileMenu->addAction(aCsv);
    fileMenu->addSeparator();
    fileMenu->addAction(aQuit);

    QAction *aLib   = mkAct(QStringLiteral("新增算子"), QKeySequence(QStringLiteral("Ctrl+N")), &MainWindow::addCounter);
    QAction *aDup   = mkAct(QStringLiteral("复制算子"), QKeySequence(QStringLiteral("Ctrl+D")), &MainWindow::duplicateCounter);
    QAction *aDel   = mkAct(QStringLiteral("删除算子"), QKeySequence::Delete, &MainWindow::deleteCounter);

    viewMenu->addAction(aLib);
    viewMenu->addAction(aDup);
    viewMenu->addAction(aDel);

    QAction *aAbout = mkAct(QStringLiteral("关于 CounterForge"), QKeySequence(), [this] {
        QMessageBox::about(this, QStringLiteral("关于 CounterForge"),
            QStringLiteral("<b>CounterForge 1.0</b><br>"
                           "面向兵棋推演的算子在制设计器。<br><br>"
                           "纯 C++ / Qt6 矢量绘制，支持 300&nbsp;dpi 印刷级输出。<br>"
                           "符号体系参照北约 APP-6 风格。<br><br>"
                           "作者：wter"));
    });
    helpMenu->addAction(aLeg);
    helpMenu->addAction(aAbout);

    // ── 工具栏
    QToolBar *tb = addToolBar(QStringLiteral("main"));
    tb->setMovable(false);
    tb->setFloatable(false);
    auto *brand = new QLabel(QStringLiteral("CounterForge"));
    brand->setObjectName("Brand");
    tb->addWidget(brand);
    tb->addSeparator();
    tb->addAction(aNew);
    tb->addAction(aOpen);
    tb->addAction(aSave);
    tb->addSeparator();
    tb->addAction(aLib);
    tb->addAction(aDup);
    tb->addAction(aDel);
    tb->addSeparator();
    tb->addAction(aPng);
    tb->addAction(aSheet);
    tb->addAction(aCsv);
    tb->addSeparator();
    tb->addAction(aLeg);

    // ── 布局
    auto *root = new QWidget;
    root->setObjectName("Root");
    auto *h = new QHBoxLayout(root);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    h->addWidget(buildEditor());

    m_preview = new CounterPreview;
    h->addWidget(m_preview, 1);

    h->addWidget(buildLibrary());
    setCentralWidget(root);

    m_status = new QLabel;
    statusBar()->addPermanentWidget(m_status);

    auto *m_byline = new QLabel(QStringLiteral("wter"));
    m_byline->setStyleSheet(QStringLiteral("color:#6E6252;font-size:11px;padding-right:6px;"));
    statusBar()->addPermanentWidget(m_byline);
    statusBar()->showMessage(QStringLiteral("就绪"));
}

// ══════════════════════════════════════════════════════════════════
QWidget *MainWindow::buildEditor()
{
    auto *scroll = new QScrollArea;
    scroll->setObjectName("SideBar");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(340);

    auto *w = new QWidget;
    w->setObjectName("SideInner");
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(13, 11, 10, 14);
    v->setSpacing(9);

    auto form = [](QGroupBox *g) {
        auto *f = new QFormLayout(g);
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        f->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        f->setHorizontalSpacing(10);
        f->setVerticalSpacing(7);
        f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return f;
    };

    // ── 编制信息
    auto *g1 = new QGroupBox(QStringLiteral("编 制 信 息"));
    auto *f1 = form(g1);
    m_superior = new QLineEdit;
    m_superior->setPlaceholderText(QStringLiteral("如 72A.K / 46A"));
    m_superior->setMaxLength(18);
    f1->addRow(QStringLiteral("上级"), m_superior);

    auto *idRow = new QWidget;
    auto *idLay = new QHBoxLayout(idRow);
    idLay->setContentsMargins(0, 0, 0, 0);
    idLay->setSpacing(6);

    m_designation = new QLineEdit;
    m_designation->setPlaceholderText(QStringLiteral("番号，如 27"));
    m_designation->setMaxLength(14);
    idLay->addWidget(m_designation, 1);

    m_mark = new QLineEdit;
    m_mark->setPlaceholderText(QStringLiteral("○"));
    m_mark->setMaxLength(2);
    m_mark->setFixedWidth(52);
    m_mark->setAlignment(Qt::AlignCenter);
    m_mark->setToolTip(QStringLiteral("算子左侧的小标记，填 1–2 个字符（如 ○ / ★ / 空）"));
    idLay->addWidget(m_mark);
    f1->addRow(QStringLiteral("番号 / 标记"), idRow);

    m_echelon = new QComboBox;
    m_echelon->addItems(cf::echelonNames());
    m_echelon->setToolTip(QStringLiteral("北约梯队符号：班 • / 连 I / 营 II / 旅 X / 师 XX …"));
    f1->addRow(QStringLiteral("规模"), m_echelon);

    m_branch = new QComboBox;
    m_branch->addItems(cf::branchNames());
    f1->addRow(QStringLiteral("兵种"), m_branch);
    v->addWidget(g1);

    // ── 战斗数值
    auto *g2 = new QGroupBox(QStringLiteral("战 斗 数 值"));
    auto *l2 = new QVBoxLayout(g2);
    l2->setSpacing(6);
    auto *nums = new QWidget;
    auto *gn = new QGridLayout(nums);
    gn->setContentsMargins(0, 0, 0, 0);
    gn->setHorizontalSpacing(8);
    gn->setVerticalSpacing(4);

    auto addNum = [&](int col, const QString &name, QSpinBox *&box, int hi,
                      const QString &tip) {
        auto *lb = new QLabel(name);
        lb->setStyleSheet(QStringLiteral("color:#93866F;font-size:11px;"));
        box = new QSpinBox;
        box->setRange(0, hi);
        box->setToolTip(tip);
        box->setAlignment(Qt::AlignCenter);
        gn->addWidget(lb, 0, col);
        gn->addWidget(box, 1, col);
    };
    addNum(0, QStringLiteral("战斗力"), m_combat, 99, QStringLiteral("算子上部左侧数值"));
    addNum(1, QStringLiteral("行动力"), m_movement, 99, QStringLiteral("算子上部右侧数值"));
    addNum(2, QStringLiteral("堆叠"), m_stacking, 2, QStringLiteral("格内占几点：1 印空圈，2 印实心圈（军标左侧）"));
    m_stacking->setMinimum(1);
    l2->addWidget(nums);
    v->addWidget(g2);

    // ── 编制色
    auto *g3 = new QGroupBox(QStringLiteral("编 制 色"));
    auto *v3 = new QVBoxLayout(g3);
    v3->setSpacing(7);

    m_colorBtn = new QPushButton;
    m_colorBtn->setObjectName("Color");
    m_colorBtn->setCursor(Qt::PointingHandCursor);
    m_colorBtn->setToolTip(QStringLiteral("点击打开拾色器"));
    v3->addWidget(m_colorBtn);

    auto *pal = new QWidget;
    auto *grid = new QGridLayout(pal);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    for (int i = 0; i < 16; ++i) {
        const QColor c = cf::formationPreset(i);
        auto *b = new QPushButton(pal);
        b->setFixedSize(27, 22);
        b->setCursor(Qt::PointingHandCursor);
        b->setToolTip(cf::formationPresetNames().value(i));
        b->setStyleSheet(QStringLiteral("QPushButton{background:%1;border:1px solid rgba(255,255,255,0.16);"
                                        "border-radius:4px;} QPushButton:hover{border:2px solid #E0BE50;}")
                             .arg(c.name()));
        connect(b, &QPushButton::clicked, this, [this, c] {
            if (m_updating || m_items.isEmpty()) return;
            m_items[m_current].formationColor = c;
            if (m_items[m_current].autoTopColor) { /* 上部自动跟随 */ }
            updateColorUi();
            refreshPreview();
            refreshRow(m_current);
            setDirty(true);
        });
        grid->addWidget(b, i / 8, i % 8);
    }
    v3->addWidget(pal);

    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color:#2A241B;"));
    v3->addWidget(line);

    m_autoTop = new QRadioButton(QStringLiteral("上部底色自动加深"));
    m_manualTop = new QRadioButton(QStringLiteral("上部底色自定义"));
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(8);
    topRow->addWidget(m_autoTop, 1);
    topRow->addWidget(m_manualTop);
    v3->addLayout(topRow);

    m_topColorBtn = new QPushButton;
    m_topColorBtn->setObjectName("Color");
    m_topColorBtn->setCursor(Qt::PointingHandCursor);
    v3->addWidget(m_topColorBtn);
    v->addWidget(g3);

    // ── 外观
    auto *g4 = new QGroupBox(QStringLiteral("外 观"));
    auto *f4 = form(g4);
    auto *radRow = new QWidget;
    auto *radLay = new QHBoxLayout(radRow);
    radLay->setContentsMargins(0, 0, 0, 0);
    radLay->setSpacing(8);
    m_radius = new QSlider(Qt::Horizontal);
    m_radius->setRange(0, 30);
    m_radiusVal = new QLabel(QStringLiteral("0%"));
    m_radiusVal->setFixedWidth(34);
    m_radiusVal->setStyleSheet(QStringLiteral("color:#93866F;font-family:Consolas;"));
    radLay->addWidget(m_radius, 1);
    radLay->addWidget(m_radiusVal);
    f4->addRow(QStringLiteral("圆角"), radRow);

    m_showEchelon = new QCheckBox(QStringLiteral("显示规模符号"));
    m_showStackMark = new QCheckBox(QStringLiteral("显示堆叠圈"));
    m_showStackMark->setToolTip(QStringLiteral("军标左侧的小圈：空圈＝占 1 点，实心圈＝占 2 点"));
    m_showDesignation = new QCheckBox(QStringLiteral("显示番号"));
    m_paperBorder = new QCheckBox(QStringLiteral("白色裁切边"));
    m_hexToggle = new QCheckBox(QStringLiteral("六角格预览"));
    m_hexToggle->setChecked(true);

    auto *checks = new QWidget;
    auto *gc = new QGridLayout(checks);
    gc->setContentsMargins(0, 4, 0, 0);
    gc->setHorizontalSpacing(10);
    gc->setVerticalSpacing(7);
    gc->addWidget(m_showEchelon, 0, 0);
    gc->addWidget(m_showStackMark, 0, 1);
    gc->addWidget(m_showDesignation, 1, 0);
    gc->addWidget(m_paperBorder, 1, 1);
    gc->addWidget(m_hexToggle, 2, 0);
    f4->addRow(checks);
    v->addWidget(g4);

    v->addStretch();

    scroll->setWidget(w);

    // ── 连接
    connect(m_superior, &QLineEdit::textEdited, this, &MainWindow::syncFromUi);
    connect(m_designation, &QLineEdit::textEdited, this, &MainWindow::syncFromUi);
    connect(m_mark, &QLineEdit::textEdited, this, &MainWindow::syncFromUi);
    connect(m_echelon, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { syncFromUi(); });
    connect(m_branch, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { onBranchChanged(); });
    connect(m_combat, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncFromUi(); });
    connect(m_movement, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncFromUi(); });
    connect(m_stacking, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { syncFromUi(); });
    connect(m_radius, &QSlider::valueChanged, this, [this](int v) {
        m_radiusVal->setText(QStringLiteral("%1%").arg(v));
        syncFromUi();
    });
    connect(m_showEchelon, &QCheckBox::toggled, this, [this](bool) { syncFromUi(); });
    connect(m_showStackMark, &QCheckBox::toggled, this, [this](bool) { syncFromUi(); });
    connect(m_showDesignation, &QCheckBox::toggled, this, [this](bool) { syncFromUi(); });
    connect(m_paperBorder, &QCheckBox::toggled, this, [this](bool) { syncFromUi(); });
    connect(m_hexToggle, &QCheckBox::toggled, this, [this](bool v) {
        if (m_preview) m_preview->setShowHex(v);
    });
    connect(m_colorBtn, &QPushButton::clicked, this, &MainWindow::chooseColor);
    connect(m_topColorBtn, &QPushButton::clicked, this, &MainWindow::chooseTopColor);
    connect(m_autoTop, &QRadioButton::toggled, this, [this](bool on) {
        if (m_updating || m_items.isEmpty()) return;
        m_items[m_current].autoTopColor = on;
        updateColorUi();
        refreshPreview();
        setDirty(true);
    });
    connect(m_manualTop, &QRadioButton::toggled, this, [this](bool on) {
        if (m_updating || m_items.isEmpty() || on == false) return;
        m_items[m_current].autoTopColor = false;
        updateColorUi();
        refreshPreview();
        setDirty(true);
    });

    return scroll;
}

// ══════════════════════════════════════════════════════════════════
QWidget *MainWindow::buildLibrary()
{
    auto *panel = new QWidget;
    panel->setObjectName("LibraryPanel");
    panel->setFixedWidth(302);

    auto *v = new QVBoxLayout(panel);
    v->setContentsMargins(14, 14, 14, 14);
    v->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("算 子 库"));
    title->setStyleSheet(QStringLiteral("color:#C2903F;font-size:11px;letter-spacing:2px;font-weight:bold;"));
    v->addWidget(title);

    auto *hint = new QLabel(QStringLiteral("单击选择 · 右键更多操作"));
    hint->setStyleSheet(QStringLiteral("color:#6E6350;font-size:11px;"));
    v->addWidget(hint);

    m_library = new QListWidget;
    m_library->setItemDelegate(new CounterItemDelegate(this));
    m_library->setUniformItemSizes(true);
    m_library->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_library->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_library->setContextMenuPolicy(Qt::CustomContextMenu);
    v->addWidget(m_library, 1);

    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    auto *bAdd = new QPushButton(QStringLiteral("＋ 新增"));
    auto *bDup = new QPushButton(QStringLiteral("复制"));
    auto *bDel = new QPushButton(QStringLiteral("删除"));
    row->addWidget(bAdd, 1);
    row->addWidget(bDup, 1);
    row->addWidget(bDel, 1);
    v->addLayout(row);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#241E16;"));
    v->addWidget(sep);

    auto *expTitle = new QLabel(QStringLiteral("导 出"));
    expTitle->setStyleSheet(QStringLiteral("color:#C2903F;font-size:11px;letter-spacing:2px;font-weight:bold;"));
    v->addWidget(expTitle);

    auto *bPng = new QPushButton(QStringLiteral("导出单枚 PNG"));
    bPng->setObjectName("Primary");
    auto *bSheet = new QPushButton(QStringLiteral("导出印刷拼版（A4）"));
    auto *bCsv = new QPushButton(QStringLiteral("导出数据表 CSV"));
    for (auto *b : {bPng, bSheet, bCsv})
        b->setCursor(Qt::PointingHandCursor);
    v->addWidget(bPng);
    v->addWidget(bSheet);
    v->addWidget(bCsv);

    connect(m_library, &QListWidget::currentRowChanged, this, &MainWindow::selectIndex);
    connect(bAdd, &QPushButton::clicked, this, &MainWindow::addCounter);
    connect(bDup, &QPushButton::clicked, this, &MainWindow::duplicateCounter);
    connect(bDel, &QPushButton::clicked, this, &MainWindow::deleteCounter);
    connect(bPng, &QPushButton::clicked, this, &MainWindow::exportPng);
    connect(bSheet, &QPushButton::clicked, this, &MainWindow::exportSheet);
    connect(bCsv, &QPushButton::clicked, this, &MainWindow::exportCsv);

    connect(m_library, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const QModelIndex idx = m_library->indexAt(pos);
        QMenu menu(this);
        if (idx.isValid()) {
            m_library->setCurrentRow(idx.row());
            menu.addAction(QStringLiteral("复制算子"), this, &MainWindow::duplicateCounter);
            menu.addAction(QStringLiteral("删除算子"), this, &MainWindow::deleteCounter);
            menu.addSeparator();
            if (idx.row() > 0)
                menu.addAction(QStringLiteral("上移"), this, [this] { moveCounter(-1); });
            if (idx.row() < m_items.size() - 1)
                menu.addAction(QStringLiteral("下移"), this, [this] { moveCounter(1); });
            menu.addSeparator();
        }
        menu.addAction(QStringLiteral("新增算子"), this, &MainWindow::addCounter);
        menu.exec(m_library->viewport()->mapToGlobal(pos));
    });

    return panel;
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::rebuildList(int selectRow)
{
    const QSignalBlocker block(m_library);
    m_library->clear();
    for (const auto &s : m_items) {
        auto *it = new QListWidgetItem(s.title());
        it->setData(Qt::UserRole, s.toJson());
        it->setSizeHint(QSize(280, 74));
        m_library->addItem(it);
    }
    m_current = qBound(0, selectRow, qMax(0, m_items.size() - 1));
    m_library->setCurrentRow(m_current);
}

void MainWindow::selectIndex(int row)
{
    if (m_updating || row < 0 || row >= m_items.size()) return;
    m_current = row;
    loadToUi(m_items[row]);
    refreshPreview();
    updateStatus();
}

void MainWindow::loadToUi(const cf::CounterSpec &s)
{
    m_updating = true;
    m_superior->setText(s.superior);
    m_designation->setText(s.designation);
    m_mark->setText(s.mark);
    m_echelon->setCurrentIndex(int(s.echelon));
    m_branch->setCurrentIndex(int(s.branch));
    m_combat->setValue(s.combat);
    m_movement->setValue(s.movement);
    m_stacking->setValue(s.stacking);
    m_radius->setValue(s.cornerPercent);
    m_radiusVal->setText(QStringLiteral("%1%").arg(s.cornerPercent));
    m_showEchelon->setChecked(s.showEchelon);
    m_showStackMark->setChecked(s.showStackMark);
    m_showDesignation->setChecked(s.showDesignation);
    m_paperBorder->setChecked(s.paperBorder);
    m_autoTop->setChecked(s.autoTopColor);
    m_manualTop->setChecked(!s.autoTopColor);

    const bool supply = (s.branch == cf::Branch::Supply);
    m_combat->setEnabled(!supply);
    m_echelon->setEnabled(!supply);

    m_updating = false;
    updateColorUi();
}

void MainWindow::updateColorUi()
{
    if (m_items.isEmpty()) return;
    const cf::CounterSpec &s = m_items[m_current];

    m_colorBtn->setText(QStringLiteral("编制色   %1   ●").arg(s.formationColor.name().toUpper()));
    m_colorBtn->setStyleSheet(QStringLiteral(
        "QPushButton#Color{background:%1;color:%2;border:1px solid rgba(255,255,255,0.20);"
        "border-radius:5px;padding:8px 10px;text-align:left;font-family:Consolas;font-size:11px;}")
        .arg(s.formationColor.name(),
             s.formationColor.lightness() > 140 ? QStringLiteral("#1A1A1A") : QStringLiteral("#FFFFFF")));

    const QColor top = s.autoTopColor ? QColor(0x45, 0x45, 0x4A) : s.topColor;
    m_topColorBtn->setText(s.autoTopColor
        ? QStringLiteral("上部底色   自动（跟随编制色）")
        : QStringLiteral("上部底色   %1   ●").arg(s.topColor.name().toUpper()));
    m_topColorBtn->setStyleSheet(QStringLiteral(
        "QPushButton#Color{background:%1;color:%2;border:1px solid rgba(255,255,255,0.20);"
        "border-radius:5px;padding:8px 10px;text-align:left;font-family:Consolas;font-size:11px;}")
        .arg(s.autoTopColor ? QStringLiteral("#241E17") : s.topColor.name(),
             "#D6CBB6"));
    m_topColorBtn->setEnabled(!s.autoTopColor);
    Q_UNUSED(top);
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::syncFromUi()
{
    if (m_updating || m_items.isEmpty()) return;

    cf::CounterSpec s = m_items[m_current];
    s.superior        = m_superior->text();
    s.designation     = m_designation->text();
    s.mark            = m_mark->text();
    s.echelon         = cf::Echelon(qBound(0, m_echelon->currentIndex(), int(cf::Echelon::Count) - 1));
    s.branch          = cf::Branch(qBound(0, m_branch->currentIndex(), int(cf::Branch::Count) - 1));
    s.combat          = m_combat->value();
    s.movement        = m_movement->value();
    s.stacking        = qBound(1, m_stacking->value(), 2);
    s.cornerPercent   = m_radius->value();
    s.showEchelon     = m_showEchelon->isChecked();
    s.showStackMark   = m_showStackMark->isChecked();
    s.showDesignation = m_showDesignation->isChecked();
    s.paperBorder     = m_paperBorder->isChecked();

    commit(s);
}

void MainWindow::commit(const cf::CounterSpec &s)
{
    m_items[m_current] = s;
    refreshPreview();
    refreshRow(m_current);
    setDirty(true);
    updateStatus();
}

void MainWindow::refreshPreview()
{
    if (m_items.isEmpty()) return;
    m_preview->setSpec(m_items[m_current]);
}

void MainWindow::refreshRow(int row)
{
    if (row < 0 || row >= m_items.size() || row >= m_library->count()) return;
    const QSignalBlocker block(m_library);
    auto *it = m_library->item(row);
    it->setData(Qt::UserRole, m_items[row].toJson());
    it->setText(m_items[row].title());
}

void MainWindow::onBranchChanged()
{
    if (m_updating || m_items.isEmpty()) return;

    const cf::Branch newB = cf::Branch(qBound(0, m_branch->currentIndex(), int(cf::Branch::Count) - 1));

    const bool supply = (newB == cf::Branch::Supply);
    m_combat->setEnabled(!supply);
    m_echelon->setEnabled(!supply);

    syncFromUi();
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::chooseColor()
{
    if (m_items.isEmpty()) return;
    const QColor c = QColorDialog::getColor(m_items[m_current].formationColor, this,
                                            QStringLiteral("选择编制色"),
                                            QColorDialog::ShowAlphaChannel);
    if (!c.isValid()) return;
    m_items[m_current].formationColor = c;
    updateColorUi();
    refreshPreview();
    refreshRow(m_current);
    setDirty(true);
}

void MainWindow::chooseTopColor()
{
    if (m_items.isEmpty()) return;
    const QColor c = QColorDialog::getColor(m_items[m_current].topColor, this,
                                            QStringLiteral("选择上部底色"));
    if (!c.isValid()) return;
    m_items[m_current].autoTopColor = false;
    m_items[m_current].topColor = c;
    m_autoTop->setChecked(false);
    m_manualTop->setChecked(true);
    updateColorUi();
    refreshPreview();
    setDirty(true);
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::addCounter()
{
    cf::CounterSpec s;
    if (!m_items.isEmpty()) {
        s.formationColor = m_items[m_current].formationColor;
        s.superior = m_items[m_current].superior;
    }
    m_items.insert(m_current + 1, s);
    rebuildList(m_current + 1);
    loadToUi(m_items[m_current]);
    refreshPreview();
    setDirty(true);
    updateStatus();
}

void MainWindow::duplicateCounter()
{
    if (m_items.isEmpty()) return;
    m_items.insert(m_current + 1, m_items[m_current]);
    rebuildList(m_current + 1);
    loadToUi(m_items[m_current]);
    refreshPreview();
    setDirty(true);
    updateStatus();
}

void MainWindow::deleteCounter()
{
    if (m_items.size() <= 1) {
        statusBar()->showMessage(QStringLiteral("至少保留一枚算子"), 3000);
        return;
    }
    m_items.remove(m_current);
    rebuildList(qBound(0, m_current - 1, m_items.size() - 1));
    loadToUi(m_items[m_current]);
    refreshPreview();
    setDirty(true);
    updateStatus();
}

void MainWindow::moveCounter(int delta)
{
    if (m_items.size() < 2) return;
    const int to = m_current + delta;
    if (to < 0 || to >= m_items.size()) return;
    m_items.move(m_current, to);
    rebuildList(to);
    loadToUi(m_items[m_current]);
    refreshPreview();
    setDirty(true);
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::newProject()
{
    if (!maybeSave()) return;
    m_items.clear();
    m_items.append(cf::CounterSpec());
    m_path.clear();
    rebuildList(0);
    loadToUi(m_items[0]);
    refreshPreview();
    setDirty(false);
    updateStatus();
}

void MainWindow::openProject()
{
    if (!maybeSave()) return;
    const QString fn = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开算子项目"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("CounterForge 项目 (*.cfproj *.json)"));
    if (fn.isEmpty()) return;
    openPath(fn);
}

bool MainWindow::openPath(const QString &fileName)
{
    const QString fn = fileName;
    QFile f(fn);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), f.errorString());
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
                             QStringLiteral("文件格式不正确：%1").arg(err.errorString()));
        return false;
    }
    const QJsonArray arr = doc.object().value("counters").toArray();
    if (arr.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("项目中没有算子。"));
        return false;
    }

    m_items.clear();
    for (const auto &val : arr) m_items.append(cf::CounterSpec::fromJson(val.toObject()));
    m_path = fn;
    rebuildList(0);
    loadToUi(m_items[0]);
    refreshPreview();
    setDirty(false);
    updateStatus();
    statusBar()->showMessage(QStringLiteral("已打开 %1").arg(QFileInfo(fn).fileName()), 4000);
    return true;
}

void MainWindow::saveProject()
{
    if (m_path.isEmpty()) { saveProjectAs(); return; }

    QJsonArray arr;
    for (const auto &s : m_items) arr.append(s.toJson());
    QJsonObject root;
    root["app"] = QStringLiteral("CounterForge");
    root["version"] = 1;
    root["counters"] = arr;

    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), f.errorString());
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    setDirty(false);
    statusBar()->showMessage(QStringLiteral("已保存 %1").arg(QFileInfo(m_path).fileName()), 4000);
}

void MainWindow::saveProjectAs()
{
    QString fn = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存算子项目"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/我的算子.cfproj",
        QStringLiteral("CounterForge 项目 (*.cfproj)"));
    if (fn.isEmpty()) return;
    if (!fn.endsWith(".cfproj", Qt::CaseInsensitive) && !fn.endsWith(".json", Qt::CaseInsensitive))
        fn += ".cfproj";
    m_path = fn;
    saveProject();
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::exportPng()
{
    if (m_items.isEmpty()) return;
    const cf::CounterSpec &s = m_items[m_current];

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("导出单枚 PNG"));
    dlg.setMinimumWidth(340);
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    form->setSpacing(9);

    auto *px = new QSpinBox(&dlg);
    px->setRange(64, 2400);
    px->setValue(750);
    px->setSuffix(QStringLiteral(" px"));
    form->addRow(QStringLiteral("输出边长"), px);

    auto *paper = new QCheckBox(QStringLiteral("带白色裁切边"), &dlg);
    paper->setChecked(s.paperBorder);
    form->addRow(QString(), paper);

    auto *info = new QLabel(QStringLiteral("750 px ≈ 300 dpi 下的 63 mm 见方，适合屏幕与厚涂打印"));
    info->setStyleSheet(QStringLiteral("color:#8A7D66;font-size:11px;"));
    info->setWordWrap(true);
    form->addRow(info);
    lay->addLayout(form);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("选择保存位置"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    QString base = s.superior.trimmed();
    if (base.isEmpty()) base = cf::branchName(s.branch);
    base.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), "_");

    const QString fn = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存 PNG"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/" + base + ".png",
        QStringLiteral("PNG 图片 (*.png)"));
    if (fn.isEmpty()) return;

    cf::RenderStyle st;
    st.paperBorder = paper->isChecked();
    const QImage img = cf::counterImage(s, px->value(), st);
    if (!img.save(fn)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件：%1").arg(fn));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已导出 %1（%2×%2）").arg(QFileInfo(fn).fileName())
                                 .arg(px->value()), 6000);
}

void MainWindow::exportSheet()
{
    if (m_items.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("导出印刷拼版（A4）"));
    dlg.setMinimumWidth(360);
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    form->setSpacing(9);

    auto *dpi = new QComboBox(&dlg);
    dpi->addItem(QStringLiteral("150 dpi（草稿）"), 150);
    dpi->addItem(QStringLiteral("300 dpi（推荐印刷）"), 300);
    dpi->addItem(QStringLiteral("600 dpi（高品质）"), 600);
    dpi->setCurrentIndex(1);
    form->addRow(QStringLiteral("输出分辨率"), dpi);

    auto *mm = new QDoubleSpinBox(&dlg);
    mm->setRange(14.0, 30.0);
    mm->setValue(22.0);
    mm->setSingleStep(1.0);
    mm->setSuffix(QStringLiteral(" mm"));
    form->addRow(QStringLiteral("算子边长"), mm);

    auto *cuts = new QCheckBox(QStringLiteral("打印裁切标记"), &dlg);
    cuts->setChecked(true);
    form->addRow(QString(), cuts);

    auto *labels = new QCheckBox(QStringLiteral("算子下方标注名称"), &dlg);
    labels->setChecked(false);
    form->addRow(QString(), labels);

    auto *note = new QLabel(QStringLiteral("整库中的全部算子会依次排版；数量不足时自动重复填充整页。"));
    note->setStyleSheet(QStringLiteral("color:#8A7D66;font-size:11px;"));
    note->setWordWrap(true);
    form->addRow(note);
    lay->addLayout(form);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("生成"));
    lay->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString fn = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存拼版"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/算子拼版_A4.png",
        QStringLiteral("PNG 图片 (*.png)"));
    if (fn.isEmpty()) return;

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    const QImage img = cf::sheetImage(m_items, dpi->currentData().toInt(), mm->value(),
                                      cuts->isChecked(), labels->isChecked());
    QGuiApplication::restoreOverrideCursor();

    if (img.isNull() || !img.save(fn)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件：%1").arg(fn));
        return;
    }
    statusBar()->showMessage(QStringLiteral("已生成 A4 拼版 %1 × %2 px")
                                 .arg(img.width()).arg(img.height()), 6000);
}

void MainWindow::exportCsv()
{
    if (m_items.isEmpty()) return;
    QString fn = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出数据表"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/算子数据.csv",
        QStringLiteral("CSV 表格 (*.csv)"));
    if (fn.isEmpty()) return;

    QFile f(fn);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), f.errorString());
        return;
    }
    f.write("\xEF\xBB\xBF");  // UTF-8 BOM，便于 Excel 识别
    f.write(QStringLiteral("上级,番号,左标记,规模,兵种,战斗力,行动力,堆叠值,编制色\n").toUtf8());
    for (const auto &s : m_items) {
        auto q = [](const QString &t) { return QString("\"%1\"").arg(QString(t).replace("\"", "\"\"")); };
        const QString line = QStringList{
            q(s.superior), q(s.designation), q(s.mark), q(cf::echelonName(s.echelon)),
            q(cf::branchName(s.branch)), QString::number(s.combat),
            QString::number(s.movement), QString::number(s.stacking),
            s.formationColor.name().toUpper()
        }.join(",") + "\n";
        f.write(line.toUtf8());
    }
    f.close();
    statusBar()->showMessage(QStringLiteral("已导出 %1 条算子数据").arg(m_items.size()), 5000);
}

void MainWindow::showLegend()
{
    LegendDialog dlg(this);
    dlg.exec();
}

// ══════════════════════════════════════════════════════════════════
void MainWindow::setDirty(bool d)
{
    m_dirty = d;
    // 标题只保留 CounterForge；当前文件名在状态栏显示
    setWindowTitle(m_dirty ? QStringLiteral("CounterForge *") : QStringLiteral("CounterForge"));
}

bool MainWindow::maybeSave()
{
    if (!m_dirty) return true;
    const auto r = QMessageBox::question(
        this, QStringLiteral("未保存的更改"),
        QStringLiteral("当前项目有未保存的修改，要先保存吗？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (r == QMessageBox::Save) { saveProject(); return !m_dirty; }
    if (r == QMessageBox::Discard) return true;
    return false;
}

void MainWindow::updateStatus()
{
    if (m_items.isEmpty()) return;
    const cf::CounterSpec &s = m_items[m_current];
    m_status->setText(QStringLiteral("共 %1 枚 · 当前：%2  |  %3 · %4 · 战斗 %5 / 行动 %6 / 堆叠 %7")
                          .arg(m_items.size())
                          .arg(s.superior.trimmed().isEmpty() ? cf::branchName(s.branch) : s.superior,
                               cf::echelonName(s.echelon), cf::branchName(s.branch))
                          .arg(s.combat).arg(s.movement).arg(s.stacking));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (maybeSave()) e->accept();
    else e->ignore();
}
