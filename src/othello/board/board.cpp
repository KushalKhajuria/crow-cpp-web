#include "board.h"

using namespace std;

Board::Board() : board(8, vector<int>(8, 0)) {
    board[3][3] = 1;
    board[3][4] = -1;
    board[4][3] = -1;
    board[4][4] = 1;
}

bool Board::addPiece(int row, int col, int side) {
    if (validatePlacement(row, col) && flipVectors(row, col, side, true)) {
        board[row][col] = side;
        return true;
    }
    return false;
}

bool Board::validatePlacement(int row, int col) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return false;
    if (board[row][col] != 0) return false;
    return true;
}

bool Board::flipVectors(int row, int col, int side, bool flip) {
    bool flipped = false;
    if (flipVector(row, col, -1, -1, side, flip)) flipped = true;
    if (flipVector(row, col, -1, 0, side, flip)) flipped = true;
    if (flipVector(row, col, -1, 1, side, flip)) flipped = true;

    if (flipVector(row, col, 0, -1, side, flip)) flipped = true;
    if (flipVector(row, col, 0, 1, side, flip)) flipped = true;

    if (flipVector(row, col, 1, -1, side, flip)) flipped = true;
    if (flipVector(row, col, 1, 0, side, flip)) flipped = true;
    if (flipVector(row, col, 1, 1, side, flip)) flipped = true;
    return flipped;
}

bool Board::flipVector(int row, int col, int vert, int hori, int side, bool flip) {
    if (!emptySpace(row+vert,col+hori) && board[row+vert][col+hori] == side) return false;
    return flipRecur(row + vert, col + hori, vert, hori, side, flip);
}

// recursive
bool Board::flipRecur(int row, int col, int vert, int hori, int side, bool flip) {
    if (emptySpace(row, col)) return false;
    if (board[row][col] == side) return true;

    if (flipRecur(row + vert, col + hori, vert, hori, side, flip)) {
        if (flip) board[row][col] *= -1;
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

bool Board::anyMoves(int side) {
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == 0 && flipVectors(i, j, side, false)) return true;
        }
    }
    return false;
}

int Board::calcWinner() {
    int firstNum = 0;
    int secondNum = 0;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == -1) firstNum++;
            else if (board[i][j] == 1) secondNum++;
        }
    }

    if (firstNum > secondNum) return -1;
    else if (secondNum > firstNum) return 1;
    return 0;
}
