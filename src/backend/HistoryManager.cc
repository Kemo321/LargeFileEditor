#include "backend/HistoryManager.h"

#include <numeric>

static constexpr size_t kMaxUndoHistory = 100;
static constexpr uint64_t kMaxUndoBytes = 256ULL * 1024ULL * 1024ULL;

void HistoryManager::recordState( const Snapshot& current )
{
    if( isBatchOperation_ ) {
        return;
    }
    undoStack_.push_back( current );
    redoStack_.clear();

    if( undoStack_.size() > kMaxUndoHistory ) {
        undoStack_.erase( undoStack_.begin() );
    }

    // Memory guard: snapshots are full copies of the piece list, which can hold millions of
    // entries after a large replaceAll. Cap total undo memory by evicting oldest snapshots
    // (keeping at least the most recent) so editing a heavily fragmented document cannot
    // exhaust RAM.
    uint64_t undoBytes = std::accumulate(
        undoStack_.begin(), undoStack_.end(), 0ULL,
        []( uint64_t acc, const Snapshot& snapshot ) -> uint64_t {
            return acc + ( static_cast<uint64_t>( snapshot.size() ) * sizeof( Piece ) );
        } );

    while( undoStack_.size() > 1 && undoBytes > kMaxUndoBytes ) {
        undoBytes -= static_cast<uint64_t>( undoStack_.front().size() ) * sizeof( Piece );
        undoStack_.erase( undoStack_.begin() );
    }
}

auto HistoryManager::undo( Snapshot& current ) -> bool
{
    if( undoStack_.empty() ) {
        return false;
    }
    redoStack_.push_back( current );
    current = undoStack_.back();
    undoStack_.pop_back();
    return true;
}

auto HistoryManager::redo( Snapshot& current ) -> bool
{
    if( redoStack_.empty() ) {
        return false;
    }
    undoStack_.push_back( current );
    current = redoStack_.back();
    redoStack_.pop_back();
    return true;
}

auto HistoryManager::canUndo() const -> bool
{
    return !undoStack_.empty();
}

auto HistoryManager::canRedo() const -> bool
{
    return !redoStack_.empty();
}

auto HistoryManager::isDirty() const -> bool
{
    return undoStack_.size() != lastSavedUndoSize_;
}

void HistoryManager::markSaved()
{
    lastSavedUndoSize_ = undoStack_.size();
}

void HistoryManager::setBatchOperation( bool on )
{
    isBatchOperation_ = on;
}

auto HistoryManager::isBatchOperation() const -> bool
{
    return isBatchOperation_;
}
