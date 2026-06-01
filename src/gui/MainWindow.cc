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
        viewer_->setSearchHighlights( {}, -1, 0 );
        task_status_label_->setText( "Ready" );
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
            task_status_label_->setText( "Canceling..." );
        }
    } );

    connect( viewer_, &LargeFileViewer::cursorPositionChanged, this, [this]( int line, int col ) {
        cursor_pos_label_->setText( QString( "Line %1, Col %2" ).arg( line + 1 ).arg( col + 1 ) );
    } );

    connect( viewer_, &LargeFileViewer::documentModified, this,
             [this]() { setWindowModified( true ); } );

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
    // A save kicked off earlier (or by the prompt below) is still running: never block the GUI
    // thread waiting for it — defer the close until onSaveFinished() re-issues it.
    if( tasks_->isSaveRunning() ) {
        beginCloseWait( event );
        return;
    }

    if( !isWindowModified() ) {
        event->accept();
        return;
    }

    QMessageBox msgBox(
        QMessageBox::Warning, "Unsaved Changes",
        "The document has been modified. Do you want to save your changes before closing?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this );
    msgBox.setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );

    int reply = msgBox.exec();

    if( reply == QMessageBox::Save ) {
        saveFile();  // asynchronous: starts a background save if possible
        if( tasks_->isSaveRunning() ) {
            beginCloseWait( event );
        } else {
            event->accept();
        }
    } else if( reply == QMessageBox::Cancel ) {
        event->ignore();
    } else {
        event->accept();
    }
}

auto MainWindow::beginCloseWait( QCloseEvent* event ) -> void
{
    event->ignore();
    close_after_save_ = true;
    viewer_->setEnabled( false );
    menuBar()->setEnabled( false );
    task_status_label_->setText( "Closing, waiting for save operation to finish..." );
}

auto MainWindow::createActions() -> void
{
    open_act_ = new QAction( "&Open...", this );
    open_act_->setShortcuts( QKeySequence::Open );
    connect( open_act_, &QAction::triggered, this, &MainWindow::openFile );

    save_act_ = new QAction( "&Save", this );
    save_act_->setShortcuts( QKeySequence::Save );
    connect( save_act_, &QAction::triggered, this, &MainWindow::saveFile );

    save_as_act_ = new QAction( "Save &As...", this );
    save_as_act_->setShortcuts( QKeySequence::SaveAs );
    connect( save_as_act_, &QAction::triggered, this, &MainWindow::saveFileAs );

    exit_act_ = new QAction( "E&xit", this );
    exit_act_->setShortcuts( QKeySequence::Quit );
    connect( exit_act_, &QAction::triggered, qApp, &QApplication::quit );

    find_act_ = new QAction( "&Find...", this );
    find_act_->setShortcuts( QKeySequence::Find );
    connect( find_act_, &QAction::triggered, this, &MainWindow::findText );

    replace_act_ = new QAction( "&Replace...", this );
    replace_act_->setShortcuts( QKeySequence::Replace );
    connect( replace_act_, &QAction::triggered, this, &MainWindow::replaceText );

    font_small_act_ = new QAction( "Small", this );
    font_medium_act_ = new QAction( "Medium", this );
    font_large_act_ = new QAction( "Large", this );

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
    QMenu* fileMenu = menuBar()->addMenu( "&File" );
    fileMenu->addAction( open_act_ );
    fileMenu->addAction( save_act_ );
    fileMenu->addAction( save_as_act_ );
    fileMenu->addSeparator();
    fileMenu->addAction( exit_act_ );

    QMenu* editMenu = menuBar()->addMenu( "&Edit" );
    editMenu->addAction( find_act_ );
    editMenu->addAction( replace_act_ );

    QMenu* viewMenu = menuBar()->addMenu( "&View" );
    QMenu* fontSizeMenu = viewMenu->addMenu( "Font Size" );
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

    cancel_task_btn_ = new QPushButton( "Cancel", this );
    cancel_task_btn_->hide();

    task_status_label_ = new QLabel( "Ready", this );

    statusBar()->addWidget( task_status_label_ );
    statusBar()->addWidget( task_progress_bar_ );
    statusBar()->addWidget( cancel_task_btn_ );

    cursor_pos_label_ = new QLabel( "Line 1, Col 1", this );
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

auto MainWindow::setFontSizeSmall() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeSmall ) );
    task_status_label_->setText( "Font size set to Small" );
}

auto MainWindow::setFontSizeMedium() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeMedium ) );
    task_status_label_->setText( "Font size set to Medium" );
}

auto MainWindow::setFontSizeLarge() -> void
{
    viewer_->setFont( QFont( viewer_->font().family(), kFontSizeLarge ) );
    task_status_label_->setText( "Font size set to Large" );
}

