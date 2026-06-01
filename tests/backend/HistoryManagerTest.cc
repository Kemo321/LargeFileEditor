/**
 * @file HistoryManagerTest.cc
 * @brief Unit tests for the HistoryManager undo/redo stacks.
 */
#include <gtest/gtest.h>

#include <cstdint>
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

TEST( HistoryManagerTest, MemoryCapEviction )
{
    // Undo memory is capped at 256 MiB by evicting oldest snapshots (keeping >=1). Six ~64 MiB
    // snapshots (~384 MiB) must force eviction down to the cap.
    constexpr uint64_t kMaxUndoBytes = 256ULL * 1024ULL * 1024ULL;
    const uint64_t snapshotBytes = 64ULL * 1024ULL * 1024ULL;
    const auto piecesPerSnapshot = static_cast<size_t>( snapshotBytes / sizeof( Piece ) );
    constexpr int kPushes = 6;

    HistoryManager history;
    for( int i = 0; i < kPushes; ++i ) {
        std::vector<Piece> live( piecesPerSnapshot,
                                 Piece{ BufferType::Add, static_cast<uint64_t>( i ), 1 } );
        history.recordState( live, static_cast<uint64_t>( i ) );
    }

    EXPECT_TRUE( history.canUndo() );  // most recent state survived eviction

    // Drain the undo stack to count retained snapshots.
    std::vector<Piece> scratch;
    int retained = 0;
    while( history.undo( scratch ).has_value() ) {
        ++retained;
    }

    EXPECT_GE( retained, 1 );        // never evict the last surviving state
    EXPECT_LT( retained, kPushes );  // oldest snapshots were dropped

    // Retained aggregate respects the memory cap.
    EXPECT_LE( static_cast<uint64_t>( retained ) * piecesPerSnapshot * sizeof( Piece ),
               kMaxUndoBytes );
}
