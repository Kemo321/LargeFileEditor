/**
 * @file LineManagerTest.cc
 * @author Jan Szwagierczak
 * @brief Headless unit tests for LineManager: virtual-line mapping, hard soft-wrap, cache
 *        invalidation and UTF-8 chunk snapping.
 */
#include <gtest/gtest.h>

#include <string>

#include "backend/PieceTable.h"
#include "gui/LineManager.h"

namespace {

// 3-byte UTF-8 encoding of the Euro sign (U+20AC), spelled out byte-by-byte so the test does not
// depend on the source file's own encoding.
const std::string kEuro = "\xE2\x82\xAC";

}  // namespace

TEST( LineManagerTest, EmptyDocument )
{
    PieceTable pt;
    LineManager lm( &pt );

    // An empty document is exactly one virtual line of length zero.
    EXPECT_EQ( lm.getLineCount(), 1 );
    EXPECT_EQ( lm.getVirtualLineLength( 0 ), 0U );
    EXPECT_EQ( lm.getLineOffset( 0 ), 0U );
}

TEST( LineManagerTest, LineLengthExceedsMaxVisual )
{
    PieceTable pt;
    // 25 characters, zero newlines, with a max visual width of 10 -> 10 / 10 / 5.
    pt.insert( 0, "abcdefghijklmnopqrstuvwxy" );

    LineManager lm( &pt, /*max_visual_line_length=*/10 );

    EXPECT_EQ( lm.getVirtualLineLength( 0 ), 10U );
    EXPECT_EQ( lm.getVirtualLineLength( 1 ), 10U );
    EXPECT_EQ( lm.getVirtualLineLength( 2 ), 5U );

    // Each wrapped segment begins exactly max_visual_line_length bytes after the previous one.
    EXPECT_EQ( lm.getLineOffset( 0 ), 0U );
    EXPECT_EQ( lm.getLineOffset( 1 ), 10U );
    EXPECT_EQ( lm.getLineOffset( 2 ), 20U );

    EXPECT_EQ( lm.getGlobalMaxLineLength(), 10U );
}

TEST( LineManagerTest, LogicalLineNumbersSkipWrappedSegments )
{
    PieceTable pt;
    // "ab" (1 line) + a 25-char line that hard-wraps into three 10/10/5 segments + "zz".
    pt.insert( 0, "ab\nABCDEFGHIJKLMNOPQRSTUVWXY\nzz" );

    LineManager lm( &pt, /*max_visual_line_length=*/10 );

    // Only \n-delimited lines start a new logical line; the two wrap continuations do not.
    EXPECT_TRUE( lm.isLogicalLineStart( 0 ) );   // "ab"
    EXPECT_TRUE( lm.isLogicalLineStart( 1 ) );   // first segment of the long line
    EXPECT_FALSE( lm.isLogicalLineStart( 2 ) );  // wrapped continuation
    EXPECT_FALSE( lm.isLogicalLineStart( 3 ) );  // wrapped continuation
    EXPECT_TRUE( lm.isLogicalLineStart( 4 ) );   // "zz"

    EXPECT_EQ( lm.getLogicalLineNumber( 0 ), 1 );
    EXPECT_EQ( lm.getLogicalLineNumber( 1 ), 2 );
    EXPECT_EQ( lm.getLogicalLineNumber( 2 ), 2 );
    EXPECT_EQ( lm.getLogicalLineNumber( 3 ), 2 );
    EXPECT_EQ( lm.getLogicalLineNumber( 4 ), 3 );
}

TEST( LineManagerTest, LogicalColumnFoldsWrappedSegments )
{
    PieceTable pt;
    pt.insert( 0, "ab\nABCDEFGHIJKLMNOPQRSTUVWXY\nzz" );  // long line wraps 10/10/5

    LineManager lm( &pt, /*max_visual_line_length=*/10 );

    // First segment of the long line: column is already logical.
    EXPECT_EQ( lm.getLogicalColumn( 1, 3 ), 3U );
    // Second segment (continuation): col 2 is really byte 12 of the physical line.
    EXPECT_EQ( lm.getLogicalColumn( 2, 2 ), 12U );
    // Third segment (continuation): col 4 is really byte 24.
    EXPECT_EQ( lm.getLogicalColumn( 3, 4 ), 24U );
    // A real line that does not wrap is unaffected.
    EXPECT_EQ( lm.getLogicalColumn( 4, 1 ), 1U );
}

