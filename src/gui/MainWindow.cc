// Author: Tomasz Okon, Jan Szwagierczak

#include "gui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>
#include <optional>
#include <string>
#include <utility>

#include "util/FileUtils.h"

static constexpr int kDefaultWindowWidth = 800;
static constexpr int kDefaultWindowHeight = 600;
static constexpr int kProgressBarWidth = 200;
static constexpr int kProgressBarHeight = 14;
static constexpr int kFontSizeSmall = 8;
static constexpr int kFontSizeMedium = 11;
static constexpr int kFontSizeLarge = 14;
static constexpr int kStatusClearDelayMs = 2000;

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent ), current_filename_( "" )
{
    viewer_ = new LargeFileViewer( this );
    viewer_->setPieceTable( nullptr );
    viewer_->setEnabled( false );
    setCentralWidget( viewer_ );

    find_replace_dialog_ = new FindReplaceDialog( this );

    connect( find_replace_dialog_, &FindReplaceDialog::findNextRequested, this,
             &MainWindow::onFindNextRequested );
    connect( find_replace_dialog_, &FindReplaceDialog::replaceNextRequested, this,
             &MainWindow::onReplaceNextRequested );
    connect( find_replace_dialog_, &FindReplaceDialog::replaceAllRequested, this,
             &MainWindow::onReplaceAllRequested );
    connect( find_replace_dialog_, &FindReplaceDialog::dialogClosed, this, [this]() {
        if( tasks_->isReplaceRunning() ) {
            tasks_->cancelReplace();
        }
        current_find_results_.clear();
        current_find_index_ = -1;
        current_find_text_ = "";
        current_match_case_ = true;
        current_match_word_ = false;
        viewer_->setEnabled( true );
        viewer_->setSearchHighlights( {}, -1, 0 );
        task_progress_bar_->hide();
        task_status_label_->setText( "Gotowy" );
    } );

    createActions();
    createMenus();
    createStatusBar();

    tasks_ = new BackgroundTaskManager( this );
    connect( tasks_, &BackgroundTaskManager::saveFinished, this, &MainWindow::onSaveFinished );
    connect( tasks_, &BackgroundTaskManager::findFinished, this, &MainWindow::onFindFinished );
    connect( tasks_, &BackgroundTaskManager::replaceProgress, task_progress_bar_,
             &QProgressBar::setValue );
    connect( tasks_, &BackgroundTaskManager::replaceFinished, this,
             &MainWindow::onReplaceAllFinished );

    connect( cancel_task_btn_, &QPushButton::clicked, this, [this]() {
        if( tasks_->isReplaceRunning() ) {
            tasks_->cancelReplace();
            task_status_label_->setText( "Anulowanie..." );
        }
    } );

    connect( viewer_, &LargeFileViewer::cursorPositionChanged, this, [this]( int line, int col ) {
        cursor_pos_label_->setText( QString( "Wiersz %1, Kol %2" ).arg( line + 1 ).arg( col + 1 ) );
    } );

    connect( viewer_, &LargeFileViewer::documentModified, this, [this]() {
        setWindowModified( true );
        updateUndoRedoState();
    } );

    setWindowModified( false );
    updateWindowTitle();
    resize( kDefaultWindowWidth, kDefaultWindowHeight );
}

MainWindow::~MainWindow()
{
    tasks_->waitForReplace();
    tasks_->waitForSave();
}

