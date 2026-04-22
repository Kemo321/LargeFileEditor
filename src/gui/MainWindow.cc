#include "gui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QScrollBar>

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent ), current_filename_( "Untitled" )
{
    viewer_ = new LargeFileViewer( this );
    setCentralWidget( viewer_ );

    find_replace_dialog_ = new FindReplaceDialog( this );

    connect( find_replace_dialog_, &FindReplaceDialog::findNextRequested, this,
             &MainWindow::onFindNextRequested );
    connect( find_replace_dialog_, &FindReplaceDialog::replaceNextRequested, this,
             &MainWindow::onReplaceNextRequested );
    connect( find_replace_dialog_, &FindReplaceDialog::replaceAllRequested, this,
             &MainWindow::onReplaceAllRequested );

    createActions();
    createMenus();
    createStatusBar();

    QTimer *cursorTimer = new QTimer(this);
    connect(cursorTimer, &QTimer::timeout, this, [this]() {
    });
    cursorTimer->start(100);

    setWindowModified( false );
    updateWindowTitle();
    resize( 800, 600 );
}

void MainWindow::createActions()
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

    copy_act_ = new QAction( "&Copy", this );
    cut_act_ = new QAction( "Cu&t", this );
    paste_act_ = new QAction( "&Paste", this );

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

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu( "&File" );
    fileMenu->addAction( open_act_ );
    fileMenu->addAction( save_act_ );
    fileMenu->addAction( save_as_act_ );
    fileMenu->addSeparator();
    fileMenu->addAction( exit_act_ );

    QMenu* editMenu = menuBar()->addMenu( "&Edit" );
    editMenu->addAction( copy_act_ );
    editMenu->addAction( cut_act_ );
    editMenu->addAction( paste_act_ );
    editMenu->addSeparator();
    editMenu->addAction( find_act_ );
    editMenu->addAction( replace_act_ );

    QMenu* viewMenu = menuBar()->addMenu( "&View" );
    QMenu* fontSizeMenu = viewMenu->addMenu( "Font Size" );
    fontSizeMenu->addAction( font_small_act_ );
    fontSizeMenu->addAction( font_medium_act_ );
    fontSizeMenu->addAction( font_large_act_ );
}

void MainWindow::createStatusBar()
{
    task_progress_bar_ = new QProgressBar( this );
    task_progress_bar_->setMaximumWidth( 200 );
    task_progress_bar_->setMaximumHeight( 14 );
    task_progress_bar_->hide();

    task_status_label_ = new QLabel( "Ready", this );

    statusBar()->addWidget( task_status_label_ );
    statusBar()->addWidget( task_progress_bar_ );

    cursor_pos_label_ = new QLabel( "Line 1, Col 1", this );
    statusBar()->addPermanentWidget( cursor_pos_label_ );
}

void MainWindow::updateWindowTitle()
{
    setWindowFilePath( current_filename_ );
    QString displayName =
        current_filename_ == "Untitled" ? "Untitled" : QFileInfo( current_filename_ ).fileName();
    setWindowTitle( QString( "[*]%1 - LargeFileEditor" ).arg( displayName ) );
}

void MainWindow::setFontSizeSmall()
{
    viewer_->setFont( QFont( viewer_->font().family(), 8 ) );
    task_status_label_->setText("Font size set to Small");
}

void MainWindow::setFontSizeMedium()
{
    viewer_->setFont( QFont( viewer_->font().family(), 11 ) );
    task_status_label_->setText("Font size set to Medium");
}

void MainWindow::setFontSizeLarge()
{
    viewer_->setFont( QFont( viewer_->font().family(), 14 ) );
    task_status_label_->setText("Font size set to Large");
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName( this, "Open File", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText("Opening file...");
        task_progress_bar_->show();
        task_progress_bar_->setRange(0, 0); // Infinite pulse

        current_filename_ = fileName;
        current_find_text_ = "";
        current_find_index_ = -1;
        current_find_results_.clear();

        try {
            piece_table_ = std::make_unique<PieceTable>( fileName.toStdString() );
            viewer_->setPieceTable( piece_table_.get() );
            viewer_->setMockHighlights( QStringList{} );
            setWindowModified( false );
            updateWindowTitle();
            task_status_label_->setText(QString("Loaded: %1").arg(QFileInfo(fileName).fileName()));
        } catch (...) {
            task_status_label_->setText("Error: Failed to open file");
            QMessageBox::critical(this, "Error", "Could not open the file.");
        }

        task_progress_bar_->hide();
    }
}

