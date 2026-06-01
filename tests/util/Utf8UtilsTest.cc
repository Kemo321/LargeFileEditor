/**
 * @file Utf8UtilsTest.cc
 * @author Jan Szwagierczak
 * @brief Unit tests for the Utf8Utils byte-parsing helpers.
 */
#include <gtest/gtest.h>

#include <string>

#include "util/Utf8Utils.h"

namespace {

auto byteAccessor( const std::string& data )
{
    return [&data]( uint64_t pos ) { return static_cast<unsigned char>( data[pos] ); };
}

}  // namespace

TEST( Utf8UtilsTest, IsContinuationByte )
{
    EXPECT_TRUE( Utf8Utils::isContinuationByte( 0x80 ) );
    EXPECT_TRUE( Utf8Utils::isContinuationByte( 0xBF ) );
    EXPECT_FALSE( Utf8Utils::isContinuationByte( 0x41 ) );  // 'A'
    EXPECT_FALSE( Utf8Utils::isContinuationByte( 0xC0 ) );  // 2-byte lead
    EXPECT_FALSE( Utf8Utils::isContinuationByte( 0xE0 ) );  // 3-byte lead
    EXPECT_FALSE( Utf8Utils::isContinuationByte( 0xF0 ) );  // 4-byte lead
}

TEST( Utf8UtilsTest, SequenceLength )
{
    EXPECT_EQ( Utf8Utils::sequenceLength( 0x41 ), 1 );  // ASCII 'A'
    EXPECT_EQ( Utf8Utils::sequenceLength( 0xC3 ), 2 );  // 'Ã' lead (é = C3 A9)
    EXPECT_EQ( Utf8Utils::sequenceLength( 0xE2 ), 3 );  // '€' lead (E2 82 AC)
    EXPECT_EQ( Utf8Utils::sequenceLength( 0xF0 ), 4 );  // emoji lead
    EXPECT_EQ( Utf8Utils::sequenceLength( 0x80 ), 1 );  // continuation byte -> treated as 1
}

TEST( Utf8UtilsTest, SnapNoOpOnLeadByte )
{
    std::string data = "A\xC3\xA9";  // 'A' + 'é'
    auto byteAt = byteAccessor( data );
    // Position 0 ('A') and position 1 (lead of 'é') are already boundaries.
    EXPECT_EQ( Utf8Utils::snapToCharacterBoundary( byteAt, 0, 0 ), 0U );
    EXPECT_EQ( Utf8Utils::snapToCharacterBoundary( byteAt, 0, 1 ), 1U );
}

TEST( Utf8UtilsTest, SnapFromMidSequence )
{
    std::string data = "\xE2\x82\xAC";  // '€' = E2 82 AC
    auto byteAt = byteAccessor( data );
    EXPECT_EQ( Utf8Utils::snapToCharacterBoundary( byteAt, 0, 1 ),
               0U );  // 1st continuation -> lead
    EXPECT_EQ( Utf8Utils::snapToCharacterBoundary( byteAt, 0, 2 ),
               0U );  // 2nd continuation -> lead
}

TEST( Utf8UtilsTest, SnapRespectsFloor )
{
    std::string data = "\xE2\x82\xAC";  // all of '€'
    auto byteAt = byteAccessor( data );
    // With floor=2, snapping position 2 cannot walk back past 2.
    EXPECT_EQ( Utf8Utils::snapToCharacterBoundary( byteAt, 2, 2 ), 2U );
}
