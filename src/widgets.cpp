// widgets.cpp
#include "widgets.h"

#include <QApplication>
#include <QJsonObject>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>
#include <cmath>

#include "counterrenderer.h"

// ══════════════════════════════════════════════════════════════════
CounterPreview::CounterPreview(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
}

void CounterPreview::setSpec(const cf::CounterSpec &s)
{
    m_spec = s;
    update();
}

void CounterPreview::setShowHex(bool v)
{
    m_hex = v;
    update();
}

void CounterPreview::setShowShadow(bool v)
{
    m_shadow = v;
    update();
}

void CounterPreview::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = rect();

    // 作战室底色
    QLinearGradient bg(0, 0, 0, r.height());
    bg.setColorAt(0.0, QColor(0x37, 0x33, 0x2B));
    bg.setColorAt(0.55, QColor(0x28, 0x25, 0x1F));
    bg.setColorAt(1.0, QColor(0x16, 0x14, 0x0F));
    p.fillRect(r, bg);

    // 顶部暖光
    QRadialGradient glow(r.center().x(), r.height() * 0.30, r.width() * 0.70);
    glow.setColorAt(0.0, QColor(232, 198, 138, 42));
    glow.setColorAt(0.55, QColor(226, 190, 126, 14));
    glow.setColorAt(1.0, QColor(226, 190, 126, 0));
    p.fillRect(r, glow);

    // 细网格
    p.setPen(QPen(QColor(226, 206, 170, 17), 1.0));
    const int step = 30;
    for (int x = 0; x < int(r.width()); x += step)
        p.drawLine(x, 0, x, int(r.height()));
    for (int y = 0; y < int(r.height()); y += step)
        p.drawLine(0, y, int(r.width()), y);

    // 等高线，暗示这是一张军用地图
    p.setPen(QPen(QColor(226, 206, 170, 20), 1.0));
    for (int i = 0; i < 4; ++i) {
        QPainterPath line;
        const qreal y0 = r.height() * (0.22 + i * 0.19);
        line.moveTo(-30, y0);
        line.cubicTo(r.width() * 0.28, y0 - 34 - i * 9,
                     r.width() * 0.62, y0 + 26 + i * 7,
                     r.width() + 30, y0 - 12);
        p.drawPath(line);
    }
    p.setPen(QPen(QColor(226, 206, 170, 13), 1.0));
    for (int i = 0; i < 3; ++i) {
        QPainterPath line;
        const qreal y0 = r.height() * (0.30 + i * 0.19);
        line.moveTo(-30, y0);
        line.cubicTo(r.width() * 0.30, y0 - 22 - i * 7,
                     r.width() * 0.64, y0 + 18 + i * 5,
                     r.width() + 30, y0 - 6);
        p.drawPath(line);
    }

    // 纸纹，和算子同一张纸
    p.save();
    p.setOpacity(0.5);
    p.drawTiledPixmap(r, QPixmap::fromImage(cf::paperGrain()));
    p.restore();

    // 六角地图网格：平顶六角铺满整幅，像铺开的一张作战地图。
    // 以前是"单枚竖长的六边形套住算子"，那个外框比算子高 15%，
    // 眼睛会顺着外框把里面的方块也看成竖的——铺成整片网格就没这个问题。
    if (m_hex) {
        const qreal R = 52.0;                        // 外接圆半径
        const qreal gx = R * 1.5;                    // 列距
        const qreal gy = R * std::sqrt(3.0);         // 行距
        QPolygonF proto;
        for (int i = 0; i < 6; ++i) {
            const qreal a = M_PI / 180.0 * (60.0 * i);   // 顶点在左右 → 平顶
            proto << QPointF(R * std::cos(a), R * std::sin(a));
        }
        QPainterPath grid;
        for (int c = -1; c * gx < r.width() + gx; ++c) {
            const qreal ox = c * gx;
            const qreal oy = (c & 1) ? gy / 2.0 : 0.0;
            for (int row = -1; row * gy < r.height() + gy; ++row) {
                QPolygonF hex = proto;
                hex.translate(ox, oy + row * gy);
                grid.addPolygon(hex);
            }
        }
        p.setPen(QPen(QColor(232, 212, 176, 26), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawPath(grid);
    }

    // 算子：按可用区域（扣掉底部说明）居中，严格正方形
    const qreal side = qMin(r.width() * 0.58, (r.height() - 34) * 0.62);
    const qreal cyArea = (r.height() - 34) / 2.0;
    const QRectF box(r.center().x() - side / 2.0, cyArea - side / 2.0, side, side);

    if (m_hex && box.right() + 74 < r.width()) {
        QFont hf(QStringLiteral("Consolas"));
        hf.setPixelSize(12);
        p.setFont(hf);
        p.setPen(QColor(226, 208, 172, 86));
        p.drawText(QPointF(box.right() + 22, box.top() + side * 0.18), QStringLiteral("H14"));
        p.drawText(QPointF(box.right() + 22, box.top() + side * 0.62), QStringLiteral("H15"));
    }

    cf::RenderStyle st;
    st.shadow = m_shadow;
    cf::paintCounter(&p, box, m_spec, st);

    // 底部说明
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(12);
    p.setFont(f);
    p.setPen(QColor(196, 178, 148, 120));
    const QString info = QStringLiteral("边长 15.9 mm（5/8″）· 300 dpi 输出 188 px · 圆角 %1%")
                             .arg(m_spec.cornerPercent);
    p.drawText(QRectF(0, r.height() - 30, r.width(), 20), Qt::AlignCenter, info);
}

// ══════════════════════════════════════════════════════════════════
CounterItemDelegate::CounterItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize CounterItemDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return QSize(280, 74);
}

void CounterItemDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt,
                                const QModelIndex &index) const
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setRenderHint(QPainter::TextAntialiasing, true);

    const QRect r = opt.rect.adjusted(4, 3, -4, -3);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;

    QPainterPath path;
    path.addRoundedRect(r, 7, 7);
    if (selected) {
        p->fillPath(path, QColor(0x2C, 0x24, 0x18));
        p->setPen(QPen(QColor(0xC2, 0x90, 0x3F), 1.4));
        p->drawPath(path);
    } else if (hover) {
        p->fillPath(path, QColor(255, 240, 210, 12));
    }

    const cf::CounterSpec s = cf::CounterSpec::fromJson(index.data(Qt::UserRole).toJsonObject());

    const QRectF thumb(r.left() + 8, r.top() + 9, 56, 56);
    p->drawImage(thumb, cf::counterImage(s, 112));

    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    f.setWeight(QFont::DemiBold);
    p->setFont(f);
    p->setPen(selected ? QColor(0xF0, 0xE2, 0xC4) : QColor(0xDCD3C2));
    QString t1 = s.superior.trimmed().isEmpty() ? cf::branchName(s.branch) : s.superior;
    p->drawText(QRectF(r.left() + 74, r.top() + 7, r.width() - 84, 20),
                Qt::AlignLeft | Qt::AlignVCenter, t1);

    f.setPixelSize(10);
    f.setWeight(QFont::Normal);
    p->setFont(f);
    p->setPen(QColor(0x96, 0x88, 0x71));
    const QString t2 = QStringLiteral("%1 · %2 · %3-%4 · 堆叠%5")
                           .arg(cf::echelonName(s.echelon), cf::branchName(s.branch))
                           .arg(s.combat).arg(s.movement).arg(s.stacking);
    const QFontMetrics fm2(f);
    p->drawText(QRectF(r.left() + 74, r.top() + 27, r.width() - 92, 16),
                Qt::AlignLeft | Qt::AlignVCenter,
                fm2.elidedText(t2, Qt::ElideRight, int(r.width() - 96)));

    f.setPixelSize(9);
    p->setFont(f);
    p->setPen(QColor(0x7A, 0x6E, 0x5B));
    p->drawText(QRectF(r.left() + 74, r.top() + 45, r.width() - 84, 14),
                Qt::AlignLeft | Qt::AlignVCenter, cf::branchEnglish(s.branch));

    // 编制色小色块
    p->setPen(Qt::NoPen);
    p->setBrush(s.formationColor);
    p->drawRoundedRect(QRectF(r.right() - 26, r.bottom() - 18, 18, 8), 3, 3);

    p->restore();
}
