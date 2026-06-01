/**
 * @file Utf8Utils.h
 * @brief Low-level UTF-8 byte parsing helpers shared across the editor.
 */

#pragma once

#include <cstdint>
#include <functional>

/**
 * @namespace Utf8Utils
 * @brief Stateless helpers for inspecting UTF-8 byte sequences and snapping logical
 *        positions to character boundaries.
 */
namespace Utf8Utils {

/// Maximum number of bytes in a UTF-8 encoded code point.
inline constexpr int kMaxSequenceLength = 4;

/**
 * @brief Checks whether a byte is a UTF-8 continuation byte (10xxxxxx).
 * @param byte The byte to inspect.
 * @return True if the byte is a continuation byte.
 */
[[nodiscard]] auto isContinuationByte( unsigned char byte ) -> bool;

/**
 * @brief Returns the length (in bytes) of the UTF-8 sequence introduced by a lead byte.
 * @param leadByte The first byte of a UTF-8 sequence.
 * @return 2, 3 or 4 for multibyte lead bytes; 1 otherwise (ASCII or invalid lead).
 */
[[nodiscard]] auto sequenceLength( unsigned char leadByte ) -> int;

/**
 * @brief Snaps a logical position backward to the start of the UTF-8 character it lands in.
 *
 * Walks backward over continuation bytes, examining at most kMaxSequenceLength-1 preceding
 * bytes and never stepping below @p floor.
 *
 * @param byteAt Callback returning the byte at a given logical position.
 * @param floor Lowest logical position the search may reach.
 * @param position Logical position to snap.
 * @return The snapped position (<= @p position, >= @p floor).
 */
[[nodiscard]] auto snapToCharacterBoundary( const std::function<unsigned char( uint64_t )>& byteAt,
                                            uint64_t floor, uint64_t position ) -> uint64_t;

}  // namespace Utf8Utils
