/**
 * Authors: Tomasz Okon
 * Description: Header of the logic layer (backend) - Piece Table implementation.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

class PieceTable {
public:
    enum class BufferType { Original, Add };

    struct Piece {
        BufferType type_;
        uint64_t start_;
        uint64_t length_;
        uint32_t line_count_;
    };

    PieceTable();
    explicit PieceTable( const std::string& filePath );
    ~PieceTable();

    PieceTable( const PieceTable& ) = delete;
    auto operator=( const PieceTable& ) -> PieceTable& = delete;

    PieceTable( PieceTable&& other ) noexcept;
    auto operator=( PieceTable&& other ) noexcept -> PieceTable&;

    auto replaceFirst( const std::string& pattern, const std::string& replacement,
                       bool matchCase = true, bool matchWord = false ) -> bool;
    auto replaceAll( const std::string& pattern, const std::string& replacement,
                     bool matchCase = true, bool matchWord = false ) -> uint64_t;

    [[nodiscard]] auto size() const -> uint64_t;
    [[nodiscard]] auto getText() const -> std::string;
    [[nodiscard]] auto findAll( const std::string& pattern, bool matchCase = true,
                                bool matchWord = false ) const -> std::vector<uint64_t>;
    [[nodiscard]] auto getSubstr( uint64_t position, uint64_t length ) const -> std::string;
    [[nodiscard]] auto getFragmentsInRange( uint64_t position, uint64_t length ) const
        -> std::vector<Piece>;

    auto insert( uint64_t position, const std::string& text ) -> void;
    auto remove( uint64_t position, uint64_t length ) -> void;
    [[nodiscard]] auto saveToFile( const std::string& filePath ) const -> bool;

    auto undo() -> bool;
    auto redo() -> bool;
    [[nodiscard]] auto canUndo() const -> bool { return !undoStack_.empty(); }
    [[nodiscard]] auto canRedo() const -> bool { return !redoStack_.empty(); }
    [[nodiscard]] auto isDirty() const -> bool { return undoStack_.size() != lastSavedUndoSize_; }

    [[nodiscard]] auto getLineCount() const -> int;
    [[nodiscard]] auto getLineStart( int line ) const -> uint64_t;
    [[nodiscard]] auto getLineFromPosition( uint64_t position ) const -> int;

private:
    [[nodiscard]] static auto computeLPS( const std::string& pattern ) -> std::vector<int>;

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
    
    std::vector<uint64_t> originalNewlines_;
    std::vector<uint64_t> addNewlines_;

    bool isBatchOperation_{ false };
    uint64_t lastSavedUndoSize_{ 0 };
};