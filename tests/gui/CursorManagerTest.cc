/**
 * @file CursorManagerTest.cc
 * @brief Headless unit tests for CursorManager: position state, visibility, and the blink signal.
 */
#include <gtest/gtest.h>

#include <QSignalSpy>

#include "QtTestApp.h"
#include "gui/CursorManager.h"

class CursorManagerTest : public ::testing::Test {
protected:
    auto SetUp() -> void override
    {
        test_support::qtApp();  // QTimer + QSignalSpy::wait need a live event loop
    }
};

TEST_F( CursorManagerTest, PositionState )
{
    CursorManager cursor;
    cursor.setPosition( 3, 7 );
    EXPECT_EQ( cursor.line(), 3 );
    EXPECT_EQ( cursor.col(), 7 );

    cursor.setPosition( 0, 0 );
    EXPECT_EQ( cursor.line(), 0 );
    EXPECT_EQ( cursor.col(), 0 );
}

TEST_F( CursorManagerTest, VisibilityToggle )
{
    CursorManager cursor;
    EXPECT_TRUE( cursor.isVisible() );  // visible by default

    cursor.setVisible( false );
    EXPECT_FALSE( cursor.isVisible() );

    cursor.setVisible( true );
    EXPECT_TRUE( cursor.isVisible() );
}

TEST_F( CursorManagerTest, BlinkingSignal )
{
    CursorManager cursor;
    QSignalSpy spy( &cursor, &CursorManager::blinkToggled );
    ASSERT_TRUE( spy.isValid() );

    // The blink timer fires at 500 ms; spin the event loop until it toggles at least once.
    EXPECT_TRUE( spy.wait( 2000 ) );
    EXPECT_GE( spy.count(), 1 );

    // Stopping the blink halts further toggles.
    cursor.stopBlink();
    const int settled = spy.count();
    EXPECT_FALSE( spy.wait( 700 ) );
    EXPECT_EQ( spy.count(), settled );
}
