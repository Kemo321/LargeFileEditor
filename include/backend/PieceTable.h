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

    // Custom move semantics to prevent double unmapping of the same file descriptor
    PieceTable( PieceTable&& other ) noexcept;
    auto operator=( PieceTable&& other ) noexcept -> PieceTable&;

    /**
     * @brief Replaces the first occurrence of a pattern with new text.
     */
    auto replaceFirst( const std::string& pattern, const std::string& replacement ) -> bool;

    /**
     * @brief Replaces all occurrences of a pattern with new text.
     * @return Number of replacements made.
     */
    auto replaceAll( const std::string& pattern, const std::string& replacement ) -> uint64_t;

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
     * @brief For a given logical range, returns which parts come from Original and which from Add
     * buffer. Useful for syntax highlighting or specialized rendering in the GUI.
     */
    [[nodiscard]] auto getFragmentsInRange( uint64_t position, uint64_t length ) const
        -> std::vector<Piece>;

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

    /**
     * @brief Reverts the last modification by restoring the previous piece configuration.
     * @return true if the undo operation was successful, false if the history is empty.
     */
    auto undo() -> bool;

    /**
     * @brief Re-applies a previously undone modification.
     * @return true if the redo operation was successful, false if there are no states to redo.
     */
    auto redo() -> bool;

    /**
     * @brief Checks if there are any actions available to undo.
     */
    [[nodiscard]] auto canUndo() const -> bool
    {
        return !undoStack_.empty();
    }

    /**
     * @brief Checks if there are any actions available to redo.
     */
    [[nodiscard]] auto canRedo() const -> bool
    {
        return !redoStack_.empty();
    }

    /**
     * @brief Checks if the document has been modified since the last save.
     */
    [[nodiscard]] auto isDirty() const -> bool
    {
        return undoStack_.size() != lastSavedUndoSize_;
    }

    /**
     * @brief Returns a list of logical positions of all line breaks.
     * Useful for mapping vertical scrollbar to line numbers.
     */
    [[nodiscard]] auto getLineOffsets() const -> std::vector<uint64_t>;

private:
    /**
     * @brief Computes the Longest Prefix Suffix (LPS) array for the KMP algorithm.
     */
    [[nodiscard]] static auto computeLPS( const std::string& pattern ) -> std::vector<int>;

    /** * @brief Stacks for storing the state of the piece vector.
     * We store only the vector of Pieces (pointers), which makes snapshots
     * extremely memory-efficient even for massive files.
     */
    std::vector<std::vector<Piece>> undoStack_;
    std::vector<std::vector<Piece>> redoStack_;

    /**
     * @brief Saves the current configuration of pieces to the undo stack.
     * This method is called before any modifying operation (insert, remove, replace).
     * It also clears the redo stack to maintain linear history consistency.
     */
    auto saveState() -> void;

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
    bool isBatchOperation_{ false };
    uint64_t lastSavedUndoSize_{ 0 };
};
