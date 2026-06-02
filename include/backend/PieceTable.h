/**
 * @file PieceTable.h
 * @author Tomasz Okon, Jan Szwagierczak
 * @brief Logic layer header for the Piece Table implementation.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "backend/HistoryManager.h"
#include "backend/MemoryMappedFile.h"
#include "backend/Piece.h"

/**
 * @class PieceTable
 * @brief Efficient data structure for text editing in very large files.
 */
class PieceTable {
public:
    /// Buffer origin of a text piece (see backend/Piece.h).
    using BufferType = ::BufferType;
    /// A contiguous text segment (see backend/Piece.h).
    using Piece = ::Piece;

    /**
     * @brief Default constructor for an empty PieceTable.
     */
    PieceTable();

    /**
     * @brief Constructs a PieceTable by memory-mapping a file.
     * @param filePath Path to the file.
     */
    explicit PieceTable( const std::string& filePath );

    /**
     * @brief Destructor that closes memory maps.
     */
    ~PieceTable();

    PieceTable( const PieceTable& ) = delete;
    auto operator=( const PieceTable& ) -> PieceTable& = delete;

    /// Move constructor: transfers @p other's buffers, pieces and history.
    PieceTable( PieceTable&& other ) noexcept;
    /// Move assignment: transfers @p other's state, leaving it empty.
    auto operator=( PieceTable&& other ) noexcept -> PieceTable&;

    /**
     * @brief Replaces the first occurrence of a pattern.
     * @param pattern The string to search for.
     * @param replacement The string to substitute.
     * @param matchCase True if search is case-sensitive.
     * @param matchWord True if search matches whole words.
     * @return True if a replacement was made.
     */
    auto replaceFirst( const std::string& pattern, const std::string& replacement,
                       bool matchCase = true, bool matchWord = false ) -> bool;

    /**
     * @brief Replaces all occurrences of a pattern.
     * @param pattern The string to search for.
     * @param replacement The string to substitute.
     * @param matchCase True if search is case-sensitive.
     * @param matchWord True if search matches whole words.
     * @return Number of replacements made.
     */
    auto replaceAll( const std::string& pattern, const std::string& replacement,
                     bool matchCase = true, bool matchWord = false ) -> uint64_t;

    /**
     * @brief Replaces all occurrences with progress reporting and cancellation.
     *
     * Single-pass transactional rebuild: on cancel or exception the table is rolled back and 0
     * returned; the document is never left partially mutated.
     *
     * @param pattern The string to search for.
     * @param replacement The string to substitute.
     * @param matchCase True if search is case-sensitive.
     * @param matchWord True if search matches whole words.
     * @param progress Callback invoked periodically with (done, total) match counts.
     * @param cancel Polled cooperatively; when true the operation aborts and rolls back.
     * @return Number of replacements made, or 0 if none / canceled.
     */
    auto replaceAll( const std::string& pattern, const std::string& replacement, bool matchCase,
                     bool matchWord,
                     const std::function<void( uint64_t done, uint64_t total )>& progress,
                     const std::atomic<bool>& cancel ) -> uint64_t;

    /**
     * @brief Retrieves the total logical size of the text.
     * @return Size in bytes.
     */
    [[nodiscard]] auto size() const -> uint64_t;

    /**
     * @brief Reconstructs the entire text into a string.
     * @return The complete document text.
     */
    [[nodiscard]] auto getText() const -> std::string;

    /**
     * @brief Finds all logical positions of a substring.
     * @param pattern The string to search for.
     * @param matchCase True if search is case-sensitive.
     * @param matchWord True if search matches whole words.
     * @return Vector of starting byte positions.
     */
    [[nodiscard]] auto findAll( const std::string& pattern, bool matchCase = true,
                                bool matchWord = false ) const -> std::vector<uint64_t>;

    /**
     * @brief Finds all logical positions of a substring, with progress and cancellation.
     *
     * The scan polls @p cancel cooperatively and returns an empty vector if canceled mid-scan.
     *
     * @param pattern The string to search for.
     * @param matchCase True if search is case-sensitive.
     * @param matchWord True if search matches whole words.
     * @param progress Callback invoked periodically with (bytesScanned, totalBytes).
     * @param cancel Polled cooperatively; when true the search aborts and returns empty.
     * @return Vector of starting byte positions, or empty if canceled.
     */
    [[nodiscard]] auto findAll( const std::string& pattern, bool matchCase, bool matchWord,
                                const std::function<void( uint64_t, uint64_t )>& progress,
                                const std::atomic<bool>& cancel ) const -> std::vector<uint64_t>;

