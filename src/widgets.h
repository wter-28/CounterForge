// widgets.h —— 预览画布与算子库委托
#pragma once

#include <QStyledItemDelegate>
#include <QWidget>

#include "counterspec.h"

class CounterPreview : public QWidget
{
    Q_OBJECT
public:
    explicit CounterPreview(QWidget *parent = nullptr);

    void setSpec(const cf::CounterSpec &s);
    void setShowHex(bool v);
    void setShowShadow(bool v);

    QSize sizeHint() const override { return QSize(520, 600); }
    QSize minimumSizeHint() const override { return QSize(320, 360); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    cf::CounterSpec m_spec;
    bool m_hex = true;
    bool m_shadow = true;
};

class CounterItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit CounterItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *p, const QStyleOptionViewItem &opt,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &index) const override;
};