auto MainWindow::closeEvent( QCloseEvent* event ) -> void
{
    // A running save must not block the GUI thread; defer the close until onSaveFinished().
    if( tasks_->isSaveRunning() ) {
        beginCloseWait( event );
        return;
    }

    if( !isWindowModified() ) {
        event->accept();
        return;
    }

    QMessageBox msgBox( this );
    msgBox.setIcon( QMessageBox::Warning );
    msgBox.setWindowTitle( "Niezapisane zmiany" );
    msgBox.setText( "Dokument został zmodyfikowany. Czy chcesz zapisać zmiany przed zamknięciem?" );
    msgBox.setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );

    QPushButton* saveButton = msgBox.addButton( "Zapisz", QMessageBox::AcceptRole );
    QPushButton* discardButton = msgBox.addButton( "Nie zapisuj", QMessageBox::DestructiveRole );
    QPushButton* cancelButton = msgBox.addButton( "Anuluj", QMessageBox::RejectRole );

    msgBox.exec();

    if( msgBox.clickedButton() == saveButton ) {
        saveFile();
        if( tasks_->isSaveRunning() ) {
            beginCloseWait( event );
        } else {
            event->accept();
        }
    } else if( msgBox.clickedButton() == cancelButton ) {
        event->ignore();
    } else if( msgBox.clickedButton() == discardButton ) {
        event->accept();
    }
}

auto MainWindow::beginCloseWait( QCloseEvent* event ) -> void
{
    event->ignore();
    close_after_save_ = true;
    viewer_->setEnabled( false );
    menuBar()->setEnabled( false );
    task_status_label_->setText( "Zamykanie, oczekiwanie na zakończenie zapisu..." );
}

auto MainWindow::createActions() -> void
{
    open_act_ = new QAction( "&Otwórz...", this );
    open_act_->setShortcuts( QKeySequence::Open );
    connect( open_act_, &QAction::triggered, this, &MainWindow::openFile );

    save_act_ = new QAction( "&Zapisz", this );
    save_act_->setShortcuts( QKeySequence::Save );
    connect( save_act_, &QAction::triggered, this, &MainWindow::saveFile );

    save_as_act_ = new QAction( "Zapisz &jako...", this );
    save_as_act_->setShortcuts( QKeySequence::SaveAs );
    connect( save_as_act_, &QAction::triggered, this, &MainWindow::saveFileAs );

    exit_act_ = new QAction( "Za&kończ", this );
    exit_act_->setShortcuts( QKeySequence::Quit );
    connect( exit_act_, &QAction::triggered, qApp, &QApplication::quit );

    undo_act_ = new QAction( "&Cofnij", this );
    undo_act_->setShortcuts( QKeySequence::Undo );
    undo_act_->setEnabled( false );
    connect( undo_act_, &QAction::triggered, this, &MainWindow::undoText );

    redo_act_ = new QAction( "&Ponów", this );
    redo_act_->setShortcuts( QKeySequence::Redo );
    redo_act_->setEnabled( false );
    connect( redo_act_, &QAction::triggered, this, &MainWindow::redoText );

    find_act_ = new QAction( "&Znajdź...", this );
    find_act_->setShortcuts( QKeySequence::Find );
    connect( find_act_, &QAction::triggered, this, &MainWindow::findText );

    replace_act_ = new QAction( "Za&mień...", this );
    replace_act_->setShortcuts( QKeySequence::Replace );
    connect( replace_act_, &QAction::triggered, this, &MainWindow::replaceText );

    font_small_act_ = new QAction( "Mała", this );
    font_medium_act_ = new QAction( "Średnia", this );
    font_large_act_ = new QAction( "Duża", this );

    font_small_act_->setCheckable( true );
    font_medium_act_->setCheckable( true );
    font_large_act_->setCheckable( true );

    connect( font_small_act_, &QAction::triggered, this, &MainWindow::setFontSizeSmall );
    connect( font_medium_act_, &QAction::triggered, this, &MainWindow::setFontSizeMedium );
    connect( font_large_act_, &QAction::triggered, this, &MainWindow::setFontSizeLarge );

    font_size_group_ = new QActionGroup( this );
    font_size_group_->addAction( font_small_act_ );
    font_size_group_->addAction( font_medium_act_ );
    font_size_group_->addAction( font_large_act_ );
    font_size_group_->setExclusive( true );
    font_medium_act_->setChecked( true );
}