TEST( LineManagerTest, LogicalNumbersSurviveCacheInvalidation )
{
    PieceTable pt;
    pt.insert( 0, "ABCDEFGHIJKLMNOPQRSTUVWXY\ntail" );  // wrapped long line then a real line

    LineManager lm( &pt, /*max_visual_line_length=*/10 );
    ASSERT_EQ( lm.getLogicalLineNumber( 3 ), 2 );  // force full calculation ("tail")

    // Invalidating inside the wrapped region must drop and correctly rebuild the logical mapping.
    lm.invalidateCacheFromOffset( 5 );
    EXPECT_TRUE( lm.isLogicalLineStart( 0 ) );
    EXPECT_FALSE( lm.isLogicalLineStart( 1 ) );
    EXPECT_FALSE( lm.isLogicalLineStart( 2 ) );
    EXPECT_TRUE( lm.isLogicalLineStart( 3 ) );
    EXPECT_EQ( lm.getLogicalLineNumber( 3 ), 2 );
}

TEST( LineManagerTest, CacheInvalidation )
{
    PieceTable pt;
    pt.insert( 0, "line1\nline2\nline3\nline4\nline5" );  // five lines, no trailing newline

    LineManager lm( &pt );

    // Force a full calculation of all five line offsets.
    const uint64_t line0 = lm.getLineOffset( 0 );
    const uint64_t line1 = lm.getLineOffset( 1 );
    const uint64_t line2 = lm.getLineOffset( 2 );
    const uint64_t line3 = lm.getLineOffset( 3 );
    const uint64_t line4 = lm.getLineOffset( 4 );
    ASSERT_EQ( line0, 0U );
    ASSERT_EQ( line1, 6U );
    ASSERT_EQ( line2, 12U );
    ASSERT_EQ( line3, 18U );
    ASSERT_EQ( line4, 24U );

    // Invalidate from a byte that lands inside line 2 ([12,18)): lines 3, 4 and 5 must be dropped.
    lm.invalidateCacheFromOffset( 14 );

    // Lines 0-2 survived; lines 3-4 are recomputed lazily and must match the originals exactly.
    EXPECT_EQ( lm.getLineOffset( 0 ), line0 );
    EXPECT_EQ( lm.getLineOffset( 1 ), line1 );
    EXPECT_EQ( lm.getLineOffset( 2 ), line2 );
    EXPECT_EQ( lm.getLineOffset( 3 ), line3 );
    EXPECT_EQ( lm.getLineOffset( 4 ), line4 );

    EXPECT_EQ( lm.getVirtualLineLength( 2 ), 5U );
    EXPECT_EQ( lm.getVirtualLineLength( 3 ), 5U );
    EXPECT_EQ( lm.getVirtualLineLength( 4 ), 5U );
}

TEST( LineManagerTest, Utf8ChunkSnapping )
{
    PieceTable pt;
    // "a" + 3-byte Euro + "b": the Euro occupies byte columns 1, 2 and 3.
    pt.insert( 0, "a" + kEuro + "b" );

    LineManager lm( &pt );
    ASSERT_EQ( lm.getVirtualLineLength( 0 ), 5U );

    // Start column 2 lands on the Euro's second continuation byte: backward snapping must rewind to
    // the lead byte (col 1) and emit the whole 3-byte character.
    EXPECT_EQ( lm.getLineChunk( 0, /*start_col=*/2, /*length=*/2 ), kEuro );

    // Length 2 from column 0 ends mid-Euro: forward snapping must pull in the trailing bytes so the
    // chunk contains the complete character ("a" + Euro).
    EXPECT_EQ( lm.getLineChunk( 0, /*start_col=*/0, /*length=*/2 ), "a" + kEuro );
}
