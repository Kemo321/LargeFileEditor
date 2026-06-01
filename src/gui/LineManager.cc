#include "gui/LineManager.h"

#include <algorithm>
#include <cmath>

#include "backend/PieceTable.h"
#include "util/Utf8Utils.h"

LineManager::LineManager( PieceTable* pt, int max_visual_line_length )
    : pt_( pt ), max_visual_line_length_( max_visual_line_length )
{
    reset();
}

void LineManager::reset()
{
    std::lock_guard<std::mutex> lock( cache_mutex_ );
    line_start_offsets_.clear();
    line_start_offsets_.push_back( 0 );  // Line 0 always starts at offset 0
    global_max_line_length_ = 0;
}

void LineManager::invalidateCacheFromOffset( uint64_t offset )
{
    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( line_start_offsets_.empty() ) {
        return;
    }

    auto it = std::upper_bound( line_start_offsets_.begin(), line_start_offsets_.end(), offset );
    if( it != line_start_offsets_.begin() ) {
        --it;  // Keep the line that contains the offset
    }
    line_start_offsets_.erase( it + 1, line_start_offsets_.end() );
}

void LineManager::ensureOffsetCalculated( uint64_t target_offset )
{
    std::lock_guard<std::mutex> lock( cache_mutex_ );

    if( line_start_offsets_.empty() ) {
        line_start_offsets_.push_back( 0 );
    }

    uint64_t current_offset = line_start_offsets_.back();
    uint64_t file_size = pt_->size();

    if( current_offset >= file_size || current_offset >= target_offset ) {
        return;  // Already calculated past the target or reached EOF
    }

    // Process in chunks to avoid large string allocations
    const uint64_t chunk_size = 64 * 1024;  // 64KB chunks

    while( current_offset < target_offset && current_offset < file_size ) {
        uint64_t line_start = current_offset;
        uint64_t search_offset = line_start;
        bool line_ended = false;

        while( !line_ended && search_offset < file_size ) {
            uint64_t to_fetch = std::min( chunk_size, file_size - search_offset );
            // Limit fetch by MAX_VISUAL_LINE_LENGTH relative to line_start
            uint64_t max_fetch =
                std::min( to_fetch, static_cast<uint64_t>( max_visual_line_length_ ) -
                                        ( search_offset - line_start ) );

            if( max_fetch == 0 ) {
                // We reached max_visual_line_length_
                current_offset = search_offset;
                break;
            }

            std::string chunk = pt_->getSubstr( search_offset, max_fetch );
            size_t newline_pos = chunk.find( '\n' );

            if( newline_pos != std::string::npos ) {
                current_offset = search_offset + newline_pos + 1;  // +1 to skip newline
                line_ended = true;
            } else {
                search_offset += chunk.length();
                if( search_offset >= file_size ) {
                    current_offset = file_size;
                    line_ended = true;
                } else if( search_offset - line_start >=
                           static_cast<uint64_t>( max_visual_line_length_ ) ) {
                    current_offset = search_offset;
                    line_ended = true;
                }
            }
        }

        if( current_offset <= file_size && current_offset > line_start_offsets_.back() ) {
            uint64_t new_len = current_offset - line_start_offsets_.back();

            std::string last_char = pt_->getSubstr( current_offset - 1, 1 );
            if( last_char == "\n" ) {
                new_len -= 1;
            }

            if( new_len > global_max_line_length_ ) {
                global_max_line_length_ = new_len;
            }
            line_start_offsets_.push_back( current_offset );
        }
    }
}

void LineManager::ensureLineCalculated( int target_line )
{
    if( target_line < 0 ) {
        return;
    }

    while( true ) {
        {
            std::lock_guard<std::mutex> lock( cache_mutex_ );
            if( target_line < static_cast<int>( line_start_offsets_.size() ) ) {
                return;  // Already calculated
            }
            if( line_start_offsets_.back() >= pt_->size() ) {
                return;  // EOF reached, can't calculate more lines
            }
        }

        // Calculate the next chunk of lines
        uint64_t start_calc_from;
        {
            std::lock_guard<std::mutex> lock( cache_mutex_ );
            start_calc_from = line_start_offsets_.back();
        }
        // Advance offset significantly to calculate a bunch of lines
        uint64_t target_offset = std::min( start_calc_from + 1024 * 1024, pt_->size() );
        ensureOffsetCalculated( target_offset );
    }
}

