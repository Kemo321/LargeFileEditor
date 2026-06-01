/**
 * @file HistoryManagerTest.cc
 * @brief Unit tests for the HistoryManager undo/redo stacks.
 */
#include <gtest/gtest.h>

#include <vector>

#include "backend/HistoryManager.h"
#include "backend/Piece.h"

namespace {

auto makeSnapshot( uint64_t marker ) -> std::vector<Piece>
{
    return { { BufferType::Add, marker, 1 } };
}

}  // namespace

TEST( HistoryManagerTest, UndoRedoRoundTrip )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );

    history.recordState( live, 0 );  // record state "0"
    live = makeSnapshot( 1 );        // mutate to "1"

    EXPECT_TRUE( history.canUndo() );
    ASSERT_TRUE( history.undo( live ) );
    EXPECT_EQ( live[0].start_, 0U );  // restored to "0"

    EXPECT_TRUE( history.canRedo() );
    ASSERT_TRUE( history.redo( live ) );
    EXPECT_EQ( live[0].start_, 1U );  // re-applied "1"
}

TEST( HistoryManagerTest, UndoRedoRestoresCursorOffset )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );

    constexpr uint64_t kEditOffset = 42;
    history.recordState( live, kEditOffset );  // edit occurred at byte 42
    live = makeSnapshot( 1 );

    // Undo returns the offset of the edit it reverts; redo restores the same focus.
    auto undoOffset = history.undo( live );
    ASSERT_TRUE( undoOffset.has_value() );
    EXPECT_EQ( *undoOffset, kEditOffset );

    auto redoOffset = history.redo( live );
    ASSERT_TRUE( redoOffset.has_value() );
    EXPECT_EQ( *redoOffset, kEditOffset );
}

TEST( HistoryManagerTest, UndoRedoOnEmptyReturnsNullopt )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );
    EXPECT_FALSE( history.canUndo() );
    EXPECT_FALSE( history.undo( live ).has_value() );
    EXPECT_FALSE( history.canRedo() );
    EXPECT_FALSE( history.redo( live ).has_value() );
}

TEST( HistoryManagerTest, RecordStateClearsRedo )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );

    history.recordState( live, 0 );
    live = makeSnapshot( 1 );
    ASSERT_TRUE( history.undo( live ) );  // redo now has "1"
    EXPECT_TRUE( history.canRedo() );

    history.recordState( live, 0 );  // a new edit must wipe the redo stack
    EXPECT_FALSE( history.canRedo() );
}

TEST( HistoryManagerTest, DirtyTracking )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );

    EXPECT_FALSE( history.isDirty() );
    history.recordState( live, 0 );
    EXPECT_TRUE( history.isDirty() );

    history.markSaved();
    EXPECT_FALSE( history.isDirty() );

    history.recordState( live, 0 );
    EXPECT_TRUE( history.isDirty() );
}

TEST( HistoryManagerTest, BatchOperationSuppressesRecord )
{
    HistoryManager history;
    std::vector<Piece> live = makeSnapshot( 0 );

    history.setBatchOperation( true );
    EXPECT_TRUE( history.isBatchOperation() );
    history.recordState( live, 0 );
    EXPECT_FALSE( history.canUndo() );  // nothing recorded during a batch

    history.setBatchOperation( false );
    history.recordState( live, 0 );
    EXPECT_TRUE( history.canUndo() );
}
