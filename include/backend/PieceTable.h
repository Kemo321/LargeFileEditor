/**
 * Authors: Tomasz Okon
 * Description: Header of the logic layer (backend) - Piece Table implementation.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief A data structure for zero-copy, efficient editing of large texts.
 */
class PieceTable {
public:
    /**
     * @brief Identifies which buffer a piece refers to.
     */
    enum class BufferType { Original, Add };

    /**
     * @brief Represents a fragment of the document.
     */
    struct Piece {
        BufferType type_;
        uint64_t start_;
        uint64_t length_;
    };

    /**
     * @brief Default constructor creating an empty document.
     */
    PieceTable();

    /**
     * @brief Constructor initializing the table by mapping a file from disk.
     * @param filePath Path to the file to be memory-mapped.
     */
    explicit PieceTable( const std::string& filePath );

    /**
     * @brief Destructor ensuring proper unmapping of the file and closing descriptors.
     */
    ~PieceTable();

    // Disable copy semantics to prevent double unmapping of the same file descriptor
    PieceTable( const PieceTable& ) = delete;
    auto operator=( const PieceTable& ) -> PieceTable& = delete;

    // Enable move semantics (optional, but recommended for modern C++)
    PieceTable( PieceTable&& ) noexcept = default;
    auto operator=( PieceTable&& ) noexcept -> PieceTable& = default;

    /**
     * @brief Returns the total document size in characters.
     */
    [[nodiscard]] auto size() const -> uint64_t;

    /**
     * @brief Retrieves the entire current document content.
     */
    [[nodiscard]] auto getText() const -> std::string;

    /**
     * @brief Searches for all occurrences of a pattern using the KMP algorithm.
     * @param pattern The string to search for.
     * @return Vector of logical positions where the pattern starts.
     */
    [[nodiscard]] auto findAll( const std::string& pattern ) const -> std::vector<uint64_t>;

    /**
     * @brief Retrieves a fragment of the document without copying the entire file.
     * @param position Starting logical position.
     * @param length Number of characters to retrieve.
     * @return A string containing only the requested part.
     */
    [[nodiscard]] auto getSubstr( uint64_t position, uint64_t length ) const -> std::string;

    /**
     * @brief Inserts new text at the given position.
     */
    auto insert( uint64_t position, const std::string& text ) -> void;

    /**
     * @brief Removes a fragment of text from the document.
     */
    auto remove( uint64_t position, uint64_t length ) -> void;

    /**
     * @brief Saves the current document state to a file.
     * @param filePath Path where the file should be saved.
     * @return True if successful.
     */
    [[nodiscard]] auto saveToFile( const std::string& filePath ) const -> bool;

private:
    /**
     * @brief Computes the Longest Prefix Suffix (LPS) array for the KMP algorithm.
     */
    [[nodiscard]] static auto computeLPS( const std::string& pattern ) -> std::vector<int>;

    /**
     * @brief Result of finding a piece at a specific logical position.
     */
    struct FindResult {
        size_t pieceIndex_;
        uint64_t offsetInPiece_;
    };

    [[nodiscard]] auto findPieceAt( uint64_t position ) const -> FindResult;
    auto splitPiece( size_t pieceIndex, uint64_t offset ) -> void;

    auto openMmap( const std::string& filePath ) -> void;
    auto closeMmap() -> void;

    const char* originalBuffer_{ nullptr };
    uint64_t mmapSize_{ 0 };
    int fileDescriptor_{ -1 };

    std::string addBuffer_;
    std::vector<Piece> pieces_;
};
