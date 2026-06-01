#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class PieceTable;

/**
 * @class LineManager
 * @brief Maps "Virtual Line Numbers" to "Global Byte Offsets" for the PieceTable.
 * Implements "Hard Soft-Wrap" to avoid freezing on extremely long lines.
 */
class LineManager {
public:
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

    // Cache: virtual line index -> global byte offset
    std::vector<uint64_t> line_start_offsets_;
    std::mutex cache_mutex_;
};
