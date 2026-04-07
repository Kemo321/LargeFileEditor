/**
 * Authors: Tomasz Okon
 * Description: Implementation of the GUI window.
 */

#include "gui/MainWindow.h"

using namespace std;

MainWindow::MainWindow( QWidget* parent ) : QMainWindow( parent )
{
    setWindowTitle( "Large File Editor" );
    resize( 600, 400 );

    piece_table_.addSize( 100 );

    status_label_ = new QLabel( this );
    status_label_->setAlignment( Qt::AlignCenter );
    status_label_->setText( "Zaladowano backend. Rozmiar: " +
                            QString::number( piece_table_.getSize() ) );

    setCentralWidget( status_label_ );
}

MainWindow::~MainWindow() = default;