auto LineManager::getLineOffset( int virtual_line ) -> uint64_t
{
    if( virtual_line < 0 ) {
        return 0;
    }
    ensureLineCalculated( virtual_line );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( virtual_line < static_cast<int>( line_start_offsets_.size() ) ) {
        return line_start_offsets_[virtual_line];
    }
    return line_start_offsets_.back();  // Return last known if out of bounds
}

auto LineManager::getVirtualLineFromOffset( uint64_t offset ) -> int
{
    ensureOffsetCalculated( offset );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    auto it = std::upper_bound( line_start_offsets_.begin(), line_start_offsets_.end(), offset );
    if( it != line_start_offsets_.begin() ) {
        --it;
    }
    return static_cast<int>( std::distance( line_start_offsets_.begin(), it ) );
}

auto LineManager::getLineCount() -> int
{
    uint64_t file_size = pt_->size();

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( line_start_offsets_.empty() ) {
        return 1;
    }
    if( line_start_offsets_.back() >= file_size ) {
        return static_cast<int>( line_start_offsets_.size() );
    }

    // Estimate remaining lines
    uint64_t processed_bytes = line_start_offsets_.back();
    int processed_lines = static_cast<int>( line_start_offsets_.size() );

    if( processed_lines < 10 || processed_bytes == 0 ) {
        // Fallback estimate: 50 bytes per line
        return processed_lines + static_cast<int>( ( file_size - processed_bytes ) / 50 );
    }

    double avg_bytes_per_line = static_cast<double>( processed_bytes ) / processed_lines;
    int estimated_remaining =
        static_cast<int>( ( file_size - processed_bytes ) / avg_bytes_per_line );
    return processed_lines + estimated_remaining;
}

auto LineManager::getVirtualLineLength( int virtual_line ) -> uint64_t
{
    ensureLineCalculated( virtual_line + 1 );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( virtual_line < 0 || virtual_line >= static_cast<int>( line_start_offsets_.size() ) ) {
        return 0;
    }

    uint64_t start_offset = line_start_offsets_[virtual_line];
    uint64_t end_offset;

    if( virtual_line + 1 < static_cast<int>( line_start_offsets_.size() ) ) {
        end_offset = line_start_offsets_[virtual_line + 1];
    } else {
        end_offset = pt_->size();
    }

    uint64_t length = end_offset - start_offset;
    // Don't include newline character in the visual length
    if( length > 0 ) {
        std::string last_char = pt_->getSubstr( end_offset - 1, 1 );
        if( last_char == "\n" ) {
            length -= 1;
        }
    }
    return length;
}

auto LineManager::getGlobalMaxLineLength() const -> uint64_t
{
    return global_max_line_length_;
}

auto LineManager::getLineChunk( int virtual_line, uint64_t start_col,
                                uint64_t length ) -> std::string
{
    uint64_t line_len = getVirtualLineLength( virtual_line );
    if( start_col >= line_len ) {
        return "";
    }
    uint64_t actual_length = std::min( length, line_len - start_col );

    uint64_t line_start = getLineOffset( virtual_line );
    uint64_t chunk_start = line_start + start_col;

    auto byteAt = [this]( uint64_t pos ) {
        return static_cast<unsigned char>( pt_->getSubstr( pos, 1 )[0] );
    };

    // UTF-8 backward snapping for start
    if( actual_length > 0 && chunk_start > line_start ) {
        uint64_t snapped = Utf8Utils::snapToCharacterBoundary( byteAt, line_start, chunk_start );
        actual_length += chunk_start - snapped;
        chunk_start = snapped;
    }

    std::string chunk = pt_->getSubstr( chunk_start, actual_length );

    // UTF-8 forward snapping for end
    if( chunk_start + actual_length < line_start + line_len ) {
        uint64_t tail = chunk_start + actual_length;
        if( Utf8Utils::isContinuationByte( byteAt( tail ) ) ) {
            int extra = 0;
            while( extra < Utf8Utils::kMaxSequenceLength && tail + extra < line_start + line_len &&
                   Utf8Utils::isContinuationByte( byteAt( tail + extra ) ) ) {
                extra++;
            }
            chunk += pt_->getSubstr( tail, extra );
        }
    }

    return chunk;
}
