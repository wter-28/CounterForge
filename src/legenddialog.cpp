// legenddialog.cpp
#include "legenddialog.h"

#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

#include "counterrenderer.h"

static const int kCanvasW = 880;
static const int kCanvasH = 1046;

// ══════════════════════════════════════════════════════════════════
class LegendCanvas : public QWidget
{
public:
    explicit LegendCanvas(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(kCanvasW, kCanvasH);
        setObjectName(QStringLiteral("LegendCanvas"));
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        p.fillRect(rect(), QColor(0x14, 0x11, 0x0C));

        const QColor title(0xD2, 0xA2, 0x4A);
        const QColor label(0xC6, 0xBA, 0xA2);
        const QColor dim(0x7E, 0x71, 0x5C);
        const QColor ink(240, 234, 222, 240);

        QFont f(QStringLiteral("Microsoft YaHei UI"));
        f.setPixelSize(19);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(title);
        p.drawText(QPointF(34, 40), QStringLiteral("北约军兵种符号"));
        f.setPixelSize(11);
        f.setWeight(QFont::Normal);
        p.setFont(f);
        p.setPen(dim);
        p.drawText(QPointF(34, 60), QStringLiteral("NATO APP-6 风格 · 算子上的兵种识别符号"));

        // ── 兵种符号表
        const int cols = 5;
        const qreal cellW = 160, cellH = 86;
        const qreal x0 = 30, y0 = 76;

        for (int i = 0; i < int(cf::Branch::Count); ++i) {
            const cf::Branch b = cf::Branch(i);
            if (b == cf::Branch::Supply) continue;   // 补给另有版式，见底部说明
            const int c = i % cols;
            const int r = i / cols;
            const qreal x = x0 + c * cellW;
            const qreal y = y0 + r * cellH;

            p.setPen(QPen(QColor(232, 214, 180, 16), 1));
            p.setBrush(QColor(232, 214, 180, 8));
            p.drawRoundedRect(QRectF(x + 2, y + 2, cellW - 8, cellH - 8), 7, 7);

            cf::paintNatoSymbol(&p, QRectF(x + 47, y + 9, 62, 43), ink, b);

            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(12);
            p.setFont(lf);
            p.setPen(label);
            p.drawText(QRectF(x + 2, y + 54, cellW - 8, 16), Qt::AlignHCenter, cf::branchName(b));
            lf.setPixelSize(8);
            p.setFont(lf);
            p.setPen(dim);
            p.drawText(QRectF(x + 2, y + 68, cellW - 8, 12), Qt::AlignHCenter, cf::branchEnglish(b));
        }

        // ── 规模符号：北约梯队标记
        const qreal yE = y0 + 4 * cellH + 18;
        f.setPixelSize(15);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(title);
        p.drawText(QPointF(34, yE + 6), QStringLiteral("规模符号（北约梯队标记）"));

        const qreal yG = yE + 18;
        const int perRow = 7;
        for (int i = 1; i < int(cf::Echelon::Count); ++i) {
            const cf::Echelon e = cf::Echelon(i);
            const int idx = i - 1;
            const qreal x = 34 + (idx % perRow) * 116;
            const qreal y = yG + (idx / perRow) * 70;

            p.setPen(QPen(QColor(232, 214, 180, 16), 1));
            p.setBrush(QColor(232, 214, 180, 8));
            p.drawRoundedRect(QRectF(x, y, 108, 62), 6, 6);

            cf::paintEchelon(&p, QPointF(x + 54, y + 22), 26, ink, e);

            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(10);
            p.setFont(lf);
            p.setPen(label);
            p.drawText(QRectF(x, y + 40, 108, 18), Qt::AlignHCenter | Qt::AlignTop,
                       cf::echelonName(e));
        }

        // ── 堆叠圈（军标正左边，两级）
        const qreal yS = yG + 2 * 70 + 12;
        f.setPixelSize(15);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(title);
        p.drawText(QPointF(34, yS + 4), QStringLiteral("堆叠圈（军标正左边）：空圈 = 1，实心圈 = 2"));

        const qreal yC = yS + 16;
        for (int v = 1; v <= 2; ++v) {
            const qreal x = 34 + (v - 1) * 176;

            p.setPen(QPen(QColor(232, 214, 180, 16), 1));
            p.setBrush(QColor(232, 214, 180, 8));
            p.drawRoundedRect(QRectF(x, yC, 164, 58), 6, 6);

            cf::paintStackMark(&p, QPointF(x + 32, yC + 29), 30, ink, v);

            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(12);
            p.setFont(lf);
            p.setPen(label);
            p.drawText(QRectF(x + 58, yC + 9, 100, 20), Qt::AlignLeft | Qt::AlignVCenter,
                       cf::stackName(v));

            lf.setPixelSize(10);
            p.setFont(lf);
            p.setPen(dim);
            p.drawText(QRectF(x + 58, yC + 29, 100, 20), Qt::AlignLeft | Qt::AlignVCenter,
                       QStringLiteral("格内占 %1 点").arg(v));
        }

        // ── 版面说明
        const qreal yA = yC + 58 + 30;
        f.setPixelSize(15);
        f.setWeight(QFont::Bold);
        p.setFont(f);
        p.setPen(title);
        p.drawText(QPointF(34, yA), QStringLiteral("算子上各元素的含义"));

        cf::CounterSpec demo;
        demo.superior = QStringLiteral("72A.K");
        demo.designation = QStringLiteral("27");
        demo.showDesignation = true;
        demo.mark = QString::fromUtf8("★");
        demo.echelon = cf::Echelon::Division;
        demo.stacking = 2;
        demo.branch = cf::Branch::Infantry;
        demo.combat = 4;
        demo.movement = 6;
        demo.formationColor = cf::formationPreset(5);
        demo.silhouette = cf::Silhouette::None;

        const QRectF cbox(316, yA + 24, 178, 178);
        cf::RenderStyle st;
        st.shadow = true;
        cf::paintCounter(&p, cbox, demo, st);

        auto U = [&](qreal u) { return cbox.left() + u * cbox.width() / 1000.0; };
        auto V = [&](qreal v) { return cbox.top() + v * cbox.height() / 1000.0; };

        auto callout = [&](const QPointF &from, const QPointF &to, const QString &text, bool leftSide) {
            p.setPen(QPen(QColor(0xD2, 0xA2, 0x4A, 150), 1.0));
            p.drawLine(from, to);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0xD2, 0xA2, 0x4A));
            p.drawEllipse(from, 2.0, 2.0);

