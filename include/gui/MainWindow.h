/**
 * @file MainWindow.h
 * @author Tomasz Okon, Jan Szwagierczak
 * @brief Header of the main application window integrating all components.
 */

#pragma once

#include <QAction>
#include <QActionGroup>
#include <QFutureWatcher>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QtConcurrent/QtConcurrent>
#include <memory>
#include <vector>

#include "backend/PieceTable.h"
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

    auto onSaveFinished() -> void;
    auto onFindFinished() -> void;
    auto onReplaceAllFinished() -> void;

    auto setFontSizeSmall() -> void;
    auto setFontSizeMedium() -> void;
    auto setFontSizeLarge() -> void;

    auto createActions() -> void;
    auto createMenus() -> void;
    auto createStatusBar() -> void;
    auto updateWindowTitle() -> void;
    auto processFindResults() -> void;
    static auto isBinaryFile( const QString& filePath ) -> bool;

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
    bool replace_canceled_{ false };

    QFutureWatcher<bool>* save_watcher_{};
    QFutureWatcher<std::vector<uint64_t>>* find_watcher_{};
    QFutureWatcher<uint64_t>* replace_watcher_{};
};
