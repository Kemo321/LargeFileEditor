/**
 * Authors: Jan Szwagierczak
 * Description: Header of the application's main window (Qt).
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
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
    /**
     * @brief Opens a native file dialog and selects a file.
     */
    void openFile();

    /**
     * @brief Mocks saving the current file.
     */
    void saveFile();

    /**
     * @brief Opens a native file dialog to save the current file as a new file.
     */
    void saveFileAs();

    /**
     * @brief Shows the non-modal Find dialog.
     */
    void findText();

    /**
     * @brief Shows the non-modal Replace dialog.
     */
    void replaceText();

    /**
     * @brief Shows the font size dialog.
     */
    void showFontDialog();

    /**
     * @brief Mocks an unsaved document state.
     */
    void setModifiedMock();

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
    QAction* font_act_{};

    // Mock State
    QString current_filename_;
};

#endif
