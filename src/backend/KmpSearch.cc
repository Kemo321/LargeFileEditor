// Author: Tomasz Okon
#include "backend/KmpSearch.h"

#include <algorithm>
#include <cctype>

auto KmpSearch::computeLPS( const std::string& pattern ) -> std::vector<int>
{
    const int length = static_cast<int>( pattern.length() );
    std::vector<int> lps( length, 0 );
    int len = 0;
    int idx = 1;

    while( idx < length ) {
        if( pattern[idx] == pattern[len] ) {
            ++len;
            lps[idx] = len;
            ++idx;
        } else {
            if( len != 0 ) {
                len = lps[len - 1];
            } else {
                lps[idx] = 0;
                ++idx;
            }
        }
    }
    return lps;
}

auto KmpSearch::findAll( const std::vector<Span>& spans, uint64_t totalBytes,
                         const std::string& pattern, bool matchCase, bool matchWord,
                         const std::function<char( uint64_t )>& byteAt, const ProgressFn& progress,
                         const std::atomic<bool>& cancel ) -> std::vector<uint64_t>
{
    // Poll progress/cancel roughly every 1 MiB of scanned bytes.
    static constexpr uint64_t kScanProgressMask = ( 1ULL << 20 ) - 1;

    std::vector<uint64_t> results;
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

    for( const auto& span : spans ) {
        if( span.data___ == nullptr ) {
            logical_pos += span.length_h_h_;
            continue;
        }

        for( uint64_t idx = 0; idx < span.length_h_h_; ++idx ) {
            if( ( logical_pos & kScanProgressMask ) == 0 ) {
                if( cancel.load( std::memory_order_relaxed ) ) {
                    return {};
                }
                progress( logical_pos, totalBytes );
            }

            char currentChar = span.data___[idx];
            char compareChar = matchCase ? currentChar
                                         : static_cast<char>( std::tolower(
                                               static_cast<unsigned char>( currentChar ) ) );

            while( matchIdx > 0 && searchPattern[matchIdx] != compareChar ) {
                matchIdx = lps[matchIdx - 1];
            }
            if( searchPattern[matchIdx] == compareChar ) {
                ++matchIdx;
            }

            if( matchIdx == static_cast<int>( searchPattern.length() ) ) {
                uint64_t foundPos = logical_pos - matchIdx + 1;
                bool keepResult = true;

                if( matchWord ) {
                    char before = byteAt( foundPos - 1 );
                    if( std::isalnum( static_cast<unsigned char>( before ) ) != 0 ) {
                        keepResult = false;
                    }
                    if( keepResult && ( foundPos + matchIdx < totalBytes ) ) {
                        char after = byteAt( foundPos + matchIdx );
                        if( std::isalnum( static_cast<unsigned char>( after ) ) != 0 ) {
                            keepResult = false;
                        }
                    }
                }

                if( keepResult ) {
                    results.push_back( foundPos );
                }
                // Non-overlapping: reset so successive matches are >= patternLen apart.
                matchIdx = 0;
            }
            ++logical_pos;
        }
    }
    return results;
}
