/**
 * @file MainWindow.h
 * @author Tomasz Okon, Jan Szwagierczak
 * @brief Header of the main application window integrating all components.
 */

#pragma once

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

#include "backend/PieceTable.h"
#include "gui/BackgroundTaskManager.h"
#include "gui/FindReplaceDialog.h"
#include "gui/LargeFileViewer.h"

/**
 * @class MainWindow
 * @brief Application's main window orchestrating UI layout and backend events.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Instantiates the main user interface.
     * @param parent Pointer to parent QWidget.
     */
    explicit MainWindow( QWidget* parent = nullptr );
    ~MainWindow() override;

protected:
    /// Prompts to save unsaved changes (and defers close during a running save).
    auto closeEvent( QCloseEvent* event ) -> void override;

private:
    auto openFile() -> void;
    auto saveFile() -> void;
    auto saveFileAs() -> void;
    auto findText() -> void;
    auto replaceText() -> void;

    auto onFindNextRequested( const QString& text, bool matchCase, bool matchWord ) -> void;
    auto onReplaceNextRequested( const QString& findText, const QString& replaceText,
                                 bool matchCase, bool matchWord ) -> void;
    auto onReplaceAllRequested( const QString& findText, const QString& replaceText, bool matchCase,
                                bool matchWord ) -> void;

    auto onSaveFinished( bool success ) -> void;
    auto onFindFinished( std::vector<uint64_t> results ) -> void;
    auto onReplaceAllFinished( uint64_t replacedCount, bool canceled ) -> void;

    auto setFontSizeSmall() -> void;
    auto setFontSizeMedium() -> void;
    auto setFontSizeLarge() -> void;

    auto createActions() -> void;
    auto createMenus() -> void;
    auto createStatusBar() -> void;
    auto updateWindowTitle() -> void;
    auto processFindResults() -> void;

    auto undoText() -> void;
    auto redoText() -> void;
    // Syncs undo_act_/redo_act_ enabled state with backend history (disabled when no document).
    auto updateUndoRedoState() -> void;

    // Defers window close until a running save completes; re-issued from onSaveFinished().
    auto beginCloseWait( QCloseEvent* event ) -> void;

    // Completes a deferred close: re-issues close() on save success, restores the UI on failure.
    auto finalizePendingClose() -> void;

    LargeFileViewer* viewer_;
    FindReplaceDialog* find_replace_dialog_{};
    std::unique_ptr<PieceTable> piece_table_;

    QLabel* cursor_pos_label_{};
    QProgressBar* task_progress_bar_{};
    QLabel* task_status_label_{};
    QPushButton* cancel_task_btn_{};

    QAction* open_act_{};
    QAction* save_act_{};
    QAction* save_as_act_{};
    QAction* exit_act_{};

    QAction* undo_act_{};
    QAction* redo_act_{};

    QAction* find_act_{};
    QAction* replace_act_{};

    QAction* font_small_act_{};
    QAction* font_medium_act_{};
    QAction* font_large_act_{};
    QActionGroup* font_size_group_{};

    QString current_filename_;
    QString pending_temp_filename_;

    std::vector<uint64_t> current_find_results_;
    QString current_find_text_;
    int current_find_index_{ -1 };
    bool current_match_case_{ true };
    bool current_match_word_{ false };

    BackgroundTaskManager* tasks_{};

    bool close_after_save_{ false };
};
