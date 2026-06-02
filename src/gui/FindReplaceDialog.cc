// Author: Tomasz Okon, Jan Szwagierczak

#include "gui/FindReplaceDialog.h"

#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

FindReplaceDialog::FindReplaceDialog( QWidget* parent ) : QDialog( parent )
{
    setWindowTitle( "Znajdź i zamień" );
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

auto FindReplaceDialog::showFind() -> void
{
    tab_widget_->setCurrentIndex( 0 );
    show();
    raise();
    activateWindow();
}

auto FindReplaceDialog::showReplace() -> void
{
    tab_widget_->setCurrentIndex( 1 );
    show();
    raise();
    activateWindow();
}

auto FindReplaceDialog::setActionsEnabled( bool enabled ) -> void
{
    for( QPushButton* button :
         { find_next_button_1_, find_next_button_2_, replace_button_, replace_all_button_ } ) {
        if( button != nullptr ) {
            button->setEnabled( enabled );
        }
    }
}

auto FindReplaceDialog::hideEvent( QHideEvent* event ) -> void
{
    QDialog::hideEvent( event );
    emit dialogClosed();
}

auto FindReplaceDialog::mousePressEvent( QMouseEvent* event ) -> void
{
    if( event->button() == Qt::LeftButton ) {
        if( windowHandle() != nullptr ) {
            windowHandle()->startSystemMove();
        }
        event->accept();
    }
}

auto FindReplaceDialog::setupUi() -> void
{
    auto* mainLayout = new QVBoxLayout( this );
    tab_widget_ = new QTabWidget( this );

    auto* findTab = new QWidget( tab_widget_ );
    auto* findLayout = new QVBoxLayout( findTab );

    auto* findInputLayout1 = new QHBoxLayout();
    findInputLayout1->addWidget( new QLabel( "Znajdź:" ) );
    find_input_1_ = new QLineEdit();
    findInputLayout1->addWidget( find_input_1_ );
    findLayout->addLayout( findInputLayout1 );

    match_case_1_ = new QCheckBox( "Uwzględnij wielkość liter" );
    match_word_1_ = new QCheckBox( "Całe słowa" );
    findLayout->addWidget( match_case_1_ );
    findLayout->addWidget( match_word_1_ );

    auto* findButtonLayout = new QHBoxLayout();
    findButtonLayout->addStretch();
    find_next_button_1_ = new QPushButton( "Znajdź następny" );
    auto* btnClose1 = new QPushButton( "Zamknij" );
    findButtonLayout->addWidget( find_next_button_1_ );
    findButtonLayout->addWidget( btnClose1 );
    connect( btnClose1, &QPushButton::clicked, this, &QDialog::hide );

    connect( find_next_button_1_, &QPushButton::clicked, this, [this]() {
        emit findNextRequested( find_input_1_->text(), match_case_1_->isChecked(),
                                match_word_1_->isChecked() );
    } );

    findLayout->addLayout( findButtonLayout );

    auto* replaceTab = new QWidget( tab_widget_ );
    auto* replaceLayout = new QVBoxLayout( replaceTab );

    auto* findInputLayout2 = new QHBoxLayout();
    findInputLayout2->addWidget( new QLabel( "Znajdź:" ) );
    find_input_2_ = new QLineEdit();
    findInputLayout2->addWidget( find_input_2_ );
    replaceLayout->addLayout( findInputLayout2 );

    auto* replaceInputLayout = new QHBoxLayout();
    replaceInputLayout->addWidget( new QLabel( "Zamień na:" ) );
    replace_input_ = new QLineEdit();
    replaceInputLayout->addWidget( replace_input_ );
    replaceLayout->addLayout( replaceInputLayout );

    match_case_2_ = new QCheckBox( "Uwzględnij wielkość liter" );
    match_word_2_ = new QCheckBox( "Całe słowa" );
    replaceLayout->addWidget( match_case_2_ );
    replaceLayout->addWidget( match_word_2_ );

    auto* replaceButtonLayout = new QHBoxLayout();
    replaceButtonLayout->addStretch();
    find_next_button_2_ = new QPushButton( "Znajdź następny" );
    replace_button_ = new QPushButton( "Zamień" );
    replace_all_button_ = new QPushButton( "Zamień wszystko" );
    auto* btnClose2 = new QPushButton( "Zamknij" );
    replaceButtonLayout->addWidget( find_next_button_2_ );
    replaceButtonLayout->addWidget( replace_button_ );
    replaceButtonLayout->addWidget( replace_all_button_ );
    replaceButtonLayout->addWidget( btnClose2 );

    connect( btnClose2, &QPushButton::clicked, this, &QDialog::hide );

    connect( find_next_button_2_, &QPushButton::clicked, this, [this]() {
        emit findNextRequested( find_input_2_->text(), match_case_2_->isChecked(),
                                match_word_2_->isChecked() );
    } );
    connect( replace_button_, &QPushButton::clicked, this, [this]() {
        emit replaceNextRequested( find_input_2_->text(), replace_input_->text(),
                                   match_case_2_->isChecked(), match_word_2_->isChecked() );
    } );
    connect( replace_all_button_, &QPushButton::clicked, this, [this]() {
        emit replaceAllRequested( find_input_2_->text(), replace_input_->text(),
                                  match_case_2_->isChecked(), match_word_2_->isChecked() );
    } );

    replaceLayout->addLayout( replaceButtonLayout );

    tab_widget_->addTab( findTab, "Znajdź" );
    tab_widget_->addTab( replaceTab, "Zamień" );
    mainLayout->addWidget( tab_widget_ );
    setLayout( mainLayout );
}
