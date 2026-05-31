/**
 * @file PieceTableTest.cc
 * @author Tomasz Okon
 * @brief Unit tests for the PieceTable backend logic.
 */
#include <gtest/gtest.h>

#include <atomic>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "backend/PieceTable.h"

class PieceTableTest : public ::testing::Test {
protected:
    std::string tempFilePath_ = "test_piece_table_temp.txt";

    auto SetUp() -> void override
    {
    }

    auto TearDown() -> void override
    {
        std::remove( tempFilePath_.c_str() );
    }

    auto createTempFile( const std::string& text ) -> std::string
    {
        std::ofstream out( tempFilePath_, std::ios::binary );
        out.write( text.data(), static_cast<std::streamsize>( text.size() ) );
        out.close();
        return tempFilePath_;
    }
};

TEST_F( PieceTableTest, EmptyInitialization )
{
    PieceTable pieceTable;
    EXPECT_EQ( pieceTable.size(), 0 );
    EXPECT_EQ( pieceTable.getText(), "" );
}

TEST_F( PieceTableTest, InitializationWithOriginalText )
{
    PieceTable pieceTable( createTempFile( "Hello ZPR!" ) );
    EXPECT_EQ( pieceTable.size(), 10 );
    EXPECT_EQ( pieceTable.getText(), "Hello ZPR!" );
}

TEST_F( PieceTableTest, InsertAtEnd )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    pieceTable.insert( 5, " World" );
    EXPECT_EQ( pieceTable.size(), 11 );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, InsertAtBeginning )
{
    PieceTable pieceTable( createTempFile( "World" ) );
    pieceTable.insert( 0, "Hello " );
    EXPECT_EQ( pieceTable.size(), 11 );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, InsertInTheMiddle )
{
    PieceTable pieceTable( createTempFile( "HelloWorld" ) );
    pieceTable.insert( 5, " " );
    EXPECT_EQ( pieceTable.size(), 11 );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, MultipleInsertsGrowth )
{
    PieceTable pieceTable;
    pieceTable.insert( 0, "A" );
    pieceTable.insert( 1, "B" );
    pieceTable.insert( 2, "C" );
    EXPECT_EQ( pieceTable.size(), 3 );
    EXPECT_EQ( pieceTable.getText(), "ABC" );
}

TEST_F( PieceTableTest, InsertConsecutiveAtSamePosition )
{
    PieceTable pieceTable( createTempFile( "AC" ) );
    pieceTable.insert( 1, "B" );
    pieceTable.insert( 1, "X" );
    EXPECT_EQ( pieceTable.getText(), "AXBC" );
}

TEST_F( PieceTableTest, RemoveAtBeginning )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    pieceTable.remove( 0, 6 );
    EXPECT_EQ( pieceTable.getText(), "World" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveAtEnd )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    pieceTable.remove( 5, 6 );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveInTheMiddle )
{
    PieceTable pieceTable( createTempFile( "Hello CRUEL World" ) );
    pieceTable.remove( 5, 6 );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, RemoveEntireText )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    pieceTable.remove( 0, 5 );
    EXPECT_EQ( pieceTable.size(), 0 );
    EXPECT_EQ( pieceTable.getText(), "" );
}

TEST_F( PieceTableTest, RemoveSpanningMultiplePieces )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    pieceTable.insert( 5, " Cruel" );
    pieceTable.remove( 6, 9 );
    EXPECT_EQ( pieceTable.getText(), "Hello ld" );
}

TEST_F( PieceTableTest, InsertInsideInsertedText )
{
    PieceTable pieceTable( createTempFile( "Start End" ) );
    pieceTable.insert( 6, "The " );
    pieceTable.insert( 10, "Great " );
    EXPECT_EQ( pieceTable.getText(), "Start The Great End" );
}