            QFont lf(QStringLiteral("Microsoft YaHei UI"));
            lf.setPixelSize(12);
            p.setFont(lf);
            p.setPen(label);
            const QRectF tr = leftSide ? QRectF(to.x() - 168, to.y() - 9, 164, 18)
                                       : QRectF(to.x() + 4, to.y() - 9, 172, 18);
            p.drawText(tr, int(leftSide ? (Qt::AlignRight | Qt::AlignVCenter)
                                        : (Qt::AlignLeft | Qt::AlignVCenter)), text);
        };

        // 左侧
        callout(QPointF(U(300), V(120)), QPointF(240, V(120)), QStringLiteral("上级（番号代码）"), true);
        callout(QPointF(U(500), V(268)), QPointF(240, V(300)), QStringLiteral("规模符号"), true);
        callout(QPointF(U(252), V(452)), QPointF(240, V(452)),
                QStringLiteral("堆叠圈（空 1 / 实 2）"), true);
        callout(QPointF(U(500), V(540)), QPointF(240, V(620)), QStringLiteral("兵种符号"), true);
        callout(QPointF(U(300), V(830)), QPointF(240, V(830)), QStringLiteral("战斗力"), true);

        // 右侧
        callout(QPointF(U(930), V(430)), QPointF(cbox.right() + 22, V(390)),
                QStringLiteral("番号（右侧竖排）"), false);
        callout(QPointF(U(760), V(700)), QPointF(cbox.right() + 22, V(640)),
                QStringLiteral("编制色"), false);
        callout(QPointF(U(700), V(800)), QPointF(cbox.right() + 22, V(800)),
                QStringLiteral("行动力"), false);

        // 左侧小标记单独提一句
        QFont nf(QStringLiteral("Microsoft YaHei UI"));
        nf.setPixelSize(11);
        p.setFont(nf);
        p.setPen(dim);
        p.drawText(QRectF(34, cbox.bottom() + 26, kCanvasW - 68, 56),
                   Qt::AlignLeft | Qt::TextWordWrap,
                   QStringLiteral("「左标记」可填任意 1–2 个字符（○ / ★ / A），用来标注炮兵支援、可激活等。"
                                  "军标左边的小圈是堆叠值：空圈占 1 点、实心圈占 2 点，不印数字。"
                                  "补给队与其它算子同一版式，只是中间换成卡车标记、标注「补给 SUPPLY」，下部只显示一个移动力。"));
    }
};

// ══════════════════════════════════════════════════════════════════
LegendDialog::LegendDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("符号图例 · 版面说明"));
    setStyleSheet(QStringLiteral("QDialog{background:#14110C;}"
                                 "QScrollArea{background:#14110C;border:none;}"
                                 "QScrollBar:vertical{background:transparent;width:10px;}"
                                 "QScrollBar::handle:vertical{background:#3A3024;border-radius:5px;min-height:30px;}"
                                 "QScrollBar::add-line,QScrollBar::sub-line{height:0;}"));

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_canvas = new LegendCanvas;
    m_scroll->setWidget(m_canvas);

    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->addWidget(m_scroll);

    const QRect avail = screen() ? screen()->availableGeometry() : QRect(0, 0, 1280, 800);
    const int w = qMin(kCanvasW + 16, avail.width() - 80);
    const int h = qMin(kCanvasH + 16, avail.height() - 80);
    resize(w, h);
    setMinimumSize(qMin(700, w), qMin(480, h));
}
