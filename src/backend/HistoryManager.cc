// Author: Tomasz Okon, Jan Szwagierczak

#include "backend/HistoryManager.h"

#include <numeric>

static constexpr size_t kMaxUndoHistory = 100;
static constexpr uint64_t kMaxUndoBytes = 256ULL * 1024ULL * 1024ULL;

void HistoryManager::recordState( const std::vector<Piece>& current, uint64_t cursorOffset )
{
    if( isBatchOperation_ ) {
        return;
    }
    undoStack_.push_back( { current, cursorOffset } );
    redoStack_.clear();

    if( undoStack_.size() > kMaxUndoHistory ) {
        undoStack_.erase( undoStack_.begin() );
    }

    // Memory guard: cap total undo bytes by evicting oldest snapshots (keep at least one).
    uint64_t undoBytes = std::accumulate(
        undoStack_.begin(), undoStack_.end(), 0ULL,
        []( uint64_t acc, const Snapshot& snapshot ) -> uint64_t {
            return acc + ( static_cast<uint64_t>( snapshot.pieces_.size() ) * sizeof( Piece ) );
        } );

    while( undoStack_.size() > 1 && undoBytes > kMaxUndoBytes ) {
        undoBytes -= static_cast<uint64_t>( undoStack_.front().pieces_.size() ) * sizeof( Piece );
        undoStack_.erase( undoStack_.begin() );
    }
}

auto HistoryManager::undo( std::vector<Piece>& current ) -> std::optional<uint64_t>
{
    if( undoStack_.empty() ) {
        return std::nullopt;
    }
    Snapshot restored = std::move( undoStack_.back() );
    undoStack_.pop_back();
    // Reuse the edit offset for the redo entry so a later redo refocuses identically.
    redoStack_.push_back( { std::move( current ), restored.cursorOffset_ } );
    current = std::move( restored.pieces_ );
    return restored.cursorOffset_;
}

auto HistoryManager::redo( std::vector<Piece>& current ) -> std::optional<uint64_t>
{
    if( redoStack_.empty() ) {
        return std::nullopt;
    }
    Snapshot restored = std::move( redoStack_.back() );
    redoStack_.pop_back();
    undoStack_.push_back( { std::move( current ), restored.cursorOffset_ } );
    current = std::move( restored.pieces_ );
    return restored.cursorOffset_;
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
