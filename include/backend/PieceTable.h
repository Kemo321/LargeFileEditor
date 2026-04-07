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
    [[nodiscard]] auto getSize() const -> uint64_t;
    void addSize( uint64_t amount );

private:
    uint64_t size_{ 0 };
};

#endif
