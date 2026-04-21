/**
 * Authors: Jan Szwagierczak
 * Description: Header of the application's main window (Qt).
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QDockWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>

#include "backend/PieceTable.h"
#include "gui/LargeFileViewer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow( QWidget* parent = nullptr );
    ~MainWindow() override;

private:
    void createActions();
    void createMenus();
    void createStatusBar();

    QLabel* status_label_{};
    PieceTable piece_table_;

    LargeFileViewer* viewer_;

    QAction* open_act_{};
    QAction* save_as_act_{};
    QAction* exit_act_{};
    QAction* find_act_{};
    QAction* replace_act_{};
};

#endif
