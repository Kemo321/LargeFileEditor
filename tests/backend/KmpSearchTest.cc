/**
 * @file KmpSearchTest.cc
 * @brief Unit tests for the standalone KmpSearch matcher.
 */
#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

#include "backend/KmpSearch.h"

namespace {

const std::atomic<bool> kNeverCancel{ false };

// Searches a single contiguous buffer (the common case).
auto search( const std::string& text, const std::string& pattern, bool matchCase = true,
             bool matchWord = false ) -> std::vector<uint64_t>
{
    KmpSearch searcher;
    std::vector<KmpSearch::Span> spans{ { text.data(), text.size() } };
    return KmpSearch::findAll(
        spans, text.size(), pattern, matchCase, matchWord,
        [&text]( uint64_t pos ) { return text[pos]; }, []( uint64_t, uint64_t ) {}, kNeverCancel );
}

}  // namespace

TEST( KmpSearchTest, FindsAllNonOverlapping )
{
    EXPECT_EQ( search( "abcabcabc", "abc" ), ( std::vector<uint64_t>{ 0, 3, 6 } ) );
}

TEST( KmpSearchTest, NonOverlappingSemantics )
{
    // "aaa" in "aaaaa": matches at 0 and 3 (non-overlapping), not 0,1,2.
    EXPECT_EQ( search( "aaaaa", "aaa" ), ( std::vector<uint64_t>{ 0 } ) );
    EXPECT_EQ( search( "aaaaaa", "aaa" ), ( std::vector<uint64_t>{ 0, 3 } ) );
}

TEST( KmpSearchTest, CaseInsensitive )
{
    EXPECT_EQ( search( "HeLLo hello HELLO", "hello", false ),
               ( std::vector<uint64_t>{ 0, 6, 12 } ) );
    EXPECT_TRUE( search( "HeLLo", "hello", true ).empty() );
}

TEST( KmpSearchTest, WholeWordMatching )
{
    // "cat" appears in "cat", "category", "scat" — only the standalone word matches.
    EXPECT_EQ( search( "cat category scat cat", "cat", true, true ),
               ( std::vector<uint64_t>{ 0, 18 } ) );
}

TEST( KmpSearchTest, EmptyPatternAndNoMatch )
{
    EXPECT_TRUE( search( "abc", "" ).empty() );
    EXPECT_TRUE( search( "abc", "xyz" ).empty() );
    EXPECT_TRUE( search( "", "abc" ).empty() );
}

TEST( KmpSearchTest, MatchStraddlingSpanBoundary )
{
    // Logical text "abcabc" split across two spans; "cab" straddles the boundary.
    std::string whole = "abcabc";
    KmpSearch searcher;
    std::vector<KmpSearch::Span> spans{ { whole.data(), 3 }, { whole.data() + 3, 3 } };
    auto result = KmpSearch::findAll(
        spans, whole.size(), "cab", true, false, [&whole]( uint64_t pos ) { return whole[pos]; },
        []( uint64_t, uint64_t ) {}, kNeverCancel );
    EXPECT_EQ( result, ( std::vector<uint64_t>{ 2 } ) );
}

TEST( KmpSearchTest, NullSpanIsSkipped )
{
    // A null-data span only advances the logical position (mirrors an unmapped buffer).
    std::string tail = "abc";
    KmpSearch searcher;
    std::vector<KmpSearch::Span> spans{ { nullptr, 5 }, { tail.data(), tail.size() } };
    auto byteAt = [&tail]( uint64_t pos ) { return pos >= 5 ? tail[pos - 5] : '\0'; };
    auto result = KmpSearch::findAll(
        spans, 5 + tail.size(), "abc", true, false, byteAt, []( uint64_t, uint64_t ) {},
        kNeverCancel );
    EXPECT_EQ( result, ( std::vector<uint64_t>{ 5 } ) );
}

TEST( KmpSearchTest, CancelReturnsEmpty )
{
    std::string text( 4096, 'a' );
    std::atomic<bool> cancel{ true };
    KmpSearch searcher;
    std::vector<KmpSearch::Span> spans{ { text.data(), text.size() } };
    auto result = KmpSearch::findAll(
        spans, text.size(), "a", true, false, [&text]( uint64_t pos ) { return text[pos]; },
        []( uint64_t, uint64_t ) {}, cancel );
    EXPECT_TRUE( result.empty() );
}