TEST_F( PieceTableTest, ComplexEditingTypingSimulation )
{
    PieceTable pieceTable( createTempFile( "This is a test." ) );
    pieceTable.remove( 10, 4 );
    pieceTable.insert( 10, "cat" );
    pieceTable.insert( 13, " and a dog" );
    pieceTable.remove( 0, 5 );
    pieceTable.insert( 0, "That " );
    EXPECT_EQ( pieceTable.getText(), "That is a cat and a dog." );
}

TEST_F( PieceTableTest, InsertEmptyString )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    pieceTable.insert( 2, "" );
    EXPECT_EQ( pieceTable.size(), 4 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, RemoveZeroCharacters )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    pieceTable.remove( 2, 0 );
    EXPECT_EQ( pieceTable.size(), 4 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, InsertOutOfBoundsThrowsException )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    EXPECT_THROW( pieceTable.insert( 5, " Out" ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveOutOfBoundsThrowsException )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    EXPECT_THROW( pieceTable.remove( 5, 1 ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveTooManyCharactersTruncatesOrThrows )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    EXPECT_THROW( pieceTable.remove( 2, 10 ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveExactlyOnePiece )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    pieceTable.insert( 2, "XX" );
    pieceTable.remove( 2, 2 );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveCompletelyEliminatingMultipleMiddlePieces )
{
    PieceTable pieceTable( createTempFile( "StartEnd" ) );
    pieceTable.insert( 5, "A" );
    ASSERT_EQ( pieceTable.getText(), "StartAEnd" );
    pieceTable.insert( 6, "B" );
    ASSERT_EQ( pieceTable.getText(), "StartABEnd" );
    pieceTable.insert( 7, "C" );
    ASSERT_EQ( pieceTable.getText(), "StartABCEnd" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 11 ), "StartABCEnd" );
    pieceTable.remove( 4, 5 );
    EXPECT_EQ( pieceTable.getText(), "Starnd" );
}

TEST_F( PieceTableTest, InsertExactlyAtPieceBoundary )
{
    PieceTable pieceTable( createTempFile( "AB" ) );
    pieceTable.insert( 1, "CD" );
    pieceTable.insert( 3, "EF" );
    EXPECT_EQ( pieceTable.getText(), "ACDEFB" );
}

TEST_F( PieceTableTest, DeleteExactlyUpToBoundary )
{
    PieceTable pieceTable( createTempFile( "HelloWorld" ) );
    pieceTable.insert( 5, " " );
    pieceTable.remove( 0, 5 );
    EXPECT_EQ( pieceTable.getText(), " World" );
}

TEST_F( PieceTableTest, DeleteExactlyFromBoundary )
{
    PieceTable pieceTable( createTempFile( "HelloWorld" ) );
    pieceTable.insert( 5, " " );
    pieceTable.remove( 5, 6 );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
}

TEST_F( PieceTableTest, ConsecutiveDeletionsLikeBackspace )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    pieceTable.remove( 4, 1 );
    pieceTable.remove( 3, 1 );
    pieceTable.remove( 2, 1 );
    EXPECT_EQ( pieceTable.getText(), "He" );
}

TEST_F( PieceTableTest, DeleteAllAndRebuild )
{
    PieceTable pieceTable( createTempFile( "Old Text" ) );
    pieceTable.remove( 0, pieceTable.size() );
    pieceTable.insert( 0, "New Text" );
    EXPECT_EQ( pieceTable.getText(), "New Text" );
}

TEST_F( PieceTableTest, RemoveStartingExactlyAtSizeThrows )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    EXPECT_THROW( pieceTable.remove( 4, 1 ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveZeroLengthAtSize )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    pieceTable.remove( 4, 0 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, InsertMultipleTimesSequentiallyAtEnd )
{
    PieceTable pieceTable;
    pieceTable.insert( 0, "H" );
    pieceTable.insert( 1, "e" );
    pieceTable.insert( 2, "l" );
    pieceTable.insert( 3, "l" );
    pieceTable.insert( 4, "o" );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
}

TEST_F( PieceTableTest, InsertMultipleTimesSequentiallyAtBeginning )
{
    PieceTable pieceTable;
    pieceTable.insert( 0, "o" );
    pieceTable.insert( 0, "l" );
    pieceTable.insert( 0, "l" );
    pieceTable.insert( 0, "e" );
    pieceTable.insert( 0, "H" );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
    EXPECT_EQ( pieceTable.getSubstr( 1, 3 ), "ell" );
}

TEST_F( PieceTableTest, ComplexSpanningDeleteAcrossThreePieces )
{
    PieceTable pieceTable( createTempFile( "123789" ) );
    pieceTable.insert( 3, "456" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 9 ), "123456789" );
    pieceTable.remove( 2, 5 );
    EXPECT_EQ( pieceTable.getText(), "1289" );
}

