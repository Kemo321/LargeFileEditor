#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class PieceTable;

/**
 * @class LineManager
 * @author Jan Szwagierczak
 * @brief Maps "Virtual Line Numbers" to "Global Byte Offsets" for the PieceTable.
 * Implements "Hard Soft-Wrap" to avoid freezing on extremely long lines.
 */
class LineManager {
public:
    /// Binds to @p pt; lines longer than @p max_visual_line_length are hard-wrapped.
    explicit LineManager( PieceTable* pt, int max_visual_line_length = 4096 );

    /**
     * @brief Gets the global offset for the start of the given virtual line.
     * Calculates lazily if the line hasn't been mapped yet.
     */
    auto getLineOffset( int virtual_line ) -> uint64_t;

    /**
     * @brief Gets the virtual line number that contains the given global offset.
     * Calculates lazily if the offset hasn't been mapped yet.
     */
    auto getVirtualLineFromOffset( uint64_t offset ) -> int;

    /**
     * @brief Gets the estimated total number of virtual lines.
     * Uncalculated portions are estimated from the average line length.
     */
    auto getLineCount() -> int;

    /**
     * @brief True if @p virtual_line begins a real, newline-delimited logical line.
     * Hard-wrapped continuation segments of a long line return false so the gutter can blank them.
     */
    auto isLogicalLineStart( int virtual_line ) -> bool;

    /**
     * @brief 1-based logical (physical) line number for @p virtual_line.
     * Wrapped continuation segments share the number of the logical line they belong to.
     */
    auto getLogicalLineNumber( int virtual_line ) -> int;

    /**
     * @brief Byte column within the logical line for cursor (@p virtual_line, @p col).
     * Folds the lengths of any preceding wrapped segments back in, so a cursor on a continuation
     * reports its true column in the physical line rather than the segment-local one.
     */
    auto getLogicalColumn( int virtual_line, int col ) -> uint64_t;

    /**
     * @brief Invalidates the cache from a specific global offset downwards.
     */
    void invalidateCacheFromOffset( uint64_t offset );

    /**
     * @brief Clears the entire cache.
     */
    void reset();

    /**
     * @brief Helper to get the length of a virtual line.
     */
    auto getVirtualLineLength( int virtual_line ) -> uint64_t;

    /**
     * @brief Gets the maximum line length encountered so far.
     */
    [[nodiscard]] auto getGlobalMaxLineLength() const -> uint64_t;

    /**
     * @brief Gets a chunk of a line, ensuring UTF-8 boundaries.
     */
    [[nodiscard]] auto getLineChunk( int virtual_line, uint64_t start_col,
                                     uint64_t length ) -> std::string;

private:
    void ensureLineCalculated( int target_line );
    void ensureOffsetCalculated( uint64_t target_offset );

    PieceTable* pt_;
    int max_visual_line_length_;
    uint64_t global_max_line_length_{ 0 };
    // Max offset scanned by ensureOffsetCalculated.
    // calculated_up_to_ == size means EOF reached without pushing a phantom trailing line.
    uint64_t calculated_up_to_{ 0 };

    // Cache: virtual line index -> global byte offset
    std::vector<uint64_t> line_start_offsets_;
    // Parallel to line_start_offsets_: virtual line index -> 0-based logical line number. Wrapped
    // continuation segments repeat the previous entry; real (\n-delimited) lines increment it.
    std::vector<int> logical_line_numbers_;
    std::mutex cache_mutex_;
};
