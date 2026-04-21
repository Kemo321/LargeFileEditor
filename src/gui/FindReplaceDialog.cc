/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the non-modal Find and Replace dialog.
 */

#include "gui/FindReplaceDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

FindReplaceDialog::FindReplaceDialog( QWidget* parent ) : QDialog( parent )
{
    setWindowTitle( "Find and Replace" );
    setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint );
    setModal( false );

    setupUi();

    // Sync "Find what" inputs between tabs
    connect( find_input_1_, &QLineEdit::textEdited, find_input_2_, &QLineEdit::setText );
    connect( find_input_2_, &QLineEdit::textEdited, find_input_1_, &QLineEdit::setText );

    // Sync match case checkboxes
    connect( match_case_1_, &QCheckBox::toggled, match_case_2_, &QCheckBox::setChecked );
    connect( match_case_2_, &QCheckBox::toggled, match_case_1_, &QCheckBox::setChecked );

    // Sync match word checkboxes
    connect( match_word_1_, &QCheckBox::toggled, match_word_2_, &QCheckBox::setChecked );
    connect( match_word_2_, &QCheckBox::toggled, match_word_1_, &QCheckBox::setChecked );
}

void FindReplaceDialog::showFind()
{
    tab_widget_->setCurrentIndex( 0 );
    show();
    raise();
    activateWindow();
}

void FindReplaceDialog::showReplace()
{
    tab_widget_->setCurrentIndex( 1 );
    show();
    raise();
    activateWindow();
}

void FindReplaceDialog::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout( this );
    tab_widget_ = new QTabWidget( this );

    // --- Find Tab ---
    QWidget* findTab = new QWidget( tab_widget_ );
    QVBoxLayout* findLayout = new QVBoxLayout( findTab );

    QHBoxLayout* findInputLayout1 = new QHBoxLayout();
    findInputLayout1->addWidget( new QLabel( "Find what:" ) );
    find_input_1_ = new QLineEdit();
    findInputLayout1->addWidget( find_input_1_ );
    findLayout->addLayout( findInputLayout1 );

    match_case_1_ = new QCheckBox( "Match case" );
    match_word_1_ = new QCheckBox( "Match whole word" );
    findLayout->addWidget( match_case_1_ );
    findLayout->addWidget( match_word_1_ );

    QHBoxLayout* findButtonLayout = new QHBoxLayout();
    findButtonLayout->addStretch();
    QPushButton* btnFindNext1 = new QPushButton( "Find Next" );
    QPushButton* btnFindPrev1 = new QPushButton( "Find Previous" );
    QPushButton* btnClose1 = new QPushButton( "Close" );
    findButtonLayout->addWidget( btnFindNext1 );
    findButtonLayout->addWidget( btnFindPrev1 );
    findButtonLayout->addWidget( btnClose1 );
    connect( btnClose1, &QPushButton::clicked, this, &QDialog::hide );
    findLayout->addLayout( findButtonLayout );

    // --- Replace Tab ---
    QWidget* replaceTab = new QWidget( tab_widget_ );
    QVBoxLayout* replaceLayout = new QVBoxLayout( replaceTab );

    QHBoxLayout* findInputLayout2 = new QHBoxLayout();
    findInputLayout2->addWidget( new QLabel( "Find what:" ) );
    find_input_2_ = new QLineEdit();
    findInputLayout2->addWidget( find_input_2_ );
    replaceLayout->addLayout( findInputLayout2 );

    QHBoxLayout* replaceInputLayout = new QHBoxLayout();
    replaceInputLayout->addWidget( new QLabel( "Replace with:" ) );
    replace_input_ = new QLineEdit();
    replaceInputLayout->addWidget( replace_input_ );
    replaceLayout->addLayout( replaceInputLayout );

    match_case_2_ = new QCheckBox( "Match case" );
    match_word_2_ = new QCheckBox( "Match whole word" );
    replaceLayout->addWidget( match_case_2_ );
    replaceLayout->addWidget( match_word_2_ );

    QHBoxLayout* replaceButtonLayout = new QHBoxLayout();
    replaceButtonLayout->addStretch();
    QPushButton* btnFindNext2 = new QPushButton( "Find Next" );
    QPushButton* btnReplace = new QPushButton( "Replace" );
    QPushButton* btnReplaceAll = new QPushButton( "Replace All" );
    QPushButton* btnClose2 = new QPushButton( "Close" );
    replaceButtonLayout->addWidget( btnFindNext2 );
    replaceButtonLayout->addWidget( btnReplace );
    replaceButtonLayout->addWidget( btnReplaceAll );
    replaceButtonLayout->addWidget( btnClose2 );
    connect( btnClose2, &QPushButton::clicked, this, &QDialog::hide );
    replaceLayout->addLayout( replaceButtonLayout );

    // --- Add tabs ---
    tab_widget_->addTab( findTab, "Find" );
    tab_widget_->addTab( replaceTab, "Replace" );

    mainLayout->addWidget( tab_widget_ );
    setLayout( mainLayout );
}
