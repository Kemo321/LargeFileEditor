/**
 * @file Piece.h
 * @author Tomasz Okon
 * @brief Core value types shared by the PieceTable and its collaborators.
 */

#pragma once

#include <cstdint>

/**
 * @enum BufferType
 * @brief Identifies the buffer origin for a specific text piece.
 */
enum class BufferType : std::uint8_t { Original, Add };

/**
 * @struct Piece
 * @brief Represents a continuous segment of text in one of the buffers.
 */
struct Piece {
    BufferType type_;  ///< Which buffer this segment references.
    uint64_t start_;   ///< Start offset within that buffer.
    uint64_t length_;  ///< Segment length in bytes.
};
