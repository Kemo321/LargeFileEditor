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

    connect( find_input_1_, &QLineEdit::textEdited, find_input_2_, &QLineEdit::setText );
    connect( find_input_2_, &QLineEdit::textEdited, find_input_1_, &QLineEdit::setText );
    connect( match_case_1_, &QCheckBox::toggled, match_case_2_, &QCheckBox::setChecked );
    connect( match_case_2_, &QCheckBox::toggled, match_case_1_, &QCheckBox::setChecked );
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
    auto* mainLayout = new QVBoxLayout( this );
    tab_widget_ = new QTabWidget( this );

    // --- Find Tab ---
    auto* findTab = new QWidget( tab_widget_ );
    auto* findLayout = new QVBoxLayout( findTab );

    auto* findInputLayout1 = new QHBoxLayout();
    findInputLayout1->addWidget( new QLabel( "Find what:" ) );
    find_input_1_ = new QLineEdit();
    findInputLayout1->addWidget( find_input_1_ );
    findLayout->addLayout( findInputLayout1 );

    match_case_1_ = new QCheckBox( "Match case" );
    match_word_1_ = new QCheckBox( "Match whole word" );
    findLayout->addWidget( match_case_1_ );
    findLayout->addWidget( match_word_1_ );

    auto* findButtonLayout = new QHBoxLayout();
    findButtonLayout->addStretch();
    auto* btnFindNext1 = new QPushButton( "Find Next" );
    auto* btnClose1 = new QPushButton( "Close" );
    findButtonLayout->addWidget( btnFindNext1 );
    findButtonLayout->addWidget( btnClose1 );
    connect( btnClose1, &QPushButton::clicked, this, &QDialog::hide );

    connect( btnFindNext1, &QPushButton::clicked, this, [this]() {
        emit findNextRequested( find_input_1_->text(), match_case_1_->isChecked(),
                                match_word_1_->isChecked() );
    } );

    findLayout->addLayout( findButtonLayout );

    auto* replaceTab = new QWidget( tab_widget_ );
    auto* replaceLayout = new QVBoxLayout( replaceTab );

    auto* findInputLayout2 = new QHBoxLayout();
    findInputLayout2->addWidget( new QLabel( "Find what:" ) );
    find_input_2_ = new QLineEdit();
    findInputLayout2->addWidget( find_input_2_ );
    replaceLayout->addLayout( findInputLayout2 );

    auto* replaceInputLayout = new QHBoxLayout();
    replaceInputLayout->addWidget( new QLabel( "Replace with:" ) );
    replace_input_ = new QLineEdit();
    replaceInputLayout->addWidget( replace_input_ );
    replaceLayout->addLayout( replaceInputLayout );

    match_case_2_ = new QCheckBox( "Match case" );
    match_word_2_ = new QCheckBox( "Match whole word" );
    replaceLayout->addWidget( match_case_2_ );
    replaceLayout->addWidget( match_word_2_ );

    auto* replaceButtonLayout = new QHBoxLayout();
    replaceButtonLayout->addStretch();
    auto* btnFindNext2 = new QPushButton( "Find Next" );
    auto* btnReplace = new QPushButton( "Replace" );
    auto* btnReplaceAll = new QPushButton( "Replace All" );
    auto* btnClose2 = new QPushButton( "Close" );
    replaceButtonLayout->addWidget( btnFindNext2 );
    replaceButtonLayout->addWidget( btnReplace );
    replaceButtonLayout->addWidget( btnReplaceAll );
    replaceButtonLayout->addWidget( btnClose2 );

    connect( btnClose2, &QPushButton::clicked, this, &QDialog::hide );

    connect( btnFindNext2, &QPushButton::clicked, this, [this]() {
        emit findNextRequested( find_input_2_->text(), match_case_2_->isChecked(),
                                match_word_2_->isChecked() );
    } );
    connect( btnReplace, &QPushButton::clicked, this, [this]() {
        emit replaceNextRequested( find_input_2_->text(), replace_input_->text(),
                                   match_case_2_->isChecked(), match_word_2_->isChecked() );
    } );
    connect( btnReplaceAll, &QPushButton::clicked, this, [this]() {
        emit replaceAllRequested( find_input_2_->text(), replace_input_->text(),
                                  match_case_2_->isChecked(), match_word_2_->isChecked() );
    } );

    replaceLayout->addLayout( replaceButtonLayout );

    tab_widget_->addTab( findTab, "Find" );
    tab_widget_->addTab( replaceTab, "Replace" );
    mainLayout->addWidget( tab_widget_ );
    setLayout( mainLayout );
}