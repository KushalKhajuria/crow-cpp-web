#include "board.h"

using namespace std;

Board::Board() : board(8, vector<int>(8, 0)) {
    board[3][3] = 1;
    board[3][4] = -1;
    board[4][3] = -1;
    board[4][4] = 1;
}

bool Board::addPiece(int row, int col, int side) {
    if (validatePlacement(row, col)) {
        board[row][col] = side;
        flipVectors(row, col, side);
        return true;
    } 
    return false;
}

bool Board::validatePlacement(int row, int col) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return false;
    if (board[row][col] != 0) return false;
    return true;
}

void Board::flipVectors(int row, int col, int side) {
    flipVector(row, col, -1, -1, side);
    flipVector(row, col, -1, 0, side);
    flipVector(row, col, -1, 1, side);

    flipVector(row, col, 0, -1, side);
    flipVector(row, col, 0, 1, side);

    flipVector(row, col, 1, -1, side);
    flipVector(row, col, 1, 0, side);
    flipVector(row, col, 1, 1, side);
}

void Board::flipVector(int row, int col, int vert, int hori, int side) {
    flipRecur(row + vert, col + hori, vert, hori, side);
}

// recursive
bool Board::flipRecur(int row, int col, int vert, int hori, int side) {
    if (emptySpace(row, col)) return false;
    if (board[row][col] == side) return true;

    if (flipRecur(row + vert, col + hori, vert, hori, side)) {
        board[row][col] *= -1;
        return true;
    }
    return false;
}

bool Board::emptySpace(int row, int col) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return true;
    if (board[row][col] == 0) return true;
    return false;
}

const vector<vector<int>>& Board::getBoard() const {
    return board;
}

void Board::setBoard(const vector<vector<int>>& next) {
    board = next;
}
