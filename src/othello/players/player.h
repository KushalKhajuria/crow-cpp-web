#pragma once

#include "../board/board.h"

class Player {
private:
    Board& board;
    int side;
public:
    Player(Board& board, int side);
    bool placePiece(int row, int col);
};
