/**
 * @file FileUtilsTest.cc
 * @brief Unit tests for FileUtils::isBinaryFile.
 */
#include <gtest/gtest.h>

#include <QString>
#include <cstdio>
#include <fstream>
#include <string>

#include "util/FileUtils.h"

class FileUtilsTest : public ::testing::Test {
protected:
    std::string tempFilePath_ = "test_file_utils_temp.bin";

    auto TearDown() -> void override
    {
        std::remove( tempFilePath_.c_str() );
    }

    auto writeFile( const std::string& bytes ) -> QString
    {
        std::ofstream out( tempFilePath_, std::ios::binary );
        out.write( bytes.data(), static_cast<std::streamsize>( bytes.size() ) );
        out.close();
        return QString::fromStdString( tempFilePath_ );
    }
};

TEST_F( FileUtilsTest, PlainAsciiIsNotBinary )
{
    EXPECT_FALSE( FileUtils::isBinaryFile( writeFile( "Hello, world!\nSecond line.\n" ) ) );
}

TEST_F( FileUtilsTest, NulByteIsBinary )
{
    std::string data = "text";
    data.push_back( '\0' );
    data += "more";
    EXPECT_TRUE( FileUtils::isBinaryFile( writeFile( data ) ) );
}

TEST_F( FileUtilsTest, ValidUtf8MultibyteIsNotBinary )
{
    // "café €" with accented and euro characters.
    EXPECT_FALSE( FileUtils::isBinaryFile( writeFile( "caf\xC3\xA9 \xE2\x82\xAC\n" ) ) );
}

TEST_F( FileUtilsTest, HighControlRatioIsBinary )
{
    std::string data( 100, '\x01' );  // all non-text control bytes (no NULs)
    EXPECT_TRUE( FileUtils::isBinaryFile( writeFile( data ) ) );
}

TEST_F( FileUtilsTest, EmptyFileIsNotBinary )
{
    EXPECT_FALSE( FileUtils::isBinaryFile( writeFile( "" ) ) );
}

TEST_F( FileUtilsTest, MissingFileIsNotBinary )
{
    EXPECT_FALSE( FileUtils::isBinaryFile( "definitely_nonexistent_file_xyz.bin" ) );
}
