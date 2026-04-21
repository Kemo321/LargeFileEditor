/**
 * Authors: Jan Szwagierczak
 * Description: Header of the application's main window (Qt).
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QProgressBar>
#include <QString>

#include "gui/FindReplaceDialog.h"
#include "gui/LargeFileViewer.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructs the application's main window.
     * @param parent The parent widget.
     */
    explicit MainWindow( QWidget* parent = nullptr );
    ~MainWindow() override = default;

private:
    void openFile();
    void saveFile();
    void saveFileAs();
    void findText();
    void replaceText();

    static void setFontSizeSmall();
    static void setFontSizeMedium();
    static void setFontSizeLarge();

    void createActions();
    void createMenus();
    void createStatusBar();
    void updateWindowTitle();

    LargeFileViewer* viewer_;
    FindReplaceDialog* find_replace_dialog_{};

    // Status bar widgets
    QLabel* cursor_pos_label_{};
    QProgressBar* task_progress_bar_{};
    QLabel* task_status_label_{};

    // File Actions
    QAction* open_act_{};
    QAction* save_act_{};
    QAction* save_as_act_{};
    QAction* exit_act_{};

    // Edit Actions
    QAction* copy_act_{};
    QAction* cut_act_{};
    QAction* paste_act_{};
    QAction* find_act_{};
    QAction* replace_act_{};

    // View Actions
    QAction* font_small_act_{};
    QAction* font_medium_act_{};
    QAction* font_large_act_{};
    QActionGroup* font_size_group_{};

    // Mock State
    QString current_filename_;
};

#endif
