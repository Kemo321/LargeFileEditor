/**
 * @file HistoryManager.h
 * @brief Undo/redo snapshot stacks and dirty-state tracking for the PieceTable.
 */

#pragma once

#include <cstdint>
#include <optional>
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
    /**
     * @brief A historical document state: the piece sequence plus the cursor byte offset that
     *        should regain focus when this state is restored.
     *
     * The @ref cursorOffset_ describes the edit transition that produced the state, so the same
     * value drives the cursor jump whether the state is reached by undo or by redo.
     */
    struct Snapshot {
        std::vector<Piece> pieces_;
        uint64_t cursorOffset_{ 0 };
    };

    /**
     * @brief Records @p current as an undo point and clears the redo stack.
     *
     * No-op while a batch operation is in progress. Evicts oldest snapshots to respect the
     * count and total-memory caps (always keeping at least the most recent).
     *
     * @param current The pre-mutation piece list to snapshot.
     * @param cursorOffset Logical byte offset of the edit, restored on undo/redo.
     */
    void recordState( const std::vector<Piece>& current, uint64_t cursorOffset );

    /**
     * @brief Reverts to the previous snapshot.
     * @param current In/out: the live piece list, swapped with the restored snapshot.
     * @return The restored cursor byte offset, or std::nullopt if no undo was available.
     */
    [[nodiscard]] auto undo( std::vector<Piece>& current ) -> std::optional<uint64_t>;

    /**
     * @brief Re-applies the last reverted snapshot.
     * @param current In/out: the live piece list, swapped with the restored snapshot.
     * @return The restored cursor byte offset, or std::nullopt if no redo was available.
     */
    [[nodiscard]] auto redo( std::vector<Piece>& current ) -> std::optional<uint64_t>;

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
