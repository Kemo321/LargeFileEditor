/**
 * @file KmpSearch.h
 * @brief Knuth-Morris-Pratt substring search over a sequence of memory spans.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * @class KmpSearch
 * @brief Stateless KMP matcher operating on the raw buffers behind a piece sequence.
 *
 * Decoupled from PieceTable: callers resolve their pieces into ordered @ref Span values and
 * supply a random-access byte accessor for whole-word boundary checks.
 */
class KmpSearch {
public:
    /// A contiguous run of bytes (one resolved piece). @c data may be nullptr (skipped).
    struct Span {
        const char* data;
        uint64_t length;
    };

    using ProgressFn = std::function<void( uint64_t done, uint64_t total )>;

    /**
     * @brief Builds the KMP longest-proper-prefix-suffix table for @p pattern.
     */
    [[nodiscard]] static auto computeLPS( const std::string& pattern ) -> std::vector<int>;

    /**
     * @brief Finds all non-overlapping match positions across @p spans.
     *
     * @param spans Ordered spans forming the logical document (a null @c data span only
     *              advances the logical position).
     * @param totalBytes Total logical size (sum of span lengths).
     * @param pattern The substring to find.
     * @param matchCase Case-sensitive when true.
     * @param matchWord Whole-word matching when true.
     * @param byteAt Random single-byte accessor for word-boundary checks.
     * @param progress Periodic (bytesScanned, totalBytes) callback.
     * @param cancel Polled cooperatively; when set the search aborts and returns empty.
     * @return Ascending logical start positions of matches, or empty if canceled.
     */
    [[nodiscard]] auto findAll( const std::vector<Span>& spans, uint64_t totalBytes,
                                const std::string& pattern, bool matchCase, bool matchWord,
                                const std::function<char( uint64_t )>& byteAt,
                                const ProgressFn& progress,
                                const std::atomic<bool>& cancel ) const -> std::vector<uint64_t>;
};
