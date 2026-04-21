/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the application's main window (Qt).
 */

#include "gui/MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>

#include "gui/FontDialog.h"

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent ), current_filename_( "Untitled" )
{
    viewer_ = new LargeFileViewer( this );
    setCentralWidget( viewer_ );

    find_replace_dialog_ = new FindReplaceDialog( this );

    createActions();
    createMenus();
    createStatusBar();

    updateWindowTitle();
    resize( 800, 600 );

    // Mock an unsaved modification after a few seconds to demonstrate the '*'
    QTimer::singleShot( 2000, this, &MainWindow::setModifiedMock );
}

void MainWindow::createActions()
{
    // File
    open_act_ = new QAction( "&Open...", this );
    open_act_->setShortcuts( QKeySequence::Open );
    open_act_->setStatusTip( "Open a file" );
    connect( open_act_, &QAction::triggered, this, &MainWindow::openFile );

    save_act_ = new QAction( "&Save", this );
    save_act_->setShortcuts( QKeySequence::Save );
    save_act_->setStatusTip( "Save the current file" );
    connect( save_act_, &QAction::triggered, this, &MainWindow::saveFile );

    save_as_act_ = new QAction( "Save &As...", this );
    save_as_act_->setShortcuts( QKeySequence::SaveAs );
    save_as_act_->setStatusTip( "Save the current file as ..." );
    connect( save_as_act_, &QAction::triggered, this, &MainWindow::saveFileAs );

    exit_act_ = new QAction( "E&xit", this );
    exit_act_->setShortcuts( QKeySequence::Quit );
    exit_act_->setStatusTip( "Exit the application" );
    connect( exit_act_, &QAction::triggered, qApp, &QApplication::quit );

    // Edit
    copy_act_ = new QAction( "&Copy", this );
    copy_act_->setShortcuts( QKeySequence::Copy );
    copy_act_->setStatusTip( "Copy selected text" );

    cut_act_ = new QAction( "Cu&t", this );
    cut_act_->setShortcuts( QKeySequence::Cut );
    cut_act_->setStatusTip( "Cut selected text" );

    paste_act_ = new QAction( "&Paste", this );
    paste_act_->setShortcuts( QKeySequence::Paste );
    paste_act_->setStatusTip( "Paste clipboard content" );

    find_act_ = new QAction( "&Find...", this );
    find_act_->setShortcuts( QKeySequence::Find );
    find_act_->setStatusTip( "Find text in the document" );
    connect( find_act_, &QAction::triggered, this, &MainWindow::findText );

    replace_act_ = new QAction( "&Replace...", this );
    replace_act_->setShortcuts( QKeySequence::Replace );
    replace_act_->setStatusTip( "Replace text in the document" );
    connect( replace_act_, &QAction::triggered, this, &MainWindow::replaceText );

    // View
    font_act_ = new QAction( "&Font...", this );
    font_act_->setStatusTip( "Edit font settings" );
    connect( font_act_, &QAction::triggered, this, &MainWindow::showFontDialog );
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
    viewMenu->addAction( font_act_ );
}

void MainWindow::createStatusBar()
{
    // Left side: Progress bar and status
    task_progress_bar_ = new QProgressBar( this );
    task_progress_bar_->setMaximumWidth( 200 );
    task_progress_bar_->setMaximumHeight( 14 );
    task_progress_bar_->hide();

    task_status_label_ = new QLabel( "Ready", this );

    statusBar()->addWidget( task_status_label_ );
    statusBar()->addWidget( task_progress_bar_ );

    // Right side: Cursor Position
    cursor_pos_label_ = new QLabel( "Line 1, Col 1", this );
    statusBar()->addPermanentWidget( cursor_pos_label_ );
}

void MainWindow::updateWindowTitle()
{
    setWindowFilePath( current_filename_ );
    setWindowTitle( QString( "[*]%1 - LargeFileEditor" ).arg( current_filename_ ) );
}

void MainWindow::setModifiedMock()
{
    setWindowModified( true );

    viewer_->setMockHighlights( QStringList{ "dolore", "culpa" } );

    task_status_label_->setText( "Mock modifications applied." );
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName( this, "Open File", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        qDebug() << "Opened file:" << fileName;
        current_filename_ = QFileInfo( fileName ).fileName();
        setWindowModified( false );
        updateWindowTitle();
    }
}

void MainWindow::saveFile()
{
    QMessageBox::information( this, "Mockup", "Saving File Mockup" );
    setWindowModified( false );

    // Show mock progress
    task_progress_bar_->show();
    task_progress_bar_->setValue( 50 );
    task_status_label_->setText( "Saving..." );

    QTimer::singleShot( 2000, this, [this]() {
        task_progress_bar_->hide();
        task_status_label_->setText( "Ready" );
    } );
}

void MainWindow::saveFileAs()
{
    QString fileName = QFileDialog::getSaveFileName( this, "Save File As", "", "All Files (*)" );
    if( !fileName.isEmpty() ) {
        qDebug() << "Save as file:" << fileName;
        current_filename_ = QFileInfo( fileName ).fileName();
        setWindowModified( false );
        updateWindowTitle();
    }
}

void MainWindow::findText()
{
    find_replace_dialog_->showFind();
}

void MainWindow::replaceText()
{
    find_replace_dialog_->showReplace();
}

void MainWindow::showFontDialog()
{
    FontDialog dialog( this );
    if( dialog.exec() == QDialog::Accepted ) {
        qDebug() << "Font size accepted.";
    }
}
