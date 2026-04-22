/**
 * Authors: Tomasz Okon
 * Description: Implementation of the Piece Table backend logic using mmap.
 */

#include "backend/PieceTable.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <utility>

PieceTable::PieceTable() = default;

PieceTable::PieceTable( const std::string& filePath )
{
    openMmap( filePath );
    if( originalBuffer_ != MAP_FAILED && originalBuffer_ != nullptr && mmapSize_ > 0 ) {
        // Preallocate for performance estimate
        originalNewlines_.reserve(mmapSize_ / 50);
        for( uint64_t i = 0; i < mmapSize_; ++i ) {
            if( originalBuffer_[i] == '\n' ) {
                originalNewlines_.push_back( i );
            }
        }
        pieces_.push_back( { BufferType::Original, 0, mmapSize_,
                             static_cast<uint32_t>( originalNewlines_.size() ) } );
    }
}

PieceTable::~PieceTable()
{
    closeMmap();
}

auto PieceTable::openMmap( const std::string& filePath ) -> void
{
    fileDescriptor_ = open( filePath.c_str(), O_RDONLY );
    if( fileDescriptor_ == -1 ) {
        return;
    }

    struct stat sb {};
    if( fstat( fileDescriptor_, &sb ) == -1 ) {
        close(fileDescriptor_);
        fileDescriptor_ = -1;
        return;
    }

    mmapSize_ = static_cast<uint64_t>( sb.st_size );
    // Use MAP_PRIVATE to avoid SIGBUS if the file is modified externally
    originalBuffer_ = static_cast<const char*>(
        mmap( nullptr, mmapSize_, PROT_READ, MAP_PRIVATE, fileDescriptor_, 0 ) );
}

auto PieceTable::closeMmap() -> void
{
    if( originalBuffer_ != MAP_FAILED && originalBuffer_ != nullptr ) {
        munmap( const_cast<char*>( originalBuffer_ ), mmapSize_ );
        originalBuffer_ = nullptr;
    }
    if( fileDescriptor_ != -1 ) {
        close( fileDescriptor_ );
        fileDescriptor_ = -1;
    }
}

auto PieceTable::size() const -> uint64_t
{
    return std::accumulate(
        pieces_.begin(), pieces_.end(), 0ULL,
        []( uint64_t acc, const Piece& piece ) -> uint64_t { return acc + piece.length_; } );
}

auto PieceTable::getText() const -> std::string
{
    std::string result;
    try {
        result.reserve( std::min<uint64_t>(size(), 512 * 1024 * 1024) ); // Cap preallocation to 512MB
    } catch (const std::bad_alloc&) {
        // Fallback if system cannot provide contiguous memory
    }

    for( const auto& piece : pieces_ ) {
        if( piece.type_ == BufferType::Original ) {
            if( originalBuffer_ != MAP_FAILED && originalBuffer_ != nullptr ) {
                result.append( originalBuffer_ + piece.start_, piece.length_ );
            }
        } else {
            result.append( addBuffer_.data() + piece.start_, piece.length_ );
        }
    }
    return result;
}

auto PieceTable::getSubstr( uint64_t position, uint64_t length ) const -> std::string
{
    if( position + length > size() ) {
        throw std::out_of_range( "Substring range out of bounds" );
    }

    std::string result;
    result.reserve( length );
    auto [index, offset] = findPieceAt( position );
    uint64_t remaining = length;

    while( remaining > 0 && index < pieces_.size() ) {
        const Piece& piece = pieces_[index];
        uint64_t availableInPiece = piece.length_ - offset;
        uint64_t toCopy = std::min( remaining, availableInPiece );
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? originalBuffer_ : addBuffer_.data();
        result.append( bufferPtr + piece.start_ + offset, toCopy );
        remaining -= toCopy;
        offset = 0;
        index++;
    }
    return result;
}

auto PieceTable::findPieceAt( uint64_t position ) const -> FindResult
{
    uint64_t currentPos = 0;
    for( size_t i = 0; i < pieces_.size(); ++i ) {
        if( position >= currentPos && position < currentPos + pieces_[i].length_ ) {
            return { i, position - currentPos };
        }
        currentPos += pieces_[i].length_;
    }
    if( position == currentPos ) {
        return { pieces_.size(), 0 };
    }
    throw std::out_of_range( "Position out of bounds" );
}