TEST_F( PieceTableTest, DeleteLeavingEmptyPieces )
{
    PieceTable pieceTable( createTempFile( "ABCDEF" ) );
    pieceTable.insert( 3, "XYZ" );
    pieceTable.remove( 3, 3 );
    EXPECT_EQ( pieceTable.getText(), "ABCDEF" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 6 ), "ABCDEF" );
    EXPECT_EQ( pieceTable.size(), 6 );
}

TEST_F( PieceTableTest, ComplexCaseOfMultipleInsertsAndDeletes )
{
    PieceTable pieceTable( createTempFile( "The quick brown fox jumps over the lazy dog." ) );
    pieceTable.remove( 16, 19 );
    ASSERT_EQ( pieceTable.getText(), "The quick brown lazy dog." );
    pieceTable.insert( 16, "cat " );
    ASSERT_EQ( pieceTable.getText(), "The quick brown cat lazy dog." );
    pieceTable.insert( 20, "sly " );
    ASSERT_EQ( pieceTable.getText(), "The quick brown cat sly lazy dog." );
    pieceTable.remove( 4, 6 );
    EXPECT_EQ( pieceTable.getText(), "The brown cat sly lazy dog." );
    pieceTable.remove( 0, 4 );
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog." );
    pieceTable.remove( pieceTable.size() - 1, 1 );
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog" );
    EXPECT_EQ( pieceTable.getSubstr( 6, 3 ), "cat" );
    pieceTable.insert( pieceTable.size(), "!" );
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog!" );
    EXPECT_EQ( pieceTable.getSubstr( 6, 3 ), "cat" );
}

TEST_F( PieceTableTest, FindAllWithEmptyPatternOrEmptyTable )
{
    PieceTable emptyTable;
    EXPECT_TRUE( emptyTable.findAll( "Test" ).empty() );
    EXPECT_TRUE( emptyTable.findAll( "" ).empty() );

    PieceTable pieceTable( createTempFile( "Hello World" ) );
    EXPECT_TRUE( pieceTable.findAll( "" ).empty() );
}

TEST_F( PieceTableTest, FindAllNoMatch )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    auto results = pieceTable.findAll( "ZPR" );
    EXPECT_TRUE( results.empty() );
}

TEST_F( PieceTableTest, FindAllSingleOriginalPiece )
{
    PieceTable pieceTable( createTempFile( "Hello World Hello" ) );
    auto results = pieceTable.findAll( "Hello" );

    ASSERT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 12 );
}

TEST_F( PieceTableTest, FindAllSingleAddPiece )
{
    PieceTable pieceTable;
    pieceTable.insert( 0, "Test data Test" );
    auto results = pieceTable.findAll( "Test" );

    ASSERT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 10 );
}

TEST_F( PieceTableTest, FindAllSpanningOriginalToAddBoundary )
{
    PieceTable pieceTable( createTempFile( "Hel World" ) );
    pieceTable.insert( 3, "lo" );

    auto results = pieceTable.findAll( "Hello" );
    ASSERT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 0 );
}

TEST_F( PieceTableTest, FindAllSpanningAddToOriginalBoundary )
{
    PieceTable pieceTable( createTempFile( "lo World" ) );
    pieceTable.insert( 0, "Hel" );

    auto results = pieceTable.findAll( "Hello" );
    ASSERT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 0 );
}