auto MainWindow::createMenus() -> void
{
    QMenu* fileMenu = menuBar()->addMenu( "&Plik" );
    fileMenu->addAction( open_act_ );
    fileMenu->addAction( save_act_ );
    fileMenu->addAction( save_as_act_ );
    fileMenu->addSeparator();
    fileMenu->addAction( exit_act_ );

    QMenu* editMenu = menuBar()->addMenu( "&Edycja" );
    editMenu->addAction( undo_act_ );
    editMenu->addAction( redo_act_ );
    editMenu->addSeparator();
    editMenu->addAction( find_act_ );
    editMenu->addAction( replace_act_ );

    QMenu* viewMenu = menuBar()->addMenu( "&Widok" );
    QMenu* fontSizeMenu = viewMenu->addMenu( "Rozmiar czcionki" );
    fontSizeMenu->addAction( font_small_act_ );
    fontSizeMenu->addAction( font_medium_act_ );
    fontSizeMenu->addAction( font_large_act_ );
}

auto MainWindow::createStatusBar() -> void
{
    task_progress_bar_ = new QProgressBar( this );
    task_progress_bar_->setMaximumWidth( kProgressBarWidth );
    task_progress_bar_->setMaximumHeight( kProgressBarHeight );
    task_progress_bar_->hide();

    cancel_task_btn_ = new QPushButton( "Anuluj", this );
    cancel_task_btn_->hide();

    task_status_label_ = new QLabel( "Gotowy", this );

    statusBar()->addWidget( task_status_label_ );
    statusBar()->addWidget( task_progress_bar_ );
    statusBar()->addWidget( cancel_task_btn_ );

    cursor_pos_label_ = new QLabel( "Wiersz 1, Kol 1", this );
    statusBar()->addPermanentWidget( cursor_pos_label_ );
}

auto MainWindow::updateWindowTitle() -> void
{
    if( current_filename_.isEmpty() ) {
        setWindowTitle( "LargeFileEditor" );
    } else {
        setWindowFilePath( current_filename_ );
        QString displayName = QFileInfo( current_filename_ ).fileName();
        setWindowTitle( QString( "[*]%1 - LargeFileEditor" ).arg( displayName ) );
    }
}

auto MainWindow::undoText() -> void
{
    if( !piece_table_ ) {
        return;
    }
    if( std::optional<uint64_t> restoredOffset = piece_table_->undo() ) {
        viewer_->refreshView();
        // Recenter on the reverted edit so the change is never off-screen.
        viewer_->jumpToLogicalPosition( *restoredOffset );
        setWindowModified( piece_table_->isDirty() );
        updateUndoRedoState();
        task_status_label_->setText( "Cofanie zakończone pomyślnie" );
    }
}

auto MainWindow::redoText() -> void
{
    if( !piece_table_ ) {
        return;
    }
    if( std::optional<uint64_t> restoredOffset = piece_table_->redo() ) {
        viewer_->refreshView();
        viewer_->jumpToLogicalPosition( *restoredOffset );
        setWindowModified( piece_table_->isDirty() );
        updateUndoRedoState();
        task_status_label_->setText( "Ponawianie zakończone pomyślnie" );
    }
}

auto MainWindow::updateUndoRedoState() -> void
{
    if( piece_table_ ) {
        undo_act_->setEnabled( piece_table_->canUndo() );
        redo_act_->setEnabled( piece_table_->canRedo() );
    } else {
        undo_act_->setEnabled( false );
        redo_act_->setEnabled( false );
    }
}

auto MainWindow::setFontSizeSmall() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeSmall ) );
    task_status_label_->setText( "Ustawiono małą czcionkę" );
}

auto MainWindow::setFontSizeMedium() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeMedium ) );
    task_status_label_->setText( "Ustawiono średnią czcionkę" );
}

auto MainWindow::setFontSizeLarge() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeLarge ) );
    task_status_label_->setText( "Ustawiono dużą czcionkę" );
}

