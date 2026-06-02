// Author: Jan Szwagierczak

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
    line_start_offsets_.push_back( 0 );
    logical_line_numbers_.clear();
    logical_line_numbers_.push_back( 0 );  // virtual line 0 is always logical line 0
    global_max_line_length_ = 0;
    calculated_up_to_ = 0;
}

void LineManager::invalidateCacheFromOffset( uint64_t offset )
{
    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( line_start_offsets_.empty() ) {
        return;
    }

    auto it = std::upper_bound( line_start_offsets_.begin(), line_start_offsets_.end(), offset );
    if( it != line_start_offsets_.begin() ) {
        --it;  // keep the line containing the offset
    }
    auto kept = std::distance( line_start_offsets_.begin(), it ) + 1;
    line_start_offsets_.erase( it + 1, line_start_offsets_.end() );
    if( kept < static_cast<decltype( kept )>( logical_line_numbers_.size() ) ) {
        logical_line_numbers_.erase( logical_line_numbers_.begin() + kept,
                                     logical_line_numbers_.end() );
    }
    calculated_up_to_ = line_start_offsets_.back();
}

void LineManager::ensureOffsetCalculated( uint64_t target_offset )
{
    std::lock_guard<std::mutex> lock( cache_mutex_ );

    if( line_start_offsets_.empty() ) {
        line_start_offsets_.push_back( 0 );
    }

    uint64_t current_offset = line_start_offsets_.back();
    uint64_t file_size = pt_->size();

    if( current_offset > calculated_up_to_ ) {
        calculated_up_to_ = current_offset;
    }

    if( calculated_up_to_ >= file_size || calculated_up_to_ >= target_offset ) {
        return;
    }

    const uint64_t chunk_size = 64 * 1024;

    while( current_offset < target_offset && current_offset < file_size ) {
        uint64_t line_start = current_offset;
        uint64_t search_offset = line_start;
        bool line_ended = false;
        bool should_push_new_line = false;

        while( !line_ended && search_offset < file_size ) {
            uint64_t to_fetch = std::min( chunk_size, file_size - search_offset );
            // cap fetch at max_visual_line_length_ measured from line_start (hard soft-wrap)
            uint64_t max_fetch =
                std::min( to_fetch, static_cast<uint64_t>( max_visual_line_length_ ) -
                                        ( search_offset - line_start ) );

            if( max_fetch == 0 ) {
                current_offset = search_offset;
                should_push_new_line = true;
                break;
            }

            std::string chunk = pt_->getSubstr( search_offset, max_fetch );
            size_t newline_pos = chunk.find( '\n' );

            if( newline_pos != std::string::npos ) {
                current_offset = search_offset + newline_pos + 1;  // skip the newline
                line_ended = true;
                should_push_new_line = true;
            } else {
                search_offset += chunk.length();
                if( search_offset >= file_size ) {
                    current_offset = file_size;
                    line_ended = true;
                    should_push_new_line = false;
                } else if( search_offset - line_start >=
                           static_cast<uint64_t>( max_visual_line_length_ ) ) {
                    current_offset = search_offset;
                    line_ended = true;
                    should_push_new_line = true;
                }
            }
        }

        if( current_offset <= file_size && current_offset > line_start_offsets_.back() ) {
            uint64_t new_len = current_offset - line_start_offsets_.back();

            std::string last_char = pt_->getSubstr( current_offset - 1, 1 );
            bool ended_with_newline = ( last_char == "\n" );
            if( ended_with_newline ) {
                new_len -= 1;
            }

            if( new_len > global_max_line_length_ ) {
                global_max_line_length_ = new_len;
            }

            // Only cache a new virtual-line start for a genuine break;if( should_push_new_line ) {
                // The new virtual line starts a fresh logical line only if the segment that just
                // ended was terminated by '\n'; a hard-wrap continuation keeps the previous number.
                int prev_logical = logical_line_numbers_.back();
                logical_line_numbers_.push_back( ended_with_newline ? prev_logical + 1
                                                                    : prev_logical );
                line_start_offsets_.push_back( current_offset );
            }
        }

        if( current_offset > calculated_up_to_ ) {
            calculated_up_to_ = current_offset;
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
                return;
            }
            if( calculated_up_to_ >= pt_->size() ) {
                return;  // EOF
            }
        }

        uint64_t start_calc_from;
        {
            std::lock_guard<std::mutex> lock( cache_mutex_ );
            start_calc_from = line_start_offsets_.back();
        }
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
    return line_start_offsets_.back();
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
    if( calculated_up_to_ >= file_size ) {
        return static_cast<int>( line_start_offsets_.size() );
    }

    uint64_t processed_bytes = line_start_offsets_.back();
    int processed_lines = static_cast<int>( line_start_offsets_.size() );

    if( processed_lines < 10 || processed_bytes == 0 ) {
        return processed_lines + static_cast<int>( ( file_size - processed_bytes ) / 50 );
    }

    double avg_bytes_per_line = static_cast<double>( processed_bytes ) / processed_lines;
    int estimated_remaining =
        static_cast<int>( ( file_size - processed_bytes ) / avg_bytes_per_line );
    return processed_lines + estimated_remaining;
}

