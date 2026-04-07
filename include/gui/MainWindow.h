/**
 * Authors: Tomasz Okon, Jan Szwagierczak
 * Description: Header of the application's main window (Qt).
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>

#include "backend/PieceTable.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow( QWidget* parent = nullptr );
    ~MainWindow();

private:
    QLabel* status_label_;
    PieceTable piece_table_;
};

#endif
