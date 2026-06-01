/**
 * @file MemoryMappedFileTest.cc
 * @brief Unit tests for the MemoryMappedFile RAII wrapper.
 */
#include <gtest/gtest.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include "backend/MemoryMappedFile.h"

class MemoryMappedFileTest : public ::testing::Test {
protected:
    std::string tempFilePath_ = "test_mmap_temp.txt";
    std::string secondFilePath_ = "test_mmap_temp_2.txt";

    auto TearDown() -> void override
    {
        // A permission-stripped file must be made removable again before cleanup.
        std::error_code ec;
        std::filesystem::permissions(
            tempFilePath_, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add, ec );
        std::remove( tempFilePath_.c_str() );
        std::remove( secondFilePath_.c_str() );
    }

    static auto writeTo( const std::string& path, const std::string& text ) -> std::string
    {
        std::ofstream out( path, std::ios::binary );
        out.write( text.data(), static_cast<std::streamsize>( text.size() ) );
        out.close();
        return path;
    }

    auto writeFile( const std::string& text ) -> std::string
    {
        return writeTo( tempFilePath_, text );
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

TEST_F( MemoryMappedFileTest, MoveAssignmentOperator )
{
    const std::string contentA = "the original mapping";
    const std::string contentB = "a second, different mapping";

    // Destination starts out already holding a valid mapping of file A.
    MemoryMappedFile dest( writeFile( contentA ) );
    ASSERT_TRUE( dest.isValid() );

    MemoryMappedFile source( writeTo( secondFilePath_, contentB ) );
    ASSERT_TRUE( source.isValid() );

    // Move-assign: the old mapping of A must be released (closeMmap) and B taken over.
    dest = std::move( source );
    EXPECT_TRUE( dest.isValid() );
    EXPECT_EQ( std::string( dest.data(), dest.size() ), contentB );

    // NOLINTNEXTLINE(bugprone-use-after-move) — verifying the moved-from state
    EXPECT_FALSE( source.isValid() );
    EXPECT_EQ( source.data(), nullptr );
    EXPECT_EQ( source.size(), 0U );

    // Self-assignment must be a guarded no-op (no double-free, mapping preserved). Aliasing
    // through a pointer keeps the compiler from diagnosing the obvious self-move.
    auto* selfAlias = &dest;
    dest = std::move( *selfAlias );
    EXPECT_TRUE( dest.isValid() );
    EXPECT_EQ( std::string( dest.data(), dest.size() ), contentB );
}

TEST_F( MemoryMappedFileTest, MapEmptyFile )
{
    MemoryMappedFile file( writeFile( "" ) );  // a real, zero-byte file

    // mmap of a zero-length region cannot satisfy the data_ != nullptr && size_ > 0 invariant.
    EXPECT_FALSE( file.isValid() );
    EXPECT_EQ( file.size(), 0U );
}

TEST_F( MemoryMappedFileTest, MapFileWithoutPermissions )
{
    if( geteuid() == 0 ) {
        GTEST_SKIP() << "Running as root bypasses filesystem permission checks.";
    }

    const std::string path = writeFile( "unreadable bytes" );
    std::error_code ec;
    std::filesystem::permissions( path, std::filesystem::perms::none, ec );
    ASSERT_FALSE( ec );

    // open(O_RDONLY) fails with EACCES; the constructor must close its fd and stay invalid.
    MemoryMappedFile file( path );
    EXPECT_FALSE( file.isValid() );
    EXPECT_EQ( file.data(), nullptr );
    EXPECT_EQ( file.size(), 0U );
}
