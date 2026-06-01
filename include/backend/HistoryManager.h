/**
 * @file HistoryManager.h
 * @brief Undo/redo snapshot stacks and dirty-state tracking for the PieceTable.
 */

#pragma once

#include <cstdint>
#include <vector>

#include "backend/Piece.h"

/**
 * @class HistoryManager
 * @brief Owns the undo/redo history of piece-list snapshots and the save/dirty bookkeeping.
 *
 * The PieceTable hands its current piece list to @ref recordState before each mutation and
 * swaps it through @ref undo / @ref redo, keeping all history policy (count cap, memory cap,
 * batch suppression) in one place.
 */
class HistoryManager {
public:
    using Snapshot = std::vector<Piece>;

    /**
     * @brief Records @p current as an undo point and clears the redo stack.
     *
     * No-op while a batch operation is in progress. Evicts oldest snapshots to respect the
     * count and total-memory caps (always keeping at least the most recent).
     */
    void recordState( const Snapshot& current );

    /**
     * @brief Reverts to the previous snapshot.
     * @param current In/out: the live piece list, swapped with the restored snapshot.
     * @return True if an undo was performed.
     */
    [[nodiscard]] auto undo( Snapshot& current ) -> bool;

    /**
     * @brief Re-applies the last reverted snapshot.
     * @param current In/out: the live piece list, swapped with the restored snapshot.
     * @return True if a redo was performed.
     */
    [[nodiscard]] auto redo( Snapshot& current ) -> bool;

    [[nodiscard]] auto canUndo() const -> bool;
    [[nodiscard]] auto canRedo() const -> bool;

    /// True if the document changed since the last @ref markSaved.
    [[nodiscard]] auto isDirty() const -> bool;

    /// Marks the current history depth as the saved state.
    void markSaved();

    /// Suppresses @ref recordState while a multi-step batch operation runs.
    void setBatchOperation( bool on );
    [[nodiscard]] auto isBatchOperation() const -> bool;

private:
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    uint64_t lastSavedUndoSize_{ 0 };
    bool isBatchOperation_{ false };
};
