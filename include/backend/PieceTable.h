/**
 * Authors: Tomasz Okon
 * Description: Header of the logic layer (backend).
 */

#ifndef PIECETABLE_H
#define PIECETABLE_H

#include <cstdint>

class PieceTable {
public:
    PieceTable();
    uint64_t getSize() const;
    void addSize( uint64_t amount );

private:
    uint64_t size_;
};

#endif
