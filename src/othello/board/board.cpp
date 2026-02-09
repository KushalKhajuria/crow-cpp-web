#include "board.h"

using namespace std;

class Board {
private:
    vector<vector<int>> board;
public:
    Board() {
        board[3][3] = 1;
        board[3][4] = -1;
        board[4][3] = 1;
        board[4][3] = -1;
    }

    void addPiece(int row, int col, int side) {
        if (validatePlacement(row, col)) {
            board[row][col] = side;
            flipVectors(row, col, side);
        } else {
            // reprompt
        }
    }

    bool validatePlacement(int row, int col) {
        if (row < 0 || row > 7 || col < 0 || col > 7) return false;
        if (board[row][col] != 0) return false;
        return true;
    }

    void flipVectors(int row, int col, int side) {
        flipVector(row, col, -1, -1, side);
        flipVector(row, col, -1, 0, side);
        flipVector(row, col, -1, 1, side);

        flipVector(row, col, 0, -1, side);
        flipVector(row, col, 0, 1, side);

        flipVector(row, col, 1, -1, side);
        flipVector(row, col, 1, 0, side);
        flipVector(row, col, 1, 1, side);
    }

    void flipVector(int row, int col, int vert, int hori, int side) {
        flipRecur(row + vert, col + hori, vert, hori, side);
    }

    // recursive
    bool flipRecur(int row, int col, int vert, int hori, int side) {
        if (emptySpace(row, col)) return false;
        if (board[row][col] == side) return true;

        if (flipRecur(row + vert, col + hori, vert, hori, side)) {
            board[row][col] *= -1;
            return true;
        }
        return false;
    }

    bool emptySpace(int row, int col) {
        if (row < 0 || row > 7 || col < 0 || col > 7) return true;
        if (board[row][col] == 0) return true;
        return false;
    }
};