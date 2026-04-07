/**
 * Authors: Tomasz Okon
 * Description: Implementation of the logic layer.
 */

#include "backend/PieceTable.h"

using namespace std;

PieceTable::PieceTable() : size_( 0 )
{
}

uint64_t PieceTable::getSize() const
{
    return size_;
}

void PieceTable::addSize( uint64_t amount )
{
    size_ += amount;
}
