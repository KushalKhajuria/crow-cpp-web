#include "othello.h"

Othello::Othello() : p1(board, 1), p2(board, -1), cur(&p1) {}

void Othello::runGame() {
    
}

void Othello::nextTurn() {
    if (cur == &p1) cur = &p2;
    else cur = &p1;
}

void Othello::setCur(Player* cur) {
    this->cur = cur;
}

bool Othello::placePiece(int row, int col) {
    if (!cur) return false;
    bool ok = cur->placePiece(row, col);
    if (ok) nextTurn();
    return ok;
}

const Board& Othello::getBoard() const {
    return board;
}

int Othello::getCurrentSide() const {
    if (cur == &p1) return 1;
    if (cur == &p2) return -1;
    return 0;
}