TEST_F( PieceTableTest, FindAllSpanningMultipleTinyPieces )
{
    PieceTable pieceTable;
    pieceTable.insert( 0, "H" );
    pieceTable.insert( 1, "e" );
    pieceTable.insert( 2, "l" );
    pieceTable.insert( 3, "l" );
    pieceTable.insert( 4, "o" );

    auto results = pieceTable.findAll( "Hello" );
    ASSERT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 0 );
}

TEST_F( PieceTableTest, FindAllNonOverlappingMatches )
{
    PieceTable pieceTable( createTempFile( "ANANA" ) );

    // Non-overlapping semantics: scanning resumes after the consumed match,
    // so "ANA" matches once at 0 (the candidate at 2 overlaps and is skipped).
    auto results = pieceTable.findAll( "ANA" );
    ASSERT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 0 );
}

TEST_F( PieceTableTest, FindAllNonOverlappingRepeatedChar )
{
    PieceTable pieceTable( createTempFile( "tttt" ) );

    auto results = pieceTable.findAll( "ttt" );
    ASSERT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 0 );
}

TEST_F( PieceTableTest, FindAllComplexLPSBranchCoverage )
{
    PieceTable pieceTable( createTempFile( "ABACABAD ABACABAD" ) );

    auto results = pieceTable.findAll( "ABACABAD" );
    ASSERT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 9 );
}

TEST_F( PieceTableTest, FindAllKMPMismatchFallbackInsideSearch )
{
    PieceTable pieceTable( createTempFile( "AABAACAADAABAABA" ) );

    auto results = pieceTable.findAll( "AABA" );
    // Non-overlapping: matches at 0 and 9; the candidate at 12 overlaps the
    // match consumed at 9 (9 + 4 = 13 > 12) and is skipped.
    ASSERT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 9 );
}

TEST_F( PieceTableTest, ReplaceFirstFound )
{
    PieceTable pieceTable( createTempFile( "Hello World Hello" ) );
    bool result = pieceTable.replaceFirst( "Hello", "Hi" );

    EXPECT_TRUE( result );
    EXPECT_EQ( pieceTable.getText(), "Hi World Hello" );
}

TEST_F( PieceTableTest, ReplaceFirstNotFound )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    bool result = pieceTable.replaceFirst( "ZPR", "Test" );

    EXPECT_FALSE( result );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, ReplaceFirstEmptyPattern )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    bool result = pieceTable.replaceFirst( "", "Test" );

    EXPECT_FALSE( result );
    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, ReplaceAllEmptyPatternOrNotFound )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );

    EXPECT_EQ( pieceTable.replaceAll( "", "X" ), 0 );
    EXPECT_EQ( pieceTable.replaceAll( "ZPR", "X" ), 0 );

    EXPECT_EQ( pieceTable.getText(), "Hello World" );
}

TEST_F( PieceTableTest, ReplaceAllWithShorterText )
{
    PieceTable pieceTable( createTempFile( "cat dog cat" ) );
    uint64_t count = pieceTable.replaceAll( "cat", "X" );

    EXPECT_EQ( count, 2 );
    EXPECT_EQ( pieceTable.getText(), "X dog X" );
}

TEST_F( PieceTableTest, ReplaceAllWithLongerText )
{
    PieceTable pieceTable( createTempFile( "A B A" ) );
    uint64_t count = pieceTable.replaceAll( "A", "APPLE" );

    EXPECT_EQ( count, 2 );
    EXPECT_EQ( pieceTable.getText(), "APPLE B APPLE" );
}

TEST_F( PieceTableTest, ReplaceAllWithEmptyTextDeletion )
{
    PieceTable pieceTable( createTempFile( "Test REMOVE string REMOVE" ) );
    uint64_t count = pieceTable.replaceAll( " REMOVE", "" );

    EXPECT_EQ( count, 2 );
    EXPECT_EQ( pieceTable.getText(), "Test string" );
}