auto MainWindow::openFile() -> void
{
    if( tasks_->isReplaceRunning() ) {
        return;
    }
    QString fileName =
        QFileDialog::getOpenFileName( this, "Otwórz plik", "", "Wszystkie pliki (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Otwieranie pliku..." );
        task_progress_bar_->show();
        task_progress_bar_->setRange( 0, 0 );

        if( FileUtils::isBinaryFile( fileName ) ) {
            task_progress_bar_->hide();
            task_status_label_->setText( "Nieobsługiwany typ pliku: binarny" );
            QMessageBox::warning( this, "Nieobsługiwany plik",
                                  "Nieobsługiwany typ pliku: pliki binarne nie są obsługiwane." );
            current_filename_ = "";
            current_find_text_ = "";
            current_find_index_ = -1;
            current_find_results_.clear();
            current_match_case_ = true;
            current_match_word_ = false;
            piece_table_ = nullptr;
            viewer_->setPieceTable( nullptr );
            viewer_->setEnabled( false );
            setWindowModified( false );
            updateWindowTitle();
            updateUndoRedoState();
            return;
        }

        current_filename_ = fileName;
        current_find_text_ = "";
        current_find_index_ = -1;
        current_find_results_.clear();
        current_match_case_ = true;
        current_match_word_ = false;

        try {
            piece_table_ = std::make_unique<PieceTable>( fileName.toStdString() );
            viewer_->setPieceTable( piece_table_.get() );
            viewer_->setEnabled( true );
            viewer_->setMockHighlights( QStringList{} );
            setWindowModified( false );
            updateWindowTitle();
            task_status_label_->setText(
                QString( "Wczytano: %1" ).arg( QFileInfo( fileName ).fileName() ) );
            updateUndoRedoState();
        } catch( ... ) {
            task_status_label_->setText( "Błąd: nie udało się otworzyć pliku" );
            QMessageBox::critical( this, "Błąd", "Nie można otworzyć pliku." );
        }

        task_progress_bar_->hide();
    }
}

auto MainWindow::saveFile() -> void
{
    if( current_filename_.isEmpty() ) {
        return;
    }

    if( !piece_table_ || tasks_->isSaveRunning() || tasks_->isReplaceRunning() ) {
        return;
    }

    viewer_->setEnabled( false );
    save_act_->setEnabled( false );
    open_act_->setEnabled( false );

    task_progress_bar_->show();
    task_progress_bar_->setRange( 0, 0 );
    task_status_label_->setText( "Zapisywanie w tle..." );

    pending_temp_filename_ = current_filename_ + ".tmp";

    tasks_->startSave( piece_table_.get(), pending_temp_filename_ );
}

auto MainWindow::onSaveFinished( bool success ) -> void
{
    task_progress_bar_->hide();

    viewer_->setEnabled( true );
    save_act_->setEnabled( true );
    open_act_->setEnabled( true );

    if( success ) {
        QString backup_filename = current_filename_ + ".bak";
        QFile::remove( backup_filename );
        if( QFile::exists( current_filename_ ) ) {
            QFile::rename( current_filename_, backup_filename );
        }

        if( !QFile::rename( pending_temp_filename_, current_filename_ ) ) {
            QFile::rename( backup_filename, current_filename_ );
            task_status_label_->setText( "Błąd zapisu: zmiana nazwy nie powiodła się" );
            QMessageBox::critical(
                this, "Błąd zapisu",
                "Nie można zapisać pliku: zmiana nazwy pliku tymczasowego nie powiodła się." );
            finalizePendingClose();
            return;
        }

        QFile::remove( backup_filename );

        int currentScroll = viewer_->verticalScrollBar()->value();
        piece_table_ = std::make_unique<PieceTable>( current_filename_.toStdString() );
        viewer_->setPieceTable( piece_table_.get() );
        viewer_->verticalScrollBar()->setValue( currentScroll );

        updateUndoRedoState();
        setWindowModified( false );
        task_status_label_->setText( "Plik zapisany pomyślnie" );
    } else {
        task_status_label_->setText( "Krytyczny: zapis nie powiódł się" );
        QMessageBox::critical(
            this, "Błąd zapisu",
            "Nie można zapisać pliku: zapis tablicy fragmentów nie powiódł się." );
    }

    QTimer::singleShot( kStatusClearDelayMs, this, [this]() {
        if( task_status_label_->text().contains( "zapisany pomyślnie" ) ) {
            task_status_label_->setText( "Gotowy" );
        }
    } );

    finalizePendingClose();
}

