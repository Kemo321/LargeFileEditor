/**
 * Authors: Tomasz Okon
 * Description: Implementation of the logic layer.
 */

#include "backend/PieceTable.h"

using namespace std;

PieceTable::PieceTable() = default;

auto PieceTable::getSize() const -> uint64_t
{
    return size_;
}

void PieceTable::addSize( uint64_t amount )
{
    size_ += amount;
}
