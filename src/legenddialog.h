// legenddialog.h —— 符号图例 / 版面说明
#pragma once

#include <QDialog>

class QScrollArea;

class LegendCanvas;

class LegendDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LegendDialog(QWidget *parent = nullptr);

private:
    LegendCanvas *m_canvas = nullptr;
    QScrollArea  *m_scroll = nullptr;
};