auto MainWindow::finalizePendingClose() -> void
{
    if( !close_after_save_ ) {
        return;
    }

    if( isWindowModified() ) {
        // Save failed: abandon the close and restore the UI.
        close_after_save_ = false;
        menuBar()->setEnabled( true );
        return;
    }

    close();  // re-enters closeEvent, which now accepts
}

auto MainWindow::saveFileAs() -> void
{
    if( !piece_table_ || tasks_->isReplaceRunning() ) {
        return;
    }

    QString fileName =
        QFileDialog::getSaveFileName( this, "Zapisz plik jako", "", "Wszystkie pliki (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Zapisywanie jako..." );
        if( piece_table_->saveToFile( fileName.toStdString() ) ) {
            tasks_->waitForSave();

            current_filename_ = fileName;
            setWindowModified( false );
            updateWindowTitle();
            task_status_label_->setText( "Plik zapisany jako: " +
                                         QFileInfo( fileName ).fileName() );
        } else {
            task_status_label_->setText( "Zapisywanie jako nie powiodło się!" );
            QMessageBox::critical( this, "Błąd zapisu",
                                   "Nie można zapisać pliku: zapis w backendzie nie powiódł się." );
        }
    }
}

auto MainWindow::findText() -> void
{
    find_replace_dialog_->showFind();
    task_status_label_->setText( "Otwarto okno wyszukiwania" );
}

auto MainWindow::replaceText() -> void
{
    find_replace_dialog_->showReplace();
    task_status_label_->setText( "Otwarto okno zamiany" );
}

auto MainWindow::onFindNextRequested( const QString& text, bool matchCase, bool matchWord ) -> void
{
    if( !piece_table_ || text.isEmpty() || tasks_->isFindRunning() ) {
        return;
    }

    if( text != current_find_text_ || matchCase != current_match_case_ ||
        matchWord != current_match_word_ ) {
        current_find_text_ = text;
        current_match_case_ = matchCase;
        current_match_word_ = matchWord;

        task_progress_bar_->show();
        task_progress_bar_->setRange( 0, 0 );
        task_status_label_->setText( "Wyszukiwanie..." );

        viewer_->setEnabled( false );
        find_replace_dialog_->setFindInProgress( true );

        tasks_->startFind( piece_table_.get(), text, matchCase, matchWord );
    } else {
        processFindResults();
    }
}

auto MainWindow::onFindFinished( std::vector<uint64_t> results ) -> void
{
    task_progress_bar_->hide();
    viewer_->setEnabled( true );
    find_replace_dialog_->setFindInProgress( false );
    current_find_results_ = std::move( results );
    current_find_index_ = -1;
    processFindResults();
}

auto MainWindow::processFindResults() -> void
{
    if( current_find_results_.empty() ) {
        task_status_label_->setText(
            QString( "Brak dopasowań dla '%1'" ).arg( current_find_text_ ) );
        QMessageBox::information( find_replace_dialog_, "Znajdź", "Nie znaleziono tekstu." );
        return;
    }

    ++current_find_index_;
    if( current_find_index_ >= static_cast<int>( current_find_results_.size() ) ) {
        current_find_index_ = 0;
        task_status_label_->setText( "Wyszukiwanie zawinięte na początek" );
    }

    uint64_t targetPos = current_find_results_[current_find_index_];
    int matchByteLen = static_cast<int>( current_find_text_.toUtf8().length() );
    viewer_->jumpToLogicalPosition( targetPos, matchByteLen );
    viewer_->setSearchHighlights( current_find_results_, current_find_index_,
                                  current_find_text_.length() );

    task_status_label_->setText( QString( "Dopasowanie %1 z %2" )
                                     .arg( current_find_index_ + 1 )
                                     .arg( current_find_results_.size() ) );
}

