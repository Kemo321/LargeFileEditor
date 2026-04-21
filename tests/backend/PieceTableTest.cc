/**
 * Authors: Tomasz Okon
 * Description: Unit tests for the PieceTable class.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>

#include "backend/PieceTable.h"

// NOLINTBEGIN(readability-magic-numbers)

class PieceTableTest : public ::testing::Test {
protected:
    std::string tempFilePath_ = "test_piece_table_temp.txt";

    // cppcheck-suppress unusedFunction
    void SetUp() override
    {
    }

    // cppcheck-suppress unusedFunction
    void TearDown() override
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
    pieceTable.insert( 1, "B" );  // ABC
    pieceTable.insert( 1, "X" );  // AXBC
    EXPECT_EQ( pieceTable.getText(), "AXBC" );
}

TEST_F( PieceTableTest, RemoveAtBeginning )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    pieceTable.remove( 0, 6 );  // Deletes "Hello "
    EXPECT_EQ( pieceTable.getText(), "World" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveAtEnd )
{
    PieceTable pieceTable( createTempFile( "Hello World" ) );
    pieceTable.remove( 5, 6 );  // Deletes " World"
    EXPECT_EQ( pieceTable.getText(), "Hello" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveInTheMiddle )
{
    PieceTable pieceTable( createTempFile( "Hello CRUEL World" ) );
    pieceTable.remove( 5, 6 );  // Deletes " CRUEL"
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
    pieceTable.insert( 5, " Cruel" );  // "Hello Cruel World"

    // We remove "Cruel Wor" (part from the Add Buffer, part from the Original Buffer)
    // This is a so-called killer test for Piece Table.
    pieceTable.remove( 6, 9 );
    EXPECT_EQ( pieceTable.getText(), "Hello ld" );
}

TEST_F( PieceTableTest, InsertInsideInsertedText )
{
    PieceTable pieceTable( createTempFile( "Start End" ) );
    pieceTable.insert( 6, "The " );  // "Start The End"
    // Now we insert into the middle of text that is already in the Add Buffer
    pieceTable.insert( 10, "Great " );
    EXPECT_EQ( pieceTable.getText(), "Start The Great End" );
}

TEST_F( PieceTableTest, ComplexEditingTypingSimulation )
{
    PieceTable pieceTable( createTempFile( "This is a test." ) );
    pieceTable.remove( 10, 4 );             // "This is a ."
    pieceTable.insert( 10, "cat" );         // "This is a cat."
    pieceTable.insert( 13, " and a dog" );  // "This is a cat and a dog."
    pieceTable.remove( 0, 5 );              // "is a cat and a dog."
    pieceTable.insert( 0, "That " );        // "That is a cat and a dog."
    EXPECT_EQ( pieceTable.getText(), "That is a cat and a dog." );
}

TEST_F( PieceTableTest, InsertEmptyString )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    pieceTable.insert( 2, "" );  // Nothing should change
    EXPECT_EQ( pieceTable.size(), 4 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, RemoveZeroCharacters )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    pieceTable.remove( 2, 0 );  // Nothing should change
    EXPECT_EQ( pieceTable.size(), 4 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, InsertOutOfBoundsThrowsException )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    // Attempting to insert beyond the file size should throw std::out_of_range
    EXPECT_THROW( pieceTable.insert( 5, " Out" ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveOutOfBoundsThrowsException )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    // Attempting to remove beyond the size (start is out of bounds)
    EXPECT_THROW( pieceTable.remove( 5, 1 ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveTooManyCharactersTruncatesOrThrows )
{
    PieceTable pieceTable( createTempFile( "Test" ) );
    // We assume a strict API - it throws if the user asks for too much.
    EXPECT_THROW( pieceTable.remove( 2, 10 ), std::out_of_range );
}

TEST_F( PieceTableTest, RemoveExactlyOnePiece )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    // Insert a temporary chunk.
    pieceTable.insert( 2, "XX" );
    // Remove the inserted chunk.
    pieceTable.remove( 2, 2 );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
    EXPECT_EQ( pieceTable.size(), 5 );
}

TEST_F( PieceTableTest, RemoveCompletelyEliminatingMultipleMiddlePieces )
{
    PieceTable pieceTable( createTempFile( "StartEnd" ) );
    // Build multiple middle pieces.
    pieceTable.insert( 5, "A" );  // StartAEnd
    ASSERT_EQ( pieceTable.getText(), "StartAEnd" );
    pieceTable.insert( 6, "B" );  // StartABEnd
    ASSERT_EQ( pieceTable.getText(), "StartABEnd" );
    pieceTable.insert( 7, "C" );  // StartABCEnd
    ASSERT_EQ( pieceTable.getText(), "StartABCEnd" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 11 ), "StartABCEnd" );
    // Delete across all middle pieces.
    pieceTable.remove( 4, 5 );  // Deletes "ABCEn"
    EXPECT_EQ( pieceTable.getText(), "Starnd" );
}

TEST_F( PieceTableTest, InsertExactlyAtPieceBoundary )
{
    PieceTable pieceTable( createTempFile( "AB" ) );
    // Insert exactly on boundaries.
    pieceTable.insert( 1, "CD" );
    pieceTable.insert( 3, "EF" );
    EXPECT_EQ( pieceTable.getText(), "ACDEFB" );
}

TEST_F( PieceTableTest, DeleteExactlyUpToBoundary )
{
    PieceTable pieceTable( createTempFile( "HelloWorld" ) );
    pieceTable.insert( 5, " " );
    // Delete left side up to boundary.
    pieceTable.remove( 0, 5 );
    EXPECT_EQ( pieceTable.getText(), " World" );
}

TEST_F( PieceTableTest, DeleteExactlyFromBoundary )
{
    PieceTable pieceTable( createTempFile( "HelloWorld" ) );
    pieceTable.insert( 5, " " );
    // Delete right side from boundary.
    pieceTable.remove( 5, 6 );
    EXPECT_EQ( pieceTable.getText(), "Hello" );
}

TEST_F( PieceTableTest, ConsecutiveDeletionsLikeBackspace )
{
    PieceTable pieceTable( createTempFile( "Hello" ) );
    // Simulate backspace key presses.
    pieceTable.remove( 4, 1 );
    pieceTable.remove( 3, 1 );
    pieceTable.remove( 2, 1 );
    EXPECT_EQ( pieceTable.getText(), "He" );
}

TEST_F( PieceTableTest, DeleteAllAndRebuild )
{
    PieceTable pieceTable( createTempFile( "Old Text" ) );
    // Clear everything and insert new text.
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
    // Zero-length delete should do nothing.
    pieceTable.remove( 4, 0 );
    EXPECT_EQ( pieceTable.getText(), "Test" );
}

TEST_F( PieceTableTest, InsertMultipleTimesSequentiallyAtEnd )
{
    PieceTable pieceTable;
    // Build text by appending chars.
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
    // Build text by prepending chars.
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
    // Create a middle piece then delete across pieces.
    pieceTable.insert( 3, "456" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 9 ), "123456789" );
    pieceTable.remove( 2, 5 );
    EXPECT_EQ( pieceTable.getText(), "1289" );
}

TEST_F( PieceTableTest, DeleteLeavingEmptyPieces )
{
    PieceTable pieceTable( createTempFile( "ABCDEF" ) );
    // Insert and remove the same middle segment.
    pieceTable.insert( 3, "XYZ" );
    pieceTable.remove( 3, 3 );
    EXPECT_EQ( pieceTable.getText(), "ABCDEF" );
    EXPECT_EQ( pieceTable.getSubstr( 0, 6 ), "ABCDEF" );
    EXPECT_EQ( pieceTable.size(), 6 );
}

TEST_F( PieceTableTest, ComplexCaseOfMultipleInsertsAndDeletes )
{
    PieceTable pieceTable( createTempFile( "The quick brown fox jumps over the lazy dog." ) );
    pieceTable.remove( 16, 19 );  // Remove "fox jumps over the lazy "
    ASSERT_EQ( pieceTable.getText(), "The quick brown lazy dog." );
    pieceTable.insert( 16, "cat " );  // Insert "cat "
    ASSERT_EQ( pieceTable.getText(), "The quick brown cat lazy dog." );
    pieceTable.insert( 20, "sly " );  // Insert "sly "
    ASSERT_EQ( pieceTable.getText(), "The quick brown cat sly lazy dog." );
    pieceTable.remove( 4, 6 );  // Remove "quick "
    EXPECT_EQ( pieceTable.getText(), "The brown cat sly lazy dog." );
    pieceTable.remove( 0, 4 );  // Remove "The "
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog." );
    pieceTable.remove( pieceTable.size() - 1, 1 );  // Remove "."
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog" );
    EXPECT_EQ( pieceTable.getSubstr( 6, 3 ), "cat" );  // Test getSubstr
    pieceTable.insert( pieceTable.size(), "!" );       // Insert "!" at the end
    EXPECT_EQ( pieceTable.getText(), "brown cat sly lazy dog!" );
    EXPECT_EQ( pieceTable.getSubstr( 6, 3 ), "cat" );  // Test getSubstr
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

TEST_F( PieceTableTest, FindAllOverlappingMatches )
{
    PieceTable pieceTable( createTempFile( "ANANA" ) );

    auto results = pieceTable.findAll( "ANA" );
    ASSERT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 2 );
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
    ASSERT_EQ( results.size(), 3 );
    EXPECT_EQ( results[0], 0 );
    EXPECT_EQ( results[1], 9 );
    EXPECT_EQ( results[2], 12 );
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
    for( int i = 0; i < 5; i++ ) {
        pieceTable.insert( pieceTable.size(), "A " );
    }

    uint64_t count = pieceTable.replaceAll( "A ", "BB " );

    EXPECT_EQ( count, 5 );
    EXPECT_EQ( pieceTable.getText(), "BB BB BB BB BB " );
}

TEST_F( PieceTableTest, UndoRedoEmptyHistory )
{
    PieceTable pt( createTempFile( "Base Text" ) );

    EXPECT_FALSE( pt.canUndo() );
    EXPECT_FALSE( pt.canRedo() );

    EXPECT_FALSE( pt.undo() );
    EXPECT_FALSE( pt.redo() );

    EXPECT_EQ( pt.getText(), "Base Text" );
}

TEST_F( PieceTableTest, RedoInvalidationOnNewAction )
{
    PieceTable pt;
    pt.insert( 0, "A" );
    pt.insert( 1, "B" );

    EXPECT_TRUE( pt.undo() );
    EXPECT_TRUE( pt.canRedo() );

    pt.insert( 1, "C" );

    EXPECT_FALSE( pt.canRedo() );
    EXPECT_FALSE( pt.redo() );

    EXPECT_EQ( pt.getText(), "AC" );
}

TEST_F( PieceTableTest, HistorySizeLimitEnforcement )
{
    PieceTable pt;

    for( int i = 0; i < 105; ++i ) {
        pt.insert( pt.size(), "X" );
    }

    EXPECT_EQ( pt.size(), 105 );

    int successfulUndos = 0;
    while( pt.undo() ) {
        successfulUndos++;
    }

    EXPECT_EQ( successfulUndos, 100 );
    EXPECT_EQ( pt.size(), 5 );
    EXPECT_EQ( pt.getText(), "XXXXX" );
}

TEST_F( PieceTableTest, UndoReplaceAllMassiveOperation )
{
    PieceTable pt( createTempFile( "cat dog cat dog cat" ) );
    pt.replaceAll( "cat", "bird" );

    EXPECT_EQ( pt.getText(), "bird dog bird dog bird" );
    EXPECT_TRUE( pt.canUndo() );

    pt.undo();
    EXPECT_EQ( pt.getText(), "cat dog cat dog cat" );

    pt.redo();
    EXPECT_EQ( pt.getText(), "bird dog bird dog bird" );
}

TEST_F( PieceTableTest, ComplexSessionPingPong )
{
    PieceTable pt( createTempFile( "Start" ) );

    pt.insert( 5, " A" );
    pt.insert( 7, " B" );
    pt.remove( 0, 5 );

    pt.undo();
    EXPECT_EQ( pt.getText(), "Start A B" );

    pt.undo();
    EXPECT_EQ( pt.getText(), "Start A" );

    pt.redo();
    EXPECT_EQ( pt.getText(), "Start A B" );

    pt.insert( 9, " C" );
    EXPECT_EQ( pt.getText(), "Start A B C" );

    EXPECT_FALSE( pt.canRedo() );
}

TEST_F( PieceTableTest, IsDirtyFlagBehavior )
{
    PieceTable pt( createTempFile( "Clean text" ) );

    // Initially not dirty (matches the saved state)
    EXPECT_FALSE( pt.isDirty() );

    pt.insert( 10, " added" );
    EXPECT_TRUE( pt.isDirty() );

    pt.undo();
    // After undoing, it should be back to the saved state (not dirty)
    EXPECT_FALSE( pt.isDirty() );

    pt.redo();
    EXPECT_TRUE( pt.isDirty() );

    // Save the file
    std::string savePath = tempFilePath_ + "_saved.txt";
    EXPECT_TRUE( pt.saveToFile( savePath ) );

    // After saving, it is no longer dirty
    EXPECT_FALSE( pt.isDirty() );
    std::remove( savePath.c_str() );
}

TEST_F( PieceTableTest, GetLineOffsetsBasic )
{
    PieceTable pt( createTempFile( "Line1\nLine2\nLine3" ) );

    auto offsets = pt.getLineOffsets();

    ASSERT_EQ( offsets.size(), 3 );
    EXPECT_EQ( offsets[0], 0 );   // "Line1\n..."
    EXPECT_EQ( offsets[1], 6 );   // "Line2\n..."
    EXPECT_EQ( offsets[2], 12 );  // "Line3"
}

TEST_F( PieceTableTest, GetFragmentsInRange )
{
    PieceTable pt( createTempFile( "BaseText" ) );
    pt.insert( 4, "NEW" );  // "BaseNEWText"

    // Request a range that spans exactly across all 3 pieces:
    // "seNEWTe" -> starts at index 2, length 7
    auto fragments = pt.getFragmentsInRange( 2, 7 );

    ASSERT_EQ( fragments.size(), 3 );

    // Fragment 1: "se" (from Original)
    EXPECT_EQ( fragments[0].type_, PieceTable::BufferType::Original );
    EXPECT_EQ( fragments[0].length_, 2 );

    // Fragment 2: "NEW" (from Add)
    EXPECT_EQ( fragments[1].type_, PieceTable::BufferType::Add );
    EXPECT_EQ( fragments[1].length_, 3 );

    // Fragment 3: "Te" (from Original)
    EXPECT_EQ( fragments[2].type_, PieceTable::BufferType::Original );
    EXPECT_EQ( fragments[2].length_, 2 );
}

TEST_F( PieceTableTest, MoveSemanticsTransferOwnership )
{
    PieceTable pt1( createTempFile( "Move Me" ) );
    pt1.insert( 7, "!" );

    // Move pt1 into pt2
    PieceTable pt2 = std::move( pt1 );

    // pt2 should have the data
    EXPECT_EQ( pt2.size(), 8 );
    EXPECT_EQ( pt2.getText(), "Move Me!" );

    // pt1 should be safely zeroed out (size 0, no crashes on destruction)
    EXPECT_EQ( pt1.size(), 0 );
}

// NOLINTEND(readability-magic-numbers)