    /**
     * @brief Retrieves a specific substring.
     * @param position Starting logical byte position.
     * @param length Number of bytes to retrieve.
     * @return The requested substring.
     */
    [[nodiscard]] auto getSubstr( uint64_t position, uint64_t length ) const -> std::string;

    /**
     * @brief Retrieves fragment metadata for a specific range.
     * @param position Starting logical byte position.
     * @param length Length of the range.
     * @return Vector of intersecting pieces.
     */
    [[nodiscard]] auto getFragmentsInRange( uint64_t position, uint64_t length ) const
        -> std::vector<Piece>;

    /**
     * @brief Inserts text at the specified logical position.
     * @param position Byte position to insert at.
     * @param text String to insert.
     */
    auto insert( uint64_t position, const std::string& text ) -> void;

    /**
     * @brief Removes text from the specified logical position.
     * @param position Starting byte position.
     * @param length Number of bytes to remove.
     */
    auto remove( uint64_t position, uint64_t length ) -> void;

    /**
     * @brief Saves the active document state to a file.
     * @param filePath Destination file path.
     * @return True if save was successful.
     */
    [[nodiscard]] auto saveToFile( const std::string& filePath ) const -> bool;

    /**
     * @brief Reverts the last text modification.
     * @return The logical byte offset to refocus the cursor on (clamped to the document size),
     *         or std::nullopt if there was nothing to undo.
     */
    auto undo() -> std::optional<uint64_t>;

    /**
     * @brief Reapplies the last reverted text modification.
     * @return The logical byte offset to refocus the cursor on (clamped to the document size),
     *         or std::nullopt if there was nothing to redo.
     */
    auto redo() -> std::optional<uint64_t>;

    /**
     * @brief Checks if an undo operation is available.
     * @return True if undo stack is not empty.
     */
    [[nodiscard]] auto canUndo() const -> bool
    {
        return history_.canUndo();
    }

    /**
     * @brief Checks if a redo operation is available.
     * @return True if redo stack is not empty.
     */
    [[nodiscard]] auto canRedo() const -> bool
    {
        return history_.canRedo();
    }

    /**
     * @brief Checks if the document has unsaved modifications.
     * @return True if state differs from last save.
     */
    [[nodiscard]] auto isDirty() const -> bool
    {
        return history_.isDirty();
    }

private:
    // Shared KMP scan backing findAll/replaceAll; resolves pieces_ into spans for KmpSearch.
    [[nodiscard]] auto findAllImpl( const std::string& pattern, bool matchCase, bool matchWord,
                                    const std::function<void( uint64_t, uint64_t )>& progress,
                                    const std::atomic<bool>& cancel ) const
        -> std::vector<uint64_t>;

    struct FindResult {
        size_t pieceIndex_;
        uint64_t offsetInPiece_;
    };

    [[nodiscard]] auto findPieceAt( uint64_t position ) const -> FindResult;
    auto splitPiece( size_t pieceIndex, uint64_t offset ) -> void;

    // Rebuilds pieceStartOffsets_ prefix sums and total_size_; call after any change to pieces_.
    auto rebuildOffsetIndex() -> void;

    // Merges adjacent pieces referencing contiguous spans of the same buffer (length-preserving).
    auto coalescePieces() -> void;

    MemoryMappedFile mappedFile_;

    std::string addBuffer_;
    std::vector<Piece> pieces_;

    // Prefix sums of piece lengths (pieceStartOffsets_.back() == total_size_); enables O(log n)
    // findPieceAt via binary search.
    std::vector<uint64_t> pieceStartOffsets_{ 0 };

    uint64_t total_size_{ 0 };  // cached sum of piece lengths; size() returns this in O(1)

    // Mutable so const saveToFile() can update the saved-marker (history bookkeeping only).
    mutable HistoryManager history_;
};
