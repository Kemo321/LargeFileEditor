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

static constexpr uint64_t kEstimatedLineLength = 50;
static constexpr uint64_t kMaxPreallocation = 512ULL * 1024ULL * 1024ULL;
static constexpr size_t kMaxUndoHistory = 100;

PieceTable::PieceTable() = default;

PieceTable::PieceTable( const std::string& filePath )
{
    openMmap( filePath );
    if( originalBuffer_ != MAP_FAILED && originalBuffer_ != nullptr && mmapSize_ > 0 ) {
        pieces_.push_back( { BufferType::Original, 0, mmapSize_ } );
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

    struct stat statBuffer {};
    if( fstat( fileDescriptor_, &statBuffer ) == -1 ) {
        close( fileDescriptor_ );
        fileDescriptor_ = -1;
        return;
    }

    mmapSize_ = static_cast<uint64_t>( statBuffer.st_size );
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
        result.reserve( std::min<uint64_t>( size(), kMaxPreallocation ) );
    } catch( const std::bad_alloc& ) {
        throw std::runtime_error( "Document is too large to fit in memory" );
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
    for( size_t idx = 0; idx < pieces_.size(); ++idx ) {
        if( position >= currentPos && position < currentPos + pieces_[idx].length_ ) {
            return { idx, position - currentPos };
        }
        currentPos += pieces_[idx].length_;
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

    Piece nextPiece = { orig.type_, orig.start_ + offset, orig.length_ - offset };
    orig.length_ = offset;

    pieces_.insert( pieces_.begin() + static_cast<std::ptrdiff_t>( pieceIndex ) + 1, nextPiece );
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

    if( position == currentSize ) {
        pieces_.push_back( { BufferType::Add, startInAdd, textLength } );
    } else {
        auto res = findPieceAt( position );
        if( res.offsetInPiece_ > 0 ) {
            splitPiece( res.pieceIndex_, res.offsetInPiece_ );
            res.pieceIndex_++;
        }
        pieces_.insert( pieces_.begin() + static_cast<std::ptrdiff_t>( res.pieceIndex_ ),
                        { BufferType::Add, startInAdd, textLength } );
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
    auto iter = pieces_.begin() + static_cast<std::ptrdiff_t>( startRes.pieceIndex_ );
    while( removedSoFar < length && iter != pieces_.end() ) {
        removedSoFar += iter->length_;
        iter = pieces_.erase( iter );
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
    int idx = 1;

    while( idx < length ) {
        if( pattern[idx] == pattern[len] ) {
            len++;
            lps[idx] = len;
            idx++;
        } else {
            if( len != 0 ) {
                len = lps[len - 1];
            } else {
                lps[idx] = 0;
                idx++;
            }
        }
    }
    return lps;
}

auto PieceTable::findAll( const std::string& pattern, bool matchCase, bool matchWord ) const
    -> std::vector<uint64_t>
{
    static const std::atomic<bool> never{ false };
    return findAllImpl(
        pattern, matchCase, matchWord, []( uint64_t, uint64_t ) {}, never );
}

auto PieceTable::findAllImpl( const std::string& pattern, bool matchCase, bool matchWord,
                              const std::function<void( uint64_t, uint64_t )>& progress,
                              const std::atomic<bool>& cancel ) const -> std::vector<uint64_t>
{
    // Poll progress/cancel roughly every 1 MiB of scanned bytes.
    static constexpr uint64_t kScanProgressMask = ( 1ULL << 20 ) - 1;

    std::vector<uint64_t> results;
    const uint64_t totalBytes = size();
    if( pattern.empty() || totalBytes == 0 ) {
        return results;
    }

    std::string searchPattern = pattern;
    if( !matchCase ) {
        std::transform( searchPattern.begin(), searchPattern.end(), searchPattern.begin(),
                        []( unsigned char ch ) { return std::tolower( ch ); } );
    }

    std::vector<int> lps = computeLPS( searchPattern );
    int matchIdx = 0;
    uint64_t logical_pos = 0;

    for( const auto& piece : pieces_ ) {
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? originalBuffer_ : addBuffer_.data();
        if( bufferPtr == nullptr || bufferPtr == MAP_FAILED ) {
            logical_pos += piece.length_;
            continue;
        }

        for( uint64_t idx = 0; idx < piece.length_; ++idx ) {
            if( ( logical_pos & kScanProgressMask ) == 0 ) {
                if( cancel.load( std::memory_order_relaxed ) ) {
                    return {};
                }
                progress( logical_pos, totalBytes );
            }

            char currentChar = bufferPtr[piece.start_ + idx];
            char compareChar = matchCase ? currentChar
                                         : static_cast<char>( std::tolower(
                                               static_cast<unsigned char>( currentChar ) ) );

            while( matchIdx > 0 && searchPattern[matchIdx] != compareChar ) {
                matchIdx = lps[matchIdx - 1];
            }
            if( searchPattern[matchIdx] == compareChar ) {
                matchIdx++;
            }

            if( matchIdx == static_cast<int>( searchPattern.length() ) ) {
                uint64_t foundPos = logical_pos - matchIdx + 1;
                bool keepResult = true;

                if( matchWord ) {
                    char before = getSubstr( foundPos - 1, 1 )[0];
                    if( std::isalnum( static_cast<unsigned char>( before ) ) != 0 ) {
                        keepResult = false;
                    }
                    if( keepResult && ( foundPos + matchIdx < totalBytes ) ) {
                        char after = getSubstr( foundPos + matchIdx, 1 )[0];
                        if( std::isalnum( static_cast<unsigned char>( after ) ) != 0 ) {
                            keepResult = false;
                        }
                    }
                }

                if( keepResult ) {
                    results.push_back( foundPos );
                }
                // Non-overlapping semantics: resume scanning after the consumed match
                // (both accepted and word-boundary-rejected terminal branches reset to 0)
                // so successive matches are always at least patternLen apart.
                matchIdx = 0;
            }
            logical_pos++;
        }
    }
    return results;
}

auto PieceTable::replaceAll( const std::string& pattern, const std::string& replacement,
                             bool matchCase, bool matchWord ) -> uint64_t
{
    static const std::atomic<bool> never{ false };
    return replaceAll(
        pattern, replacement, matchCase, matchWord, []( uint64_t, uint64_t ) {}, never );
}

auto PieceTable::replaceAll( const std::string& pattern, const std::string& replacement,
                             bool matchCase, bool matchWord,
                             const std::function<void( uint64_t, uint64_t )>& progress,
                             const std::atomic<bool>& cancel ) -> uint64_t
{
    static constexpr uint64_t kProgressStride = 4096;

    if( pattern.empty() ) {
        return 0;
    }

    // The whole operation is two byte-sized passes (scan + merge); report progress over
    // 2 * totalBytes so the bar climbs monotonically 0->100% across both phases.
    const uint64_t totalBytes = size();
    const uint64_t progressSpan = 2 * totalBytes;
    auto scanProgress = [&progress, progressSpan]( uint64_t bytesScanned, uint64_t /*total*/ ) {
        progress( bytesScanned, progressSpan );
    };
    std::vector<uint64_t> occurrences =
        findAllImpl( pattern, matchCase, matchWord, scanProgress, cancel );
    if( occurrences.empty() ) {  // no matches, or canceled mid-scan (nothing mutated yet)
        return 0;
    }

    const uint64_t patternLen = pattern.length();
    const uint64_t replLen = replacement.length();
    const uint64_t savedAddSize = addBuffer_.size();
    bool replAppended = false;

    std::vector<Piece> newPieces;
    newPieces.reserve( pieces_.size() + 2 * occurrences.size() );

    // Forward-only cursor over the old pieces_ (occurrences are ascending and
    // non-overlapping, so the cursor is never rewound).
    size_t srcIdx = 0;
    uint64_t srcOff = 0;
    uint64_t cursor = 0;
    auto advance = [&]( uint64_t target, bool emit ) {
        while( cursor < target && srcIdx < pieces_.size() ) {
            const Piece& piece = pieces_[srcIdx];
            const uint64_t take = std::min( target - cursor, piece.length_ - srcOff );
            if( emit && take > 0 ) {
                newPieces.push_back( { piece.type_, piece.start_ + srcOff, take } );
            }
            srcOff += take;
            cursor += take;
            if( srcOff == piece.length_ ) {
                ++srcIdx;
                srcOff = 0;
            }
        }
    };

    const uint64_t matchCount = occurrences.size();
    uint64_t done = 0;

    for( uint64_t occ : occurrences ) {
        if( cancel.load( std::memory_order_relaxed ) ) {
            addBuffer_.resize( savedAddSize );
            return 0;
        }
        advance( occ, true );  // copy the gap [cursor, occ)
        if( replLen > 0 ) {
            if( !replAppended ) {
                addBuffer_.append( replacement );
                replAppended = true;
            }
            newPieces.push_back( { BufferType::Add, savedAddSize, replLen } );
        }
        advance( occ + patternLen, false );  // skip the matched source bytes
        if( ( ++done % kProgressStride ) == 0 ) {
            // Merge occupies the second half of the bar; also refreshes the cancel bridge.
            progress( totalBytes + cursor, progressSpan );
        }
    }
    advance( totalBytes, true );  // copy the tail (pieces_ not yet mutated, so size == totalBytes)

    saveState();
    pieces_ = std::move( newPieces );
    progress( progressSpan, progressSpan );
    return matchCount;
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

    if( undoStack_.size() > kMaxUndoHistory ) {
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

        fragments.push_back( { piece.type_, piece.start_ + offset, toTake } );

        remaining -= toTake;
        offset = 0;
        index++;
    }
    return fragments;
}
