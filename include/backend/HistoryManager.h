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
 * Centralizes history policy (count cap, memory cap, batch suppression) for the PieceTable.
 */
class HistoryManager {
public:
    /**
     * @brief A historical document state: the piece sequence plus the cursor byte offset to
     *        refocus when this state is restored (same value drives both undo and redo jumps).
     */
    struct Snapshot {
        std::vector<Piece> pieces_;   ///< Piece sequence of this document state.
        uint64_t cursorOffset_{ 0 };  ///< Byte offset to refocus on restore.
    };

    /**
     * @brief Records @p current as an undo point and clears the redo stack.
     *
     * No-op during a batch operation. Evicts oldest snapshots to respect the count/memory caps.
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

    /// True if an undo is available.
    [[nodiscard]] auto canUndo() const -> bool;
    /// True if a redo is available.
    [[nodiscard]] auto canRedo() const -> bool;

    /// True if the document changed since the last @ref markSaved.
    [[nodiscard]] auto isDirty() const -> bool;

    /// Marks the current history depth as the saved state.
    void markSaved();

    /// Suppresses @ref recordState while a multi-step batch operation runs.
    void setBatchOperation( bool on );
    /// True while a batch operation is suppressing recording.
    [[nodiscard]] auto isBatchOperation() const -> bool;

private:
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    uint64_t lastSavedUndoSize_{ 0 };
    bool isBatchOperation_{ false };
};
