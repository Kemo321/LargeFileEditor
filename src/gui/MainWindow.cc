#include "gui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QPromise>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>
#include <atomic>
#include <string>

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
        if( ( replace_watcher_ != nullptr ) && replace_watcher_->isRunning() ) {
            replace_canceled_ = true;
            replace_watcher_->future().cancel();
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

    save_watcher_ = new QFutureWatcher<bool>( this );
    connect( save_watcher_, &QFutureWatcher<bool>::finished, this, &MainWindow::onSaveFinished );

    find_watcher_ = new QFutureWatcher<std::vector<uint64_t>>( this );
    connect( find_watcher_, &QFutureWatcher<std::vector<uint64_t>>::finished, this,
             &MainWindow::onFindFinished );

    replace_watcher_ = new QFutureWatcher<uint64_t>( this );
    connect( replace_watcher_, &QFutureWatcher<uint64_t>::progressValueChanged, task_progress_bar_,
             &QProgressBar::setValue );
    connect( replace_watcher_, &QFutureWatcher<uint64_t>::finished, this,
             &MainWindow::onReplaceAllFinished );

    connect( cancel_task_btn_, &QPushButton::clicked, this, [this]() {
        if( ( replace_watcher_ != nullptr ) && replace_watcher_->isRunning() ) {
            replace_canceled_ = true;
            replace_watcher_->future().cancel();
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
    if( ( replace_watcher_ != nullptr ) && replace_watcher_->isRunning() ) {
        replace_watcher_->future().cancel();
        replace_watcher_->waitForFinished();
    }
    if( ( save_watcher_ != nullptr ) && save_watcher_->isRunning() ) {
        save_watcher_->waitForFinished();
    }
}

auto MainWindow::closeEvent( QCloseEvent* event ) -> void
{
    if( isWindowModified() ) {
        QMessageBox msgBox(
            QMessageBox::Warning, "Unsaved Changes",
            "The document has been modified. Do you want to save your changes before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this );
        msgBox.setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );

        int reply = msgBox.exec();

        if( reply == QMessageBox::Save ) {
            saveFile();
            // Since save is asynchronous, we cannot just accept immediately if we really want to
            // wait for it. But if it's already a temp file flow, we might need to block. For
            // simplicity and as per standard Qt flow:
            if( ( save_watcher_ != nullptr ) && save_watcher_->isRunning() ) {
                save_watcher_->waitForFinished();
            }
            event->accept();
        } else if( reply == QMessageBox::Cancel ) {
            event->ignore();
        } else {
            event->accept();
        }
    } else {
        event->accept();
    }
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
    if( ( replace_watcher_ != nullptr ) && replace_watcher_->isRunning() ) {
        return;
    }
    QString fileName = QFileDialog::getOpenFileName( this, "Open File", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Opening file..." );
        task_progress_bar_->show();
        task_progress_bar_->setRange( 0, 0 );

        if( isBinaryFile( fileName ) ) {
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

auto MainWindow::isBinaryFile( const QString& filePath ) -> bool
{
    QFile file( filePath );
    if( !file.open( QIODevice::ReadOnly ) ) {
        return false;
    }
    QByteArray chunk = file.read( 4096 );
    file.close();

    if( chunk.isEmpty() ) {
        return false;
    }

    int nullBytes = 0;
    int controlCount = 0;

    for( char i : chunk ) {
        auto uc = static_cast<unsigned char>( i );
        if( uc == '\0' ) {
            nullBytes++;
        } else if( uc < 32 ) {
            if( uc != '\t' && uc != '\n' && uc != '\r' ) {
                controlCount++;
            }
        } else if( uc == 127 ) {
            controlCount++;
        }
    }

    if( nullBytes > 0 ) {
        return true;
    }

    int invalidUtf8Count = 0;
    int i = 0;
    while( i < chunk.size() ) {
        auto b1 = static_cast<unsigned char>( chunk.at( i ) );
        if( b1 < 128 ) {
            i++;
            continue;
        }

        int seq_len = 0;
        if( ( b1 & 0xE0 ) == 0xC0 ) {
            seq_len = 2;
        } else if( ( b1 & 0xF0 ) == 0xE0 ) {
            seq_len = 3;
        } else if( ( b1 & 0xF8 ) == 0xF0 ) {
            seq_len = 4;
        } else {
            invalidUtf8Count++;
            i++;
            continue;
        }

        if( i + seq_len > chunk.size() ) {
            break;
        }

        bool valid_seq = true;
        for( int j = 1; j < seq_len; ++j ) {
            auto bj = static_cast<unsigned char>( chunk.at( i + j ) );
            if( ( bj & 0xC0 ) != 0x80 ) {
                valid_seq = false;
                break;
            }
        }

        if( !valid_seq ) {
            invalidUtf8Count++;
            i++;
        } else {
            i += seq_len;
        }
    }

    double nonTextRatio = static_cast<double>( controlCount + invalidUtf8Count ) / chunk.size();
    return nonTextRatio > 0.15;
}

auto MainWindow::saveFile() -> void
{
    if( current_filename_.isEmpty() ) {
        return;
    }

    if( !piece_table_ || save_watcher_->isRunning() || replace_watcher_->isRunning() ) {
        return;
    }

    viewer_->setEnabled( false );
    save_act_->setEnabled( false );
    open_act_->setEnabled( false );

    task_progress_bar_->show();
    task_progress_bar_->setRange( 0, 0 );
    task_status_label_->setText( "Saving in background..." );

    pending_temp_filename_ = current_filename_ + ".tmp";

    QFuture<bool> future = QtConcurrent::run(
        [this]() { return piece_table_->saveToFile( pending_temp_filename_.toStdString() ); } );

    save_watcher_->setFuture( future );
}

auto MainWindow::onSaveFinished() -> void
{
    bool success = save_watcher_->result();
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
}

auto MainWindow::saveFileAs() -> void
{
    if( !piece_table_ || replace_watcher_->isRunning() ) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName( this, "Save File As", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText( "Saving as..." );
        if( piece_table_->saveToFile( fileName.toStdString() ) ) {
            if( ( save_watcher_ != nullptr ) && save_watcher_->isRunning() ) {
                save_watcher_->waitForFinished();
            }

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
    if( !piece_table_ || text.isEmpty() || find_watcher_->isRunning() ) {
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

        QFuture<std::vector<uint64_t>> future =
            QtConcurrent::run( [this, text, matchCase, matchWord]() {
                return piece_table_->findAll( text.toStdString(), matchCase, matchWord );
            } );

        find_watcher_->setFuture( future );
    } else {
        processFindResults();
    }
}

auto MainWindow::onFindFinished() -> void
{
    task_progress_bar_->hide();
    viewer_->setEnabled( true );
    current_find_results_ = find_watcher_->result();
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
    if( !piece_table_ || findText.isEmpty() || replace_watcher_->isRunning() ) {
        return;
    }

    replace_canceled_ = false;
    viewer_->setEnabled( false );
    save_act_->setEnabled( false );
    open_act_->setEnabled( false );

    task_progress_bar_->show();
    task_progress_bar_->setRange( 0, 100 );
    task_progress_bar_->setValue( 0 );
    cancel_task_btn_->show();
    task_status_label_->setText( "Replacing..." );

    const std::string pat = findText.toUtf8().toStdString();
    const std::string repl = replaceText.toUtf8().toStdString();
    PieceTable* table = piece_table_.get();

    QFuture<uint64_t> future = QtConcurrent::run( [table, pat, repl, matchCase,
                                                   matchWord]( QPromise<uint64_t>& promise ) {
        promise.setProgressRange( 0, 100 );
        std::atomic<bool> cancel{ false };
        auto progress = [&promise, &cancel]( uint64_t done, uint64_t total ) {
            if( promise.isCanceled() ) {
                cancel.store( true );
            }
            promise.setProgressValue( total != 0 ? static_cast<int>( done * 100 / total ) : 100 );
        };
        promise.addResult( table->replaceAll( pat, repl, matchCase, matchWord, progress, cancel ) );
    } );

    replace_watcher_->setFuture( future );
}

auto MainWindow::onReplaceAllFinished() -> void
{
    task_progress_bar_->hide();
    cancel_task_btn_->hide();
    viewer_->setEnabled( true );
    save_act_->setEnabled( true );
    open_act_->setEnabled( true );

    current_find_results_.clear();
    current_find_index_ = -1;
    current_find_text_ = "";
    viewer_->setSearchHighlights( {}, -1, 0 );

    const uint64_t replaced = replace_watcher_->result();

    if( replace_canceled_ ) {
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
