/**
 * @file EditorControllerTest.cc
 * @brief Headless unit tests for EditorController: bounded navigation, UTF-8-aware cursor motion
 *        and multibyte-safe deletion.
 */
#include <gtest/gtest.h>

#include <QEvent>
#include <QKeyEvent>
#include <QSignalSpy>
#include <QString>
#include <string>

#include "QtTestApp.h"
#include "backend/PieceTable.h"
#include "gui/CursorManager.h"
#include "gui/EditorController.h"
#include "gui/LineManager.h"

namespace {

const std::string kEuro = "\xE2\x82\xAC";  // U+20AC, 3 bytes

}  // namespace

class EditorControllerTest : public ::testing::Test {
protected:
    auto SetUp() -> void override
    {
        test_support::qtApp();  // guarantee a QCoreApplication for the QObject machinery
    }

    // Builds the editing context around @p text and binds the controller to a fresh cursor.
    auto load( const std::string& text ) -> void
    {
        pt_.insert( 0, text );
        lm_ = std::make_unique<LineManager>( &pt_ );
        cursor_ = std::make_unique<CursorManager>();
        controller_ = std::make_unique<EditorController>( cursor_.get() );
        controller_->setContext( &pt_, lm_.get() );
    }

    // Synthesises a key press and forwards it to the controller, returning the modified flag.
    auto press( int key, const QString& text = QString() ) -> bool
    {
        QKeyEvent event( QEvent::KeyPress, key, Qt::NoModifier, text );
        return controller_->handleKeyPress( &event );
    }

    PieceTable pt_;
    std::unique_ptr<LineManager> lm_;
    std::unique_ptr<CursorManager> cursor_;
    std::unique_ptr<EditorController> controller_;
};

TEST_F( EditorControllerTest, CursorMovementBoundaries )
{
    load( "ab\ncd" );

    // Top-left corner: Left and Up must not escape (0, 0).
    cursor_->setPosition( 0, 0 );
    press( Qt::Key_Left );
    EXPECT_EQ( cursor_->line(), 0 );
    EXPECT_EQ( cursor_->col(), 0 );

    cursor_->setPosition( 0, 0 );
    press( Qt::Key_Up );
    EXPECT_EQ( cursor_->line(), 0 );
    EXPECT_EQ( cursor_->col(), 0 );

    // Absolute end of document (the virtual line holding the final byte): Right and Down stay put.
    const int lastLine = lm_->getVirtualLineFromOffset( pt_.size() );
    const int lastCol = static_cast<int>( lm_->getVirtualLineLength( lastLine ) );

    cursor_->setPosition( lastLine, lastCol );
    press( Qt::Key_Right );
    EXPECT_EQ( cursor_->line(), lastLine );
    EXPECT_EQ( cursor_->col(), lastCol );

    press( Qt::Key_Down );
    EXPECT_EQ( cursor_->line(), lastLine );
    EXPECT_EQ( cursor_->col(), lastCol );
}

TEST_F( EditorControllerTest, Utf8CursorMovement )
{
    load( "a" + kEuro + "b" );  // columns: 'a'=0, Euro=1..3, 'b'=4

    // Moving right from the Euro's lead byte jumps the whole 3-byte sequence (1 -> 4).
    cursor_->setPosition( 0, 1 );
    press( Qt::Key_Right );
    EXPECT_EQ( cursor_->col(), 4 );

    // Moving left from 'b' rewinds over every continuation byte back to the Euro's lead (4 -> 1).
    cursor_->setPosition( 0, 4 );
    press( Qt::Key_Left );
    EXPECT_EQ( cursor_->col(), 1 );
}

TEST_F( EditorControllerTest, BackspaceAndDeleteAtBoundaries )
{
    load( "ab" );

    // Backspace at offset 0 is a no-op: nothing modified, document and cursor untouched.
    cursor_->setPosition( 0, 0 );
    EXPECT_FALSE( press( Qt::Key_Backspace ) );
    EXPECT_EQ( pt_.size(), 2U );
    EXPECT_EQ( pt_.getText(), "ab" );
    EXPECT_EQ( cursor_->col(), 0 );

    // Delete at end-of-file is a no-op as well.
    const int lastLine = lm_->getVirtualLineFromOffset( pt_.size() );
    const int lastCol = static_cast<int>( lm_->getVirtualLineLength( lastLine ) );
    cursor_->setPosition( lastLine, lastCol );
    EXPECT_FALSE( press( Qt::Key_Delete ) );
    EXPECT_EQ( pt_.size(), 2U );
    EXPECT_EQ( pt_.getText(), "ab" );
}

TEST_F( EditorControllerTest, DeleteMultibyteChar )
{
    load( "a" + kEuro + "b" );

    QSignalSpy spy( controller_.get(), &EditorController::documentEdited );

    // Delete with the cursor on the Euro's lead byte removes all three of its bytes.
    cursor_->setPosition( 0, 1 );
    EXPECT_TRUE( press( Qt::Key_Delete ) );

    EXPECT_EQ( pt_.getText(), "ab" );
    EXPECT_EQ( pt_.size(), 2U );

    // The edit signal fires once, carrying the exact global offset of the deletion.
    ASSERT_EQ( spy.count(), 1 );
    EXPECT_EQ( spy.takeFirst().at( 0 ).toULongLong(), 1ULL );
}
