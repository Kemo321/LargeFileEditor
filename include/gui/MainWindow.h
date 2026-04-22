#pragma once

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QProgressBar>
#include <QString>
#include <memory>
#include <vector>

#include "gui/FindReplaceDialog.h"
#include "gui/LargeFileViewer.h"
#include "backend/PieceTable.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow( QWidget* parent = nullptr );
    ~MainWindow() override = default;

private slots:
    void openFile();
    void saveFile();
    void saveFileAs();
    void findText();
    void replaceText();

    void onFindNextRequested( const QString& text, bool matchCase, bool matchWord );
    void onReplaceNextRequested( const QString& findText, const QString& replaceText, bool matchCase, bool matchWord );
    void onReplaceAllRequested( const QString& findText, const QString& replaceText, bool matchCase, bool matchWord );

    void setFontSizeSmall();
    void setFontSizeMedium();
    void setFontSizeLarge();

private:
    void createActions();
    void createMenus();
    void createStatusBar();
    void updateWindowTitle();

    LargeFileViewer* viewer_;
    FindReplaceDialog* find_replace_dialog_{};
    std::unique_ptr<PieceTable> piece_table_;

    QLabel* cursor_pos_label_{};
    QProgressBar* task_progress_bar_{};
    QLabel* task_status_label_{};

    QAction* open_act_{};
    QAction* save_act_{};
    QAction* save_as_act_{};
    QAction* exit_act_{};

    QAction* copy_act_{};
    QAction* cut_act_{};
    QAction* paste_act_{};
    QAction* find_act_{};
    QAction* replace_act_{};

    QAction* font_small_act_{};
    QAction* font_medium_act_{};
    QAction* font_large_act_{};
    QActionGroup* font_size_group_{};

    QString current_filename_;

    std::vector<uint64_t> current_find_results_;
    QString current_find_text_;
    int current_find_index_{ -1 };
};