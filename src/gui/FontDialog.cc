/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the Font dialog.
 */

#include "gui/FontDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

FontDialog::FontDialog( QWidget* parent ) : QDialog( parent )
{
    setWindowTitle( "Font Settings" );
    setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    setModal( true );

    QVBoxLayout* mainLayout = new QVBoxLayout( this );

    QHBoxLayout* sizeLayout = new QHBoxLayout();
    QLabel* sizeLabel = new QLabel( "Font size:", this );
    size_combo_ = new QComboBox( this );
    size_combo_->addItem( "Small" );
    size_combo_->addItem( "Medium" );
    size_combo_->addItem( "Large" );

    // Set medium as default
    size_combo_->setCurrentIndex( 1 );

    sizeLayout->addWidget( sizeLabel );
    sizeLayout->addWidget( size_combo_ );

    QDialogButtonBox* buttonBox =
        new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );

    connect( buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );
    connect( buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject );

    mainLayout->addLayout( sizeLayout );
    mainLayout->addWidget( buttonBox );

    setLayout( mainLayout );
}