auto MainWindow::openFile() -> void
{
    if( tasks_->isReplaceRunning() ) {
        return;
    }
    QString fileName = QFileDialog::getOpenFileName( this, "Open File", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Opening file..." );
        task_progress_bar_->show();
        task_progress_bar_->setRange( 0, 0 );

        if( FileUtils::isBinaryFile( fileName ) ) {
            task_progress_bar_->hide();
            task_status_label_->setText( "Unsupported file type: Binary" );
            QMessageBox::warning( this, "Unsupported File",
                                  "Unsupported file type: Binary files are not supported." );
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
                QString( "Loaded: %1" ).arg( QFileInfo( fileName ).fileName() ) );
        } catch( ... ) {
            task_status_label_->setText( "Error: Failed to open file" );
            QMessageBox::critical( this, "Error", "Could not open the file." );
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
    task_status_label_->setText( "Saving in background..." );

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
            task_status_label_->setText( "Save error: Rename failed" );
            QMessageBox::critical( this, "Save Error",
                                   "Could not save the file: Rename failed from temp file." );
            finalizePendingClose();
            return;
        }

        QFile::remove( backup_filename );

        int currentScroll = viewer_->verticalScrollBar()->value();
        piece_table_ = std::make_unique<PieceTable>( current_filename_.toStdString() );
        viewer_->setPieceTable( piece_table_.get() );
        viewer_->verticalScrollBar()->setValue( currentScroll );

        setWindowModified( false );
        task_status_label_->setText( "File saved successfully" );
    } else {
        task_status_label_->setText( "Critical: Save failed" );
        QMessageBox::critical( this, "Save Error",
                               "Could not save the file: Backend piece table write failed." );
    }

    QTimer::singleShot( kStatusClearDelayMs, this, [this]() {
        if( task_status_label_->text().contains( "successfully" ) ) {
            task_status_label_->setText( "Ready" );
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
        // Save failed: abandon the close and restore the UI so the user can react.
        close_after_save_ = false;
        menuBar()->setEnabled( true );
        return;
    }

    close();  // re-enters closeEvent, which now accepts (no save running, not modified)
}

auto MainWindow::saveFileAs() -> void
{
    if( !piece_table_ || tasks_->isReplaceRunning() ) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName( this, "Save File As", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Saving as..." );
        if( piece_table_->saveToFile( fileName.toStdString() ) ) {
            tasks_->waitForSave();

            current_filename_ = fileName;
            setWindowModified( false );
            updateWindowTitle();
            task_status_label_->setText( "File saved as: " + QFileInfo( fileName ).fileName() );
        } else {
            task_status_label_->setText( "Save As failed!" );
            QMessageBox::critical( this, "Save Error",
                                   "Could not save the file: Backend write failed." );
        }
    }
}

auto MainWindow::findText() -> void
{
    find_replace_dialog_->showFind();
    task_status_label_->setText( "Find dialog opened" );
}

auto MainWindow::replaceText() -> void
{
    find_replace_dialog_->showReplace();
    task_status_label_->setText( "Replace dialog opened" );
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
        task_status_label_->setText( "Searching..." );

        viewer_->setEnabled( false );

        tasks_->startFind( piece_table_.get(), text, matchCase, matchWord );
    } else {
        processFindResults();
    }
}

auto MainWindow::onFindFinished( std::vector<uint64_t> results ) -> void
{
    task_progress_bar_->hide();
    viewer_->setEnabled( true );
    current_find_results_ = std::move( results );
    current_find_index_ = -1;
    processFindResults();
}

auto MainWindow::processFindResults() -> void
{
    if( current_find_results_.empty() ) {
        task_status_label_->setText( QString( "No matches for '%1'" ).arg( current_find_text_ ) );
        QMessageBox::information( find_replace_dialog_, "Find", "Text not found." );
        return;
    }

    current_find_index_++;
    if( current_find_index_ >= static_cast<int>( current_find_results_.size() ) ) {
        current_find_index_ = 0;
        task_status_label_->setText( "Search wrapped to top" );
    }

    uint64_t targetPos = current_find_results_[current_find_index_];
    int matchByteLen = static_cast<int>( current_find_text_.toUtf8().length() );
    viewer_->jumpToLogicalPosition( targetPos, matchByteLen );
    viewer_->setSearchHighlights( current_find_results_, current_find_index_,
                                  current_find_text_.length() );

    task_status_label_->setText( QString( "Match %1 of %2" )
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
    task_status_label_->setText( "Occurrence replaced" );

    int offsetShift = replaceTextLen - findTextLen;
    for( size_t idx = current_find_index_ + 1; idx < current_find_results_.size(); ++idx ) {
        current_find_results_[idx] += static_cast<uint64_t>( offsetShift );
    }

    current_find_results_.erase( current_find_results_.begin() + current_find_index_ );
    current_find_index_--;  // Adjust index because we removed the current item

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
    task_status_label_->setText( "Replacing..." );

    tasks_->startReplaceAll( piece_table_.get(), findText, replaceText, matchCase, matchWord );
}

auto MainWindow::onReplaceAllFinished( uint64_t replacedCount, bool canceled ) -> void
{
    task_progress_bar_->hide();
    cancel_task_btn_->hide();
    viewer_->setEnabled( true );
    viewer_->setBusy( false );  // re-enable rendering before any refreshView/paint
    save_act_->setEnabled( true );
    open_act_->setEnabled( true );

    current_find_results_.clear();
    current_find_index_ = -1;
    current_find_text_ = "";
    viewer_->setSearchHighlights( {}, -1, 0 );

    const uint64_t replaced = replacedCount;

    if( canceled ) {
        task_status_label_->setText( "Replace All canceled" );
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
            QString( "Successfully replaced %1 occurrences" ).arg( replaced ) );
    } else {
        task_status_label_->setText( "Replace All: No matches found" );
        QMessageBox::information( this, "Replace All", "Text not found." );
    }
}