auto PieceTable::splitPiece( size_t pieceIndex, uint64_t offset ) -> void
{
    if( offset == 0 || offset >= pieces_[pieceIndex].length_ ) {
        return;
    }

    Piece& orig = pieces_[pieceIndex];
    const auto& newlines = (orig.type_ == BufferType::Original) ? originalNewlines_ : addNewlines_;

    auto it1 = std::lower_bound(newlines.begin(), newlines.end(), orig.start_);
    auto it2 = std::lower_bound(it1, newlines.end(), orig.start_ + offset);
    uint32_t left_lines = std::distance(it1, it2);

    Piece nextPiece = { orig.type_, orig.start_ + offset, orig.length_ - offset, orig.line_count_ - left_lines };
    orig.length_ = offset;
    orig.line_count_ = left_lines;

    pieces_.insert( pieces_.begin() + pieceIndex + 1, nextPiece );
}

auto PieceTable::insert( uint64_t position, const std::string& text ) -> void
{
    if( text.empty() ) {
        return;
    }
    saveState();

    const uint64_t currentSize = size();
    if( position > currentSize ) {
        throw std::out_of_range( "Insert out of range" );
    }

    const auto startInAdd = static_cast<uint64_t>( addBuffer_.length() );
    addBuffer_.append( text );
    const auto textLength = static_cast<uint64_t>( text.length() );

    uint32_t line_count = 0;
    for( uint32_t i = 0; i < textLength; ++i ) {
        if( text[i] == '\n' ) {
            addNewlines_.push_back( startInAdd + i );
            line_count++;
        }
    }

    if( position == currentSize ) {
        pieces_.push_back( { BufferType::Add, startInAdd, textLength, line_count } );
    } else {
        auto res = findPieceAt( position );
        if( res.offsetInPiece_ > 0 ) {
            splitPiece( res.pieceIndex_, res.offsetInPiece_ );
            res.pieceIndex_++;
        }
        pieces_.insert( pieces_.begin() + res.pieceIndex_,
                        { BufferType::Add, startInAdd, textLength, line_count } );
    }
}

auto PieceTable::remove( uint64_t position, uint64_t length ) -> void
{
    if( length == 0 ) {
        return;
    }
    saveState();
    if( position + length > size() ) {
        throw std::out_of_range( "Remove out of range" );
    }

    auto endRes = findPieceAt( position + length );
    if( endRes.pieceIndex_ < pieces_.size() && endRes.offsetInPiece_ > 0 ) {
        splitPiece( endRes.pieceIndex_, endRes.offsetInPiece_ );
    }

    auto startRes = findPieceAt( position );
    if( startRes.offsetInPiece_ > 0 ) {
        splitPiece( startRes.pieceIndex_, startRes.offsetInPiece_ );
        startRes.pieceIndex_++;
    }

    uint64_t removedSoFar = 0;
    auto it = pieces_.begin() + startRes.pieceIndex_;
    while( removedSoFar < length && it != pieces_.end() ) {
        removedSoFar += it->length_;
        it = pieces_.erase( it );
    }
}

auto PieceTable::saveToFile( const std::string& filePath ) const -> bool
{
    std::ofstream outFile( filePath, std::ios::binary );
    if( !outFile.is_open() ) {
        return false;
    }

    for( const auto& piece : pieces_ ) {
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? originalBuffer_ : addBuffer_.data();
        if( bufferPtr != nullptr && bufferPtr != MAP_FAILED ) {
            outFile.write( bufferPtr + piece.start_,
                           static_cast<std::streamsize>( piece.length_ ) );
        }
    }

    if( outFile.good() ) {
        const_cast<PieceTable*>( this )->lastSavedUndoSize_ = undoStack_.size();
        return true;
    }
    return false;
}

