/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the GUI window.
 */

#include "gui/MainWindow.h"

#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

using namespace std;

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent )
{
    setWindowTitle( "Large File Editor" );
    int WIDTH = 800;
    int HEIGHT = 600;
    resize( WIDTH, HEIGHT );

    piece_table_.insert( 0, "Initial test data for Piece Table" );

    viewer_ = new LargeFileViewer( this );
    setCentralWidget( viewer_ );

    createActions();
    createMenus();
    createStatusBar();
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    open_act_ = new QAction( "&Open...", this );
    open_act_->setShortcuts( QKeySequence::Open );
    open_act_->setStatusTip( "Open a massive file instantly" );

    save_as_act_ = new QAction( "&Save As...", this );
    save_as_act_->setShortcuts( QKeySequence::SaveAs );
    save_as_act_->setStatusTip( "Consolidate and save modifications to a new file" );

    exit_act_ = new QAction( "E&xit", this );
    exit_act_->setShortcuts( QKeySequence::Quit );
    exit_act_->setStatusTip( "Exit the application" );

    find_act_ = new QAction( "&Find...", this );
    find_act_->setShortcuts( QKeySequence::Find );
    find_act_->setStatusTip( "Search for text within the massive file" );

    replace_act_ = new QAction( "&Replace...", this );
    replace_act_->setShortcuts( QKeySequence::Replace );
    replace_act_->setStatusTip( "Replace text within the massive file" );
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu( "&File" );
    fileMenu->addAction( open_act_ );
    fileMenu->addAction( save_as_act_ );
    fileMenu->addSeparator();
    fileMenu->addAction( exit_act_ );

    QMenu* editMenu = menuBar()->addMenu( "&Edit" );
    editMenu->addAction( find_act_ );
    editMenu->addAction( replace_act_ );
}

void MainWindow::createStatusBar()
{
    status_label_ = new QLabel( "Ready. Document size: 0 MB" );
    status_label_->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
    statusBar()->addWidget( status_label_ );
}
