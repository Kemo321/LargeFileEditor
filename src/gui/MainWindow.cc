/**
 * Authors: Tomasz Okon
 * Description: Implementation of the GUI window.
 */

#include "gui/MainWindow.h"

using namespace std;

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent )
{
    setWindowTitle( "Large File Editor" );
    int WIDTH = 600;
    int HEIGHT = 400;
    resize( WIDTH, HEIGHT );

    piece_table_.insert( 0, "Initial test data for Piece Table" );

    status_label_ = new QLabel( this );
    status_label_->setAlignment( Qt::AlignCenter );

    QString statusText =
        QString( "Backend loaded. Document size: %1 characters" ).arg( piece_table_.size() );

    status_label_->setText( statusText );

    setCentralWidget( status_label_ );
}

MainWindow::~MainWindow() = default;