auto LineManager::isLogicalLineStart( int virtual_line ) -> bool
{
    if( virtual_line <= 0 ) {
        return true;  // the first virtual line always begins logical line 1
    }
    ensureLineCalculated( virtual_line );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( virtual_line >= static_cast<int>( logical_line_numbers_.size() ) ) {
        return true;  // beyond the calculated cache: treat as a real line
    }
    return logical_line_numbers_[virtual_line] != logical_line_numbers_[virtual_line - 1];
}

auto LineManager::getLogicalLineNumber( int virtual_line ) -> int
{
    if( virtual_line < 0 ) {
        return 1;
    }
    ensureLineCalculated( virtual_line );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( virtual_line >= static_cast<int>( logical_line_numbers_.size() ) ) {
        return virtual_line + 1;  // estimation region: keep numbers monotonically increasing
    }
    return logical_line_numbers_[virtual_line] + 1;  // stored value is 0-based
}

auto LineManager::getLogicalColumn( int virtual_line, int col ) -> uint64_t
{
    if( virtual_line <= 0 ) {
        return static_cast<uint64_t>( std::max( 0, col ) );
    }
    ensureLineCalculated( virtual_line );

    std::lock_guard<std::mutex> lock( cache_mutex_ );
    if( virtual_line >= static_cast<int>( line_start_offsets_.size() ) ) {
        return static_cast<uint64_t>( std::max( 0, col ) );
    }
    // Walk back over wrapped continuation segments to the logical line start. Segments are
    // contiguous in bytes (no newline between them), so the logical column is the byte distance
    // from that start to this segment plus the in-segment column.
    int start = virtual_line;
    while( start > 0 && logical_line_numbers_[start] == logical_line_numbers_[start - 1] ) {
        --start;
    }
    uint64_t span_before = line_start_offsets_[virtual_line] - line_start_offsets_[start];
    return span_before + static_cast<uint64_t>( std::max( 0, col ) );
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
    // exclude the trailing newline from the visual length
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

    // snap start back to a UTF-8 boundary
    if( actual_length > 0 && chunk_start > line_start ) {
        uint64_t snapped = Utf8Utils::snapToCharacterBoundary( byteAt, line_start, chunk_start );
        actual_length += chunk_start - snapped;
        chunk_start = snapped;
    }

    std::string chunk = pt_->getSubstr( chunk_start, actual_length );

    // extend end forward over a split UTF-8 sequence
    if( chunk_start + actual_length < line_start + line_len ) {
        uint64_t tail = chunk_start + actual_length;
        if( Utf8Utils::isContinuationByte( byteAt( tail ) ) ) {
            int extra = 0;
            while( extra < Utf8Utils::kMaxSequenceLength && tail + extra < line_start + line_len &&
                   Utf8Utils::isContinuationByte( byteAt( tail + extra ) ) ) {
                ++extra;
            }
            chunk += pt_->getSubstr( tail, extra );
        }
    }

    return chunk;
}