TEST_F( PieceTableTest, ReplaceAllAcrossPieceBoundaries )
{
    PieceTable pieceTable( createTempFile( "Hel World" ) );
    pieceTable.insert( 3, "lo" );

    uint64_t count = pieceTable.replaceAll( "Hello", "Hi" );

    EXPECT_EQ( count, 1 );
    EXPECT_EQ( pieceTable.getText(), "Hi World" );
}

TEST_F( PieceTableTest, ReplaceAllConsecutiveOccurrences )
{
    PieceTable pieceTable( createTempFile( "XXX" ) );
    uint64_t count = pieceTable.replaceAll( "X", "Y" );

    EXPECT_EQ( count, 3 );
    EXPECT_EQ( pieceTable.getText(), "YYY" );
}

TEST_F( PieceTableTest, ReplaceAllMassiveFragmentation )
{
    PieceTable pieceTable;
    for( int idx = 0; idx < 5; idx++ ) {
        pieceTable.insert( pieceTable.size(), "A " );
    }

    uint64_t count = pieceTable.replaceAll( "A ", "BB " );

    EXPECT_EQ( count, 5 );
    EXPECT_EQ( pieceTable.getText(), "BB BB BB BB BB " );
}

TEST_F( PieceTableTest, ReplaceAllNonOverlappingPattern )
{
    // Previously overlapping matches ("ttt" at 0 and 1) corrupted replaceAll.
    // Non-overlapping semantics make this safe: one match, no exception.
    PieceTable pieceTable( createTempFile( "tttt" ) );
    uint64_t count = pieceTable.replaceAll( "ttt", "x" );

    EXPECT_EQ( count, 1 );
    EXPECT_EQ( pieceTable.getText(), "xt" );
}

TEST_F( PieceTableTest, ReplaceAllReplacementContainsPattern )
{
    // Replacement contains the pattern; matches come from one pre-scan of the
    // old text, so there is no runaway re-scan/growth.
    PieceTable pieceTable( createTempFile( "aaa" ) );
    uint64_t count = pieceTable.replaceAll( "a", "aa" );

    EXPECT_EQ( count, 3 );
    EXPECT_EQ( pieceTable.getText(), "aaaaaa" );
}

TEST_F( PieceTableTest, ReplaceAllCancelRollsBack )
{
    PieceTable pieceTable( createTempFile( "cat dog cat dog" ) );
    const std::string original = pieceTable.getText();

    std::atomic<bool> cancel{ true };  // canceled before the first iteration
    uint64_t count =
        pieceTable.replaceAll( "cat", "X", true, false, []( uint64_t, uint64_t ) {}, cancel );

    EXPECT_EQ( count, 0 );
    EXPECT_EQ( pieceTable.getText(), original );  // rolled back, unchanged
    EXPECT_FALSE( pieceTable.canUndo() );         // no state committed
}

TEST_F( PieceTableTest, ReplaceAllProgressReportsCompletion )
{
    PieceTable pieceTable( createTempFile( "a a a a a" ) );

    uint64_t lastDone = 0;
    uint64_t lastTotal = 0;
    std::atomic<bool> cancel{ false };
    uint64_t count = pieceTable.replaceAll(
        "a", "b", true, false,
        [&]( uint64_t done, uint64_t total ) {
            lastDone = done;
            lastTotal = total;
        },
        cancel );

    EXPECT_EQ( count, 5 );
    EXPECT_EQ( pieceTable.getText(), "b b b b b" );
    EXPECT_GT( lastTotal, 0U );        // progress was reported (byte-based span)
    EXPECT_EQ( lastDone, lastTotal );  // final progress() call reports 100%
}

TEST_F( PieceTableTest, UndoRedoEmptyHistory )
{
    PieceTable testTable( createTempFile( "Base Text" ) );

    EXPECT_FALSE( testTable.canUndo() );
    EXPECT_FALSE( testTable.canRedo() );

    EXPECT_FALSE( testTable.undo() );
    EXPECT_FALSE( testTable.redo() );

    EXPECT_EQ( testTable.getText(), "Base Text" );
}

