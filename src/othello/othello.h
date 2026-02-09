#pragma once

#include "board/board.h"
#include "pieces/pieces.h"
#include "players/player.h"

class Othello {
private:
    Board board;
    Player p1;
    Player p2;
    Player* cur;
public:
    Othello();
    // Takes a vector of ints and returns a reversed copy
    void runGame();
    void nextTurn();
    void setCur(Player* cur);
    bool placePiece(int row, int col);
    const Board& getBoard() const;
    int getCurrentSide() const;
};