auto MainWindow::onReplaceNextRequested( const QString& findText, const QString& replaceText,
                                         bool matchCase, bool matchWord ) -> void
{
    if( !piece_table_ || findText.isEmpty() ) {
        return;
    }

    if( findText != current_find_text_ || current_find_index_ < 0 ) {
        onFindNextRequested( findText, matchCase, matchWord );
        return;
    }

    int findTextLen = findText.toUtf8().length();
    int replaceTextLen = replaceText.toUtf8().length();

    uint64_t pos = current_find_results_[current_find_index_];
    piece_table_->remove( pos, findTextLen );
    piece_table_->insert( pos, replaceText.toUtf8().toStdString() );

    viewer_->refreshView();
    setWindowModified( true );
    task_status_label_->setText( "Zamieniono wystąpienie" );

    int offsetShift = replaceTextLen - findTextLen;
    for( size_t idx = current_find_index_ + 1; idx < current_find_results_.size(); ++idx ) {
        current_find_results_[idx] += static_cast<uint64_t>( offsetShift );
    }

    current_find_results_.erase( current_find_results_.begin() + current_find_index_ );
    --current_find_index_;  // removed the current item

    viewer_->setSearchHighlights( current_find_results_, current_find_index_ + 1,
                                  current_find_text_.length() );

    onFindNextRequested( findText, matchCase, matchWord );
}

auto MainWindow::onReplaceAllRequested( const QString& findText, const QString& replaceText,
                                        bool matchCase, bool matchWord ) -> void
{
    if( !piece_table_ || findText.isEmpty() || tasks_->isReplaceRunning() ) {
        return;
    }

    viewer_->setEnabled( false );
    viewer_->setBusy( true );  // worker owns the PieceTable; viewer must not read it
    save_act_->setEnabled( false );
    open_act_->setEnabled( false );

    task_progress_bar_->show();
    task_progress_bar_->setRange( 0, 100 );
    task_progress_bar_->setValue( 0 );
    cancel_task_btn_->show();
    task_status_label_->setText( "Zamienianie..." );

    tasks_->startReplaceAll( piece_table_.get(), findText, replaceText, matchCase, matchWord );
}

auto MainWindow::onReplaceAllFinished( uint64_t replacedCount, bool canceled ) -> void
{
    task_progress_bar_->hide();
    cancel_task_btn_->hide();
    viewer_->setEnabled( true );
    viewer_->setBusy( false );  // re-enable rendering before any paint
    save_act_->setEnabled( true );
    open_act_->setEnabled( true );

    current_find_results_.clear();
    current_find_index_ = -1;
    current_find_text_ = "";
    viewer_->setSearchHighlights( {}, -1, 0 );

    const uint64_t replaced = replacedCount;

    if( canceled ) {
        task_status_label_->setText( "Zamiana wszystkich anulowana" );
        if( replaced > 0 ) {  // rare: cancel arrived after the commit
            viewer_->refreshView();
            setWindowModified( true );
        }
        return;
    }

    if( replaced > 0 ) {
        viewer_->refreshView();
        setWindowModified( true );
        task_status_label_->setText(
            QString( "Pomyślnie zamieniono %1 wystąpień" ).arg( replaced ) );
    } else {
        task_status_label_->setText( "Zamień wszystko: brak dopasowań" );
        QMessageBox::information( this, "Zamień wszystko", "Nie znaleziono tekstu." );
    }
}
