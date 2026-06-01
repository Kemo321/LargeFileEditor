/**
 * @file MemoryMappedFileTest.cc
 * @brief Unit tests for the MemoryMappedFile RAII wrapper.
 */
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <utility>

#include "backend/MemoryMappedFile.h"

class MemoryMappedFileTest : public ::testing::Test {
protected:
    std::string tempFilePath_ = "test_mmap_temp.txt";

    auto TearDown() -> void override
    {
        std::remove( tempFilePath_.c_str() );
    }

    auto writeFile( const std::string& text ) -> std::string
    {
        std::ofstream out( tempFilePath_, std::ios::binary );
        out.write( text.data(), static_cast<std::streamsize>( text.size() ) );
        out.close();
        return tempFilePath_;
    }
};

TEST_F( MemoryMappedFileTest, MapsFileContents )
{
    const std::string content = "Hello, mmap world!";
    MemoryMappedFile file( writeFile( content ) );

    ASSERT_TRUE( file.isValid() );
    EXPECT_EQ( file.size(), content.size() );
    ASSERT_NE( file.data(), nullptr );
    EXPECT_EQ( std::string( file.data(), file.size() ), content );
}

TEST_F( MemoryMappedFileTest, NonexistentFileIsInvalid )
{
    MemoryMappedFile file( "definitely_nonexistent_file_xyz.txt" );
    EXPECT_FALSE( file.isValid() );
    EXPECT_EQ( file.data(), nullptr );
    EXPECT_EQ( file.size(), 0U );
}

TEST_F( MemoryMappedFileTest, DefaultConstructedIsInvalid )
{
    MemoryMappedFile file;
    EXPECT_FALSE( file.isValid() );
    EXPECT_EQ( file.data(), nullptr );
}

TEST_F( MemoryMappedFileTest, MoveTransfersOwnership )
{
    const std::string content = "movable bytes";
    MemoryMappedFile source( writeFile( content ) );
    ASSERT_TRUE( source.isValid() );

    MemoryMappedFile moved( std::move( source ) );
    EXPECT_TRUE( moved.isValid() );
    EXPECT_EQ( std::string( moved.data(), moved.size() ), content );

    // NOLINTNEXTLINE(bugprone-use-after-move) — verifying the moved-from state
    EXPECT_FALSE( source.isValid() );
    EXPECT_EQ( source.data(), nullptr );
}
