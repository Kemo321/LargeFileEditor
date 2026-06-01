/**
 * @file Piece.h
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
    BufferType type_;
    uint64_t start_;
    uint64_t length_;
};
