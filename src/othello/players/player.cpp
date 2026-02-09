#include "player.h"

Player::Player(Board& board, int side) : board(board), side(side) {}

bool Player::placePiece(int row, int col) {
    return board.addPiece(row, col, side);
}