TEST_F( PieceTableTest, RedoInvalidationOnNewAction )
{
    PieceTable testTable;
    testTable.insert( 0, "A" );
    testTable.insert( 1, "B" );

    EXPECT_TRUE( testTable.undo() );
    EXPECT_TRUE( testTable.canRedo() );

    testTable.insert( 1, "C" );

    EXPECT_FALSE( testTable.canRedo() );
    EXPECT_FALSE( testTable.redo() );

    EXPECT_EQ( testTable.getText(), "AC" );
}

TEST_F( PieceTableTest, HistorySizeLimitEnforcement )
{
    PieceTable testTable;

    for( int idx = 0; idx < 105; ++idx ) {
        testTable.insert( testTable.size(), "X" );
    }

    EXPECT_EQ( testTable.size(), 105 );

    int successfulUndos = 0;
    while( testTable.undo() ) {
        successfulUndos++;
    }

    EXPECT_EQ( successfulUndos, 100 );
    EXPECT_EQ( testTable.size(), 5 );
    EXPECT_EQ( testTable.getText(), "XXXXX" );
}

TEST_F( PieceTableTest, UndoReplaceAllMassiveOperation )
{
    PieceTable testTable( createTempFile( "cat dog cat dog cat" ) );
    testTable.replaceAll( "cat", "bird" );

    EXPECT_EQ( testTable.getText(), "bird dog bird dog bird" );
    EXPECT_TRUE( testTable.canUndo() );

    testTable.undo();
    EXPECT_EQ( testTable.getText(), "cat dog cat dog cat" );

    testTable.redo();
    EXPECT_EQ( testTable.getText(), "bird dog bird dog bird" );
}

TEST_F( PieceTableTest, ComplexSessionPingPong )
{
    PieceTable testTable( createTempFile( "Start" ) );

    testTable.insert( 5, " A" );
    testTable.insert( 7, " B" );
    testTable.remove( 0, 5 );

    testTable.undo();
    EXPECT_EQ( testTable.getText(), "Start A B" );

    testTable.undo();
    EXPECT_EQ( testTable.getText(), "Start A" );

    testTable.redo();
    EXPECT_EQ( testTable.getText(), "Start A B" );

    testTable.insert( 9, " C" );
    EXPECT_EQ( testTable.getText(), "Start A B C" );

    EXPECT_FALSE( testTable.canRedo() );
}

TEST_F( PieceTableTest, IsDirtyFlagBehavior )
{
    PieceTable testTable( createTempFile( "Clean text" ) );

    EXPECT_FALSE( testTable.isDirty() );

    testTable.insert( 10, " added" );
    EXPECT_TRUE( testTable.isDirty() );

    testTable.undo();
    EXPECT_FALSE( testTable.isDirty() );

    testTable.redo();
    EXPECT_TRUE( testTable.isDirty() );

    std::string savePath = tempFilePath_ + "_saved.txt";
    EXPECT_TRUE( testTable.saveToFile( savePath ) );

    EXPECT_FALSE( testTable.isDirty() );
    std::remove( savePath.c_str() );
}

TEST_F( PieceTableTest, GetFragmentsInRange )
{
    PieceTable testTable( createTempFile( "BaseText" ) );
    testTable.insert( 4, "NEW" );

    auto fragments = testTable.getFragmentsInRange( 2, 7 );

    ASSERT_EQ( fragments.size(), 3 );

    EXPECT_EQ( fragments[0].type_, PieceTable::BufferType::Original );
    EXPECT_EQ( fragments[0].length_, 2 );

    EXPECT_EQ( fragments[1].type_, PieceTable::BufferType::Add );
    EXPECT_EQ( fragments[1].length_, 3 );

    EXPECT_EQ( fragments[2].type_, PieceTable::BufferType::Original );
    EXPECT_EQ( fragments[2].length_, 2 );
}

