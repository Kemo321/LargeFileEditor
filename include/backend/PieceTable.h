/**
 * @file PieceTable.h
 * @author Tomasz Okon
 * @brief Logic layer header for the Piece Table implementation.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/**
 * @class PieceTable
 * @brief Efficient data structure for text editing in very large files.
 */
class PieceTable {
public:
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

    PieceTable( PieceTable&& other ) noexcept;
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
     * Single-pass, transactional rebuild: the document is never mutated until the
     * operation completes. If @p cancel becomes true (or an exception occurs) the
     * PieceTable is left exactly as it was before the call (rollback) and 0 is returned.
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
     * @return True if undo was successful.
     */
    auto undo() -> bool;

    /**
     * @brief Reapplies the last reverted text modification.
     * @return True if redo was successful.
     */
    auto redo() -> bool;

    /**
     * @brief Checks if an undo operation is available.
     * @return True if undo stack is not empty.
     */
    [[nodiscard]] auto canUndo() const -> bool
    {
        return !undoStack_.empty();
    }

    /**
     * @brief Checks if a redo operation is available.
     * @return True if redo stack is not empty.
     */
    [[nodiscard]] auto canRedo() const -> bool
    {
        return !redoStack_.empty();
    }

    /**
     * @brief Checks if the document has unsaved modifications.
     * @return True if state differs from last save.
     */
    [[nodiscard]] auto isDirty() const -> bool
    {
        return undoStack_.size() != lastSavedUndoSize_;
    }

    /**
     * @brief Calculates the total number of lines in the document.
     * @return Line count.
     */
    // [[nodiscard]] auto getLineCount() const -> int;

    /**
     * @brief Finds the starting byte position of a given line.
     * @param line Zero-based line index.
     * @return Logical byte position.
     */
    // [[nodiscard]] auto getLineStart( int line ) const -> uint64_t;

    /**
     * @brief Determines the line number for a specific byte position.
     * @param position Logical byte position.
     * @return Zero-based line index.
     */
    // [[nodiscard]] auto getLineFromPosition( uint64_t position ) const -> int;

private:
    [[nodiscard]] static auto computeLPS( const std::string& pattern ) -> std::vector<int>;

    // Shared KMP scan backing findAll/replaceAll. Reports byte-based progress and
    // aborts (returns the matches found so far is NOT done — returns empty) when cancel is set.
    [[nodiscard]] auto findAllImpl( const std::string& pattern, bool matchCase, bool matchWord,
                                    const std::function<void( uint64_t, uint64_t )>& progress,
                                    const std::atomic<bool>& cancel ) const
        -> std::vector<uint64_t>;

    std::vector<std::vector<Piece>> undoStack_;
    std::vector<std::vector<Piece>> redoStack_;

    auto saveState() -> void;

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

    bool isBatchOperation_{ false };
    uint64_t lastSavedUndoSize_{ 0 };
};
