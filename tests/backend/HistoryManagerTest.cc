/**
 * @file HistoryManagerTest.cc
 * @brief Unit tests for the HistoryManager undo/redo stacks.
 */
#include <gtest/gtest.h>

#include <vector>

#include "backend/HistoryManager.h"
#include "backend/Piece.h"

namespace {

auto makeSnapshot( uint64_t marker ) -> HistoryManager::Snapshot
{
    return { { BufferType::Add, marker, 1 } };
}

}  // namespace

TEST( HistoryManagerTest, UndoRedoRoundTrip )
{
    HistoryManager history;
    HistoryManager::Snapshot live = makeSnapshot( 0 );

    history.recordState( live );  // record state "0"
    live = makeSnapshot( 1 );     // mutate to "1"

    EXPECT_TRUE( history.canUndo() );
    ASSERT_TRUE( history.undo( live ) );
    EXPECT_EQ( live[0].start_, 0U );  // restored to "0"

    EXPECT_TRUE( history.canRedo() );
    ASSERT_TRUE( history.redo( live ) );
    EXPECT_EQ( live[0].start_, 1U );  // re-applied "1"
}

TEST( HistoryManagerTest, UndoRedoOnEmptyReturnsFalse )
{
    HistoryManager history;
    HistoryManager::Snapshot live = makeSnapshot( 0 );
    EXPECT_FALSE( history.canUndo() );
    EXPECT_FALSE( history.undo( live ) );
    EXPECT_FALSE( history.canRedo() );
    EXPECT_FALSE( history.redo( live ) );
}

TEST( HistoryManagerTest, RecordStateClearsRedo )
{
    HistoryManager history;
    HistoryManager::Snapshot live = makeSnapshot( 0 );

    history.recordState( live );
    live = makeSnapshot( 1 );
    ASSERT_TRUE( history.undo( live ) );  // redo now has "1"
    EXPECT_TRUE( history.canRedo() );

    history.recordState( live );  // a new edit must wipe the redo stack
    EXPECT_FALSE( history.canRedo() );
}

TEST( HistoryManagerTest, DirtyTracking )
{
    HistoryManager history;
    HistoryManager::Snapshot live = makeSnapshot( 0 );

    EXPECT_FALSE( history.isDirty() );
    history.recordState( live );
    EXPECT_TRUE( history.isDirty() );

    history.markSaved();
    EXPECT_FALSE( history.isDirty() );

    history.recordState( live );
    EXPECT_TRUE( history.isDirty() );
}

TEST( HistoryManagerTest, BatchOperationSuppressesRecord )
{
    HistoryManager history;
    HistoryManager::Snapshot live = makeSnapshot( 0 );

    history.setBatchOperation( true );
    EXPECT_TRUE( history.isBatchOperation() );
    history.recordState( live );
    EXPECT_FALSE( history.canUndo() );  // nothing recorded during a batch

    history.setBatchOperation( false );
    history.recordState( live );
    EXPECT_TRUE( history.canUndo() );
}