TEST_F( PieceTableTest, MoveSemanticsTransferOwnership )
{
    PieceTable table1( createTempFile( "Move Me" ) );
    table1.insert( 7, "!" );

    PieceTable table2 = std::move( table1 );

    EXPECT_EQ( table2.size(), 8 );
    EXPECT_EQ( table2.getText(), "Move Me!" );
}

TEST_F( PieceTableTest, CachedSizeStaysConsistentAcrossMutations )
{
    // size() is cached (O(1)); it must equal getText().size() after every mutation,
    // including the snapshot swaps in undo/redo and the transactional replaceAll commit.
    PieceTable pieceTable( createTempFile( "the cat sat on the mat" ) );
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.insert( 0, "Hello! " );
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.remove( 0, 7 );
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.replaceAll( "the", "a" );  // shorter replacement
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.replaceAll( "at", "atat" );  // longer replacement
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.replaceAll( "atat", "" );  // deleting replacement
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.undo();
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    pieceTable.redo();
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );
}

TEST_F( PieceTableTest, ReplaceAllCoalescesFragmentedTable )
{
    // Build a fragmented table out of many small inserts, then replaceAll. The defrag
    // pass must preserve the exact text and the cached size regardless of piece merging.
    PieceTable pieceTable;
    for( int idx = 0; idx < 6; idx++ ) {
        pieceTable.insert( pieceTable.size(), "ab " );
    }
    ASSERT_EQ( pieceTable.getText(), "ab ab ab ab ab ab " );

    uint64_t count = pieceTable.replaceAll( "ab", "X" );

    EXPECT_EQ( count, 6 );
    EXPECT_EQ( pieceTable.getText(), "X X X X X X " );
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );

    // Coalescing is length- and content-preserving: undo restores the original exactly.
    pieceTable.undo();
    EXPECT_EQ( pieceTable.getText(), "ab ab ab ab ab ab " );
    EXPECT_EQ( pieceTable.size(), pieceTable.getText().size() );
}

TEST_F( PieceTableTest, FindPieceAtBinarySearchRoundTrip )
{
    // O(log n) findPieceAt: across a heavily fragmented table, getSubstr (which routes through
    // findPieceAt + the cumulative-offset index) must agree with a flat getText() for every
    // position, length, and piece boundary.
    PieceTable pieceTable( createTempFile( "0123456789" ) );
    for( int idx = 0; idx < 12; idx++ ) {
        // Interleave inserts at varied positions to splinter the table into many pieces.
        pieceTable.insert( static_cast<uint64_t>( idx ) % ( pieceTable.size() + 1 ),
                           "<" + std::to_string( idx ) + ">" );
    }

    const std::string flat = pieceTable.getText();
    ASSERT_EQ( pieceTable.size(), flat.size() );

    for( uint64_t pos = 0; pos < flat.size(); ++pos ) {
        for( uint64_t len : { uint64_t{ 1 }, uint64_t{ 3 }, flat.size() - pos } ) {
            if( pos + len > flat.size() ) {
                continue;
            }
            EXPECT_EQ( pieceTable.getSubstr( pos, len ), flat.substr( pos, len ) )
                << "pos=" << pos << " len=" << len;
        }
    }
}

TEST_F( PieceTableTest, FindPieceAtBoundaryAndOutOfRange )
{
    PieceTable pieceTable( createTempFile( "abcdef" ) );
    pieceTable.insert( 3, "XYZ" );  // fragment into multiple pieces

    const uint64_t end = pieceTable.size();
    // position == size(): findPieceAt returns the past-the-end sentinel; a zero-length read is "".
    EXPECT_EQ( pieceTable.getSubstr( end, 0 ), "" );
    // Last byte is still reachable.
    EXPECT_EQ( pieceTable.getSubstr( end - 1, 1 ), pieceTable.getText().substr( end - 1, 1 ) );
    // Reading past the end throws.
    EXPECT_THROW( pieceTable.getSubstr( end, 1 ), std::out_of_range );
    EXPECT_THROW( pieceTable.getSubstr( end + 5, 1 ), std::out_of_range );
}