void MainWindow::saveFile()
{
    if( !piece_table_ ) {
        return;
    }

    task_progress_bar_->show();
    task_progress_bar_->setRange( 0, 100 );
    task_progress_bar_->setValue(50);
    task_status_label_->setText( "Saving..." );

    QString tempFileName = current_filename_ + ".tmp";

    if( piece_table_->saveToFile( tempFileName.toStdString() ) ) {
        
        if ( QFile::exists( current_filename_ ) && !QFile::remove( current_filename_ ) ) {
            QFile::remove( tempFileName );
            task_status_label_->setText( "Save error: Access denied" );
            task_progress_bar_->hide();
            return;
        }

        if ( !QFile::rename( tempFileName, current_filename_ ) ) {
            task_status_label_->setText( "Save error: Rename failed" );
            task_progress_bar_->hide();
            return;
        }

        int currentScroll = viewer_->verticalScrollBar()->value();
        piece_table_ = std::make_unique<PieceTable>( current_filename_.toStdString() );
        viewer_->setPieceTable( piece_table_.get() );
        viewer_->verticalScrollBar()->setValue( currentScroll );

        setWindowModified( false );
        task_status_label_->setText( "File saved successfully" );
        task_progress_bar_->setValue(100);
    } else {
        task_status_label_->setText( "Critical: Save failed" );
    }

    QTimer::singleShot( 2000, this, [this]() {
        task_progress_bar_->hide();
        if (task_status_label_->text().contains("successfully"))
            task_status_label_->setText( "Ready" );
    } );
}

void MainWindow::saveFileAs()
{
    if( !piece_table_ ) {
        return;
    }

    QString fileName = QFileDialog::getSaveFileName( this, "Save File As", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        task_status_label_->setText("Saving as...");
        if( piece_table_->saveToFile( fileName.toStdString() ) ) {
            current_filename_ = fileName;
            setWindowModified( false );
            updateWindowTitle();
            task_status_label_->setText("File saved as: " + QFileInfo(fileName).fileName());
        } else {
            task_status_label_->setText("Save As failed!");
        }
    }
}

void MainWindow::findText()
{
    find_replace_dialog_->showFind();
    task_status_label_->setText("Find dialog opened");
}

void MainWindow::replaceText()
{
    find_replace_dialog_->showReplace();
    task_status_label_->setText("Replace dialog opened");
}

void MainWindow::onFindNextRequested( const QString& text, bool matchCase, bool matchWord )
{
    if ( !piece_table_ || text.isEmpty() ) return;

    static bool lastMatchCase = matchCase;
    static bool lastMatchWord = matchWord;

    if ( text != current_find_text_ || matchCase != lastMatchCase || matchWord != lastMatchWord ) {
        current_find_text_ = text;
        lastMatchCase = matchCase;
        lastMatchWord = matchWord;
        
        current_find_results_ = piece_table_->findAll( text.toStdString(), matchCase, matchWord );
        current_find_index_ = -1;
    }

    if ( current_find_results_.empty() ) {
        task_status_label_->setText(QString("No matches for '%1'").arg(text));
        QMessageBox::information( find_replace_dialog_, "Find", "Text not found." );
        return;
    }

    current_find_index_++;
    if ( current_find_index_ >= static_cast<int>( current_find_results_.size() ) ) {
        current_find_index_ = 0; 
        task_status_label_->setText("Search wrapped to top");
    }

    uint64_t targetPos = current_find_results_[current_find_index_];
    viewer_->jumpToLogicalPosition( targetPos );
    viewer_->setMockHighlights( QStringList{ text } );
    
    task_status_label_->setText( QString( "Match %1 of %2" )
        .arg( current_find_index_ + 1 )
        .arg( current_find_results_.size() ) );
}

void MainWindow::onReplaceNextRequested( const QString& findText, const QString& replaceText,
                                         bool matchCase, bool matchWord )
{
    if( !piece_table_ || findText.isEmpty() ) {
        return;
    }

    if( findText != current_find_text_ || current_find_index_ < 0 ) {
        onFindNextRequested( findText, matchCase, matchWord );
        if( current_find_index_ < 0 ) return;
    }

    uint64_t pos = current_find_results_[current_find_index_];
    piece_table_->remove( pos, findText.length() );
    piece_table_->insert( pos, replaceText.toStdString() );

    viewer_->refreshView();
    setWindowModified( true );
    task_status_label_->setText( "Occurrence replaced" );

    int offsetShift = replaceText.length() - findText.length();
    for( size_t i = current_find_index_ + 1; i < current_find_results_.size(); ++i ) {
        current_find_results_[i] += offsetShift;
    }

    onFindNextRequested( findText, matchCase, matchWord );
}

void MainWindow::onReplaceAllRequested( const QString& findText, const QString& replaceText,
                                        bool matchCase, bool matchWord )
{
    if( !piece_table_ || findText.isEmpty() ) {
        return;
    }

    uint64_t replaced =
        piece_table_->replaceAll( findText.toStdString(), replaceText.toStdString() );

    if( replaced > 0 ) {
        viewer_->refreshView();
        current_find_text_ = "";
        task_status_label_->setText( QString( "Successfully replaced %1 occurrences" ).arg( replaced ) );
        setWindowModified( true );
    } else {
        task_status_label_->setText("Replace All: No matches found");
        QMessageBox::information( this, "Replace All", "Text not found." );
    }
}