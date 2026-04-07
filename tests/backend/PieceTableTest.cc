/**
 * Authors: Tomasz Okon, Jan Szwagierczak
 * Description: Unit tests for the PieceTable class.
 */

#include <gtest/gtest.h>

#include "backend/PieceTable.h"

using namespace std;

TEST( PieceTableTest, InitializationSizeIsZero )
{
    PieceTable table;
    EXPECT_EQ( table.getSize(), 0 );
}

TEST( PieceTableTest, AddSizeWorksProperly )
{
    PieceTable table;
    table.addSize( 50 );
    EXPECT_EQ( table.getSize(), 50 );
}
