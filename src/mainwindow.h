// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QVector>

#include "counterspec.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QSlider;
class QCheckBox;
class QRadioButton;
class QListWidget;
class QLabel;
class CounterPreview;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // 打开项目文件（也用于命令行/拖拽传入的路径）
    bool openPath(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *e) override;

private slots:
    void syncFromUi();
    void selectIndex(int row);
    void addCounter();
    void duplicateCounter();
    void deleteCounter();
    void moveCounter(int delta);
    void newProject();
    void openProject();
    void saveProject();
    void saveProjectAs();
    void exportPng();
    void exportSheet();
    void exportCsv();
    void showLegend();
    void chooseColor();
    void chooseTopColor();
    void onBranchChanged();

private:
    void buildUi();
    QWidget *buildEditor();
    QWidget *buildLibrary();
    void applyStyle();
    void loadToUi(const cf::CounterSpec &s);
    void rebuildList(int selectRow);
    void refreshPreview();
    void refreshRow(int row);
    void updateColorUi();
    void setDirty(bool d);
    bool maybeSave();
    void commit(const cf::CounterSpec &s);
    void updateStatus();

    QVector<cf::CounterSpec> m_items;
    int      m_current  = 0;
    QString  m_path;
    bool     m_dirty    = false;
    bool     m_updating = false;

    QLineEdit   *m_superior = nullptr;
    QLineEdit   *m_designation = nullptr;
    QLineEdit   *m_mark = nullptr;
    QComboBox   *m_echelon = nullptr;
    QComboBox   *m_branch = nullptr;
    QSpinBox    *m_combat = nullptr;
    QSpinBox    *m_movement = nullptr;
    QSpinBox    *m_stacking = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_topColorBtn = nullptr;
    QRadioButton *m_autoTop = nullptr;
    QRadioButton *m_manualTop = nullptr;
    QSlider     *m_radius = nullptr;
    QLabel      *m_radiusVal = nullptr;
    QCheckBox   *m_showEchelon = nullptr;
    QCheckBox   *m_showStackMark = nullptr;
    QCheckBox   *m_showDesignation = nullptr;
    QCheckBox   *m_paperBorder = nullptr;
    QCheckBox   *m_hexToggle = nullptr;
    CounterPreview *m_preview = nullptr;
    QListWidget *m_library = nullptr;
    QLabel      *m_status = nullptr;
};
