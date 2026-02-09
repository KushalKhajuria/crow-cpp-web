#pragma once
#include <vector>

using namespace std;

class Board {
private:
    vector<vector<int>> board;
public:
    Board();

    bool addPiece(int row, int col, int side);
    bool validatePlacement(int row, int col);
    void flipVectors(int row, int col, int side);
    void flipVector(int row, int col, int vert, int hori, int side);
    bool flipRecur(int row, int col, int vert, int hori, int side);
    bool emptySpace(int row, int col);
    const vector<vector<int>>& getBoard() const;
    void setBoard(const vector<vector<int>>& next);
};
