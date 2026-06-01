#include "backend/PieceTable.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "backend/KmpSearch.h"

static constexpr uint64_t kEstimatedLineLength = 50;
static constexpr uint64_t kMaxPreallocation = 512ULL * 1024ULL * 1024ULL;

PieceTable::PieceTable() = default;

PieceTable::PieceTable( const std::string& filePath ) : mappedFile_( filePath )
{
    if( mappedFile_.isValid() ) {
        pieces_.push_back( { BufferType::Original, 0, mappedFile_.size() } );
    }
    rebuildOffsetIndex();
}

PieceTable::~PieceTable() = default;

auto PieceTable::size() const -> uint64_t
{
    return total_size_;
}

auto PieceTable::rebuildOffsetIndex() -> void
{
    pieceStartOffsets_.resize( pieces_.size() + 1 );
    uint64_t running = 0;
    for( size_t idx = 0; idx < pieces_.size(); ++idx ) {
        pieceStartOffsets_[idx] = running;
        running += pieces_[idx].length_;
    }
    pieceStartOffsets_.back() = running;
    total_size_ = running;
}

auto PieceTable::coalescePieces() -> void
{
    if( pieces_.size() < 2 ) {
        return;
    }
    std::vector<Piece> merged;
    merged.reserve( pieces_.size() );
    for( const Piece& piece : pieces_ ) {
        if( piece.length_ == 0 ) {
            continue;
        }
        if( !merged.empty() ) {
            Piece& back = merged.back();
            if( back.type_ == piece.type_ && back.start_ + back.length_ == piece.start_ ) {
                back.length_ += piece.length_;  // contiguous same-buffer run -> extend
                continue;
            }
        }
        merged.push_back( piece );
    }
    pieces_ = std::move( merged );
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
            if( mappedFile_.data() != nullptr ) {
                result.append( mappedFile_.data() + piece.start_, piece.length_ );
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
            ( piece.type_ == BufferType::Original ) ? mappedFile_.data() : addBuffer_.data();
        result.append( bufferPtr + piece.start_ + offset, toCopy );
        remaining -= toCopy;
        offset = 0;
        index++;
    }
    return result;
}

auto PieceTable::findPieceAt( uint64_t position ) const -> FindResult
{
    if( position >= total_size_ ) {
        if( position == total_size_ ) {
            return { pieces_.size(), 0 };
        }
        throw std::out_of_range( "Position out of bounds" );
    }
    // pieceStartOffsets_ is sorted ascending; find the piece whose span contains position.
    auto iter = std::upper_bound( pieceStartOffsets_.begin(), pieceStartOffsets_.end(), position );
    auto idx = static_cast<size_t>( std::distance( pieceStartOffsets_.begin(), iter ) ) - 1;
    return { idx, position - pieceStartOffsets_[idx] };
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
    history_.recordState( pieces_ );

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
    rebuildOffsetIndex();
}

auto PieceTable::remove( uint64_t position, uint64_t length ) -> void
{
    if( length == 0 ) {
        return;
    }
    history_.recordState( pieces_ );
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
    rebuildOffsetIndex();
}

auto PieceTable::saveToFile( const std::string& filePath ) const -> bool
{
    std::ofstream outFile( filePath, std::ios::binary );
    if( !outFile.is_open() ) {
        return false;
    }

    for( const auto& piece : pieces_ ) {
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? mappedFile_.data() : addBuffer_.data();
        if( bufferPtr != nullptr ) {
            outFile.write( bufferPtr + piece.start_,
                           static_cast<std::streamsize>( piece.length_ ) );
        }
    }

    if( outFile.good() ) {
        history_.markSaved();
        return true;
    }
    return false;
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
    std::vector<KmpSearch::Span> spans;
    spans.reserve( pieces_.size() );
    for( const auto& piece : pieces_ ) {
        const char* bufferPtr =
            ( piece.type_ == BufferType::Original ) ? mappedFile_.data() : addBuffer_.data();
        const char* spanData = ( bufferPtr != nullptr ) ? bufferPtr + piece.start_ : nullptr;
        spans.push_back( { spanData, piece.length_ } );
    }

    KmpSearch searcher;
    return KmpSearch::findAll(
        spans, size(), pattern, matchCase, matchWord,
        [this]( uint64_t pos ) { return getSubstr( pos, 1 )[0]; }, progress, cancel );
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

    history_.recordState( pieces_ );
    pieces_ = std::move( newPieces );
    coalescePieces();  // single defrag pass before the main thread repaints
    rebuildOffsetIndex();
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

auto PieceTable::undo() -> bool
{
    if( !history_.undo( pieces_ ) ) {
        return false;
    }
    rebuildOffsetIndex();
    return true;
}

auto PieceTable::redo() -> bool
{
    if( !history_.redo( pieces_ ) ) {
        return false;
    }
    rebuildOffsetIndex();
    return true;
}

PieceTable::PieceTable( PieceTable&& other ) noexcept
    : mappedFile_( std::move( other.mappedFile_ ) ),
      addBuffer_( std::move( other.addBuffer_ ) ),
      pieces_( std::move( other.pieces_ ) ),
      pieceStartOffsets_( std::move( other.pieceStartOffsets_ ) ),
      total_size_( other.total_size_ ),
      history_( std::move( other.history_ ) )
{
    other.total_size_ = 0;
    other.pieceStartOffsets_.assign( 1, 0 );
}

auto PieceTable::operator=( PieceTable&& other ) noexcept -> PieceTable&
{
    if( this != &other ) {
        mappedFile_ = std::move( other.mappedFile_ );
        addBuffer_ = std::move( other.addBuffer_ );
        pieces_ = std::move( other.pieces_ );
        pieceStartOffsets_ = std::move( other.pieceStartOffsets_ );
        total_size_ = other.total_size_;
        history_ = std::move( other.history_ );

        other.total_size_ = 0;
        other.pieceStartOffsets_.assign( 1, 0 );
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
