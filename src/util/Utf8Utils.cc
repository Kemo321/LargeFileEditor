// Author: Jan Szwagierczak

#include "util/Utf8Utils.h"

namespace Utf8Utils {

auto isContinuationByte( unsigned char byte ) -> bool
{
    return ( byte & 0xC0 ) == 0x80;
}

auto sequenceLength( unsigned char leadByte ) -> int
{
    if( ( leadByte & 0xE0 ) == 0xC0 ) {
        return 2;
    }
    if( ( leadByte & 0xF0 ) == 0xE0 ) {
        return 3;
    }
    if( ( leadByte & 0xF8 ) == 0xF0 ) {
        return 4;
    }
    return 1;
}

auto snapToCharacterBoundary( const std::function<unsigned char( uint64_t )>& byteAt,
                              uint64_t floor, uint64_t position ) -> uint64_t
{
    int backOffset = 0;
    while( backOffset < kMaxSequenceLength && position >= floor + backOffset ) {
        unsigned char byte = byteAt( position - backOffset );
        if( !isContinuationByte( byte ) ) {
            return position - backOffset;
        }
        ++backOffset;
    }
    return position;
}

}  // namespace Utf8Utils