auto PieceTable::computeLPS( const std::string& pattern ) -> std::vector<int>
{
    const int length = static_cast<int>( pattern.length() );
    std::vector<int> lps( length, 0 );
    int len = 0;
    int i = 1;

    while( i < length ) {
        if( pattern[i] == pattern[len] ) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if( len != 0 ) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
    return lps;
}

auto PieceTable::findAll( const std::string& pattern, bool matchCase, bool matchWord ) const
    -> std::vector<uint64_t>
{
    std::vector<uint64_t> results;
    if( pattern.empty() || size() == 0 ) {
        return results;
    }

    std::string searchPattern = pattern;
    if( !matchCase ) {
        std::transform( searchPattern.begin(), searchPattern.end(), searchPattern.begin(),
                        []( unsigned char c ) { return std::tolower( c ); } );
    }

    std::vector<int> lps = computeLPS( searchPattern );
    int j = 0;
    uint64_t logical_pos = 0;

    for( const auto& piece : pieces_ ) {
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? originalBuffer_ : addBuffer_.data();
        if( bufferPtr == nullptr || bufferPtr == MAP_FAILED ) {
            logical_pos += piece.length_;
            continue;
        }

        for( uint64_t i = 0; i < piece.length_; ++i ) {
            char currentChar = bufferPtr[piece.start_ + i];
            char compareChar = matchCase ? currentChar
                                         : static_cast<char>( std::tolower(
                                               static_cast<unsigned char>( currentChar ) ) );

            while( j > 0 && searchPattern[j] != compareChar ) {
                j = lps[j - 1];
            }
            if( searchPattern[j] == compareChar ) {
                j++;
            }

            if( j == static_cast<int>( searchPattern.length() ) ) {
                uint64_t foundPos = logical_pos - j + 1;
                bool keepResult = true;

                if( matchWord ) {
                    char before = getSubstr( foundPos - 1, 1 )[0];
                    if( std::isalnum( static_cast<unsigned char>( before ) ) != 0 ) {
                        keepResult = false;
                    }
                    if( keepResult && ( foundPos + j < size() ) ) {
                        char after = getSubstr( foundPos + j, 1 )[0];
                        if( std::isalnum( static_cast<unsigned char>( after ) ) != 0 ) {
                            keepResult = false;
                        }
                    }
                }

                if( keepResult ) {
                    results.push_back( foundPos );
                }
                j = lps[j - 1];
            }
            logical_pos++;
        }
    }
    return results;
}

auto PieceTable::replaceAll( const std::string& pattern, const std::string& replacement,
                             bool matchCase, bool matchWord ) -> uint64_t
{
    if( pattern.empty() ) {
        return 0;
    }
    std::vector<uint64_t> occurrences = findAll( pattern, matchCase, matchWord );
    if( occurrences.empty() ) {
        return 0;
    }

    saveState();
    isBatchOperation_ = true;
    uint64_t patternLen = pattern.length();

    for( auto it = occurrences.rbegin(); it != occurrences.rend(); ++it ) {
        uint64_t pos = *it;
        remove( pos, patternLen );
        insert( pos, replacement );
    }
    isBatchOperation_ = false;
    return static_cast<uint64_t>( occurrences.size() );
}

auto PieceTable::replaceFirst( const std::string& pattern, const std::string& replacement,
                               bool matchCase, bool matchWord ) -> bool
{
    std::vector<uint64_t> occurrences = findAll( pattern, matchCase, matchWord );
    if( occurrences.empty() ) {
        return false;
    }
    remove( occurrences[0], pattern.length() );
    insert( occurrences[0], replacement );
    return true;
}

void PieceTable::saveState()
{
    if( isBatchOperation_ ) {
        return;
    }
    undoStack_.push_back( pieces_ );
    redoStack_.clear();
    // 100 history states is safe because pieces_ copying is now very cheap
    if( undoStack_.size() > 100 ) {
        undoStack_.erase( undoStack_.begin() );
    }
}

auto PieceTable::undo() -> bool
{
    if( undoStack_.empty() ) {
        return false;
    }
    redoStack_.push_back( pieces_ );
    pieces_ = undoStack_.back();
    undoStack_.pop_back();
    return true;
}

auto PieceTable::redo() -> bool
{
    if( redoStack_.empty() ) {
        return false;
    }
    undoStack_.push_back( pieces_ );
    pieces_ = redoStack_.back();
    redoStack_.pop_back();
    return true;
}

PieceTable::PieceTable( PieceTable&& other ) noexcept
    : originalBuffer_( other.originalBuffer_ ),
      mmapSize_( other.mmapSize_ ),
      fileDescriptor_( other.fileDescriptor_ ),
      addBuffer_( std::move( other.addBuffer_ ) ),
      pieces_( std::move( other.pieces_ ) ),
      originalNewlines_( std::move( other.originalNewlines_ ) ),
      addNewlines_( std::move( other.addNewlines_ ) ),
      isBatchOperation_( other.isBatchOperation_ ),
      lastSavedUndoSize_( other.lastSavedUndoSize_ ),
      undoStack_( std::move( other.undoStack_ ) ),
      redoStack_( std::move( other.redoStack_ ) )
{
    other.originalBuffer_ = nullptr;
    other.fileDescriptor_ = -1;
    other.mmapSize_ = 0;
}

auto PieceTable::operator=( PieceTable&& other ) noexcept -> PieceTable&
{
    if( this != &other ) {
        closeMmap();
        originalBuffer_ = other.originalBuffer_;
        mmapSize_ = other.mmapSize_;
        fileDescriptor_ = other.fileDescriptor_;
        addBuffer_ = std::move( other.addBuffer_ );
        pieces_ = std::move( other.pieces_ );
        originalNewlines_ = std::move( other.originalNewlines_ );
        addNewlines_ = std::move( other.addNewlines_ );
        isBatchOperation_ = other.isBatchOperation_;
        lastSavedUndoSize_ = other.lastSavedUndoSize_;
        undoStack_ = std::move( other.undoStack_ );
        redoStack_ = std::move( other.redoStack_ );

        other.originalBuffer_ = nullptr;
        other.fileDescriptor_ = -1;
        other.mmapSize_ = 0;
    }
    return *this;
}

auto PieceTable::getLineCount() const -> int
{
    return std::accumulate( pieces_.begin(), pieces_.end(), 1,
                            []( int acc, const Piece& piece ) { return acc + piece.line_count_; } );
}

auto PieceTable::getLineStart( int line ) const -> uint64_t
{
    if( line <= 0 ) {
        return 0;
    }

    uint64_t current_pos = 0;
    int current_line = 0;

    for( const auto& piece : pieces_ ) {
        if( current_line + piece.line_count_ >= static_cast<uint32_t>( line ) ) {
            int newline_index = line - current_line - 1;
            const auto& newlines = (piece.type_ == BufferType::Original) ? originalNewlines_ : addNewlines_;
            auto it = std::lower_bound(newlines.begin(), newlines.end(), piece.start_);
            
            uint64_t absolute_pos = *(it + newline_index);
            uint64_t offset_in_piece = absolute_pos - piece.start_;
            return current_pos + offset_in_piece + 1;
        }
        current_line += piece.line_count_;
        current_pos += piece.length_;
    }
    return size();
}

auto PieceTable::getLineFromPosition( uint64_t position ) const -> int
{
    if( position >= size() ) {
        return getLineCount() - 1;
    }

    uint64_t current_pos = 0;
    int current_line = 0;

    for( const auto& piece : pieces_ ) {
        if( position < current_pos + piece.length_ ) {
            uint64_t offset_in_piece = position - current_pos;
            const auto& newlines = (piece.type_ == BufferType::Original) ? originalNewlines_ : addNewlines_;
            
            auto it1 = std::lower_bound(newlines.begin(), newlines.end(), piece.start_);
            auto it2 = std::upper_bound(it1, newlines.end(), piece.start_ + offset_in_piece);
            return current_line + std::distance(it1, it2);
        }
        current_line += piece.line_count_;
        current_pos += piece.length_;
    }
    return current_line;
}

auto PieceTable::getFragmentsInRange( uint64_t position, uint64_t length ) const
    -> std::vector<Piece>
{
    std::vector<Piece> fragments;
    if( length == 0 || position >= size() ) {
        return fragments;
    }

    uint64_t safeLength = std::min( length, size() - position );
    auto [index, offset] = findPieceAt( position );
    uint64_t remaining = safeLength;

    while( remaining > 0 && index < pieces_.size() ) {
        const Piece& piece = pieces_[index];
        uint64_t availableInPiece = piece.length_ - offset;
        uint64_t toTake = std::min( remaining, availableInPiece );

        fragments.push_back( { piece.type_, piece.start_ + offset, toTake, 0 } ); // line_count omitted for fragments

        remaining -= toTake;
        offset = 0;
        index++;
    }
    return fragments;
}