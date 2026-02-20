#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Cell {
    bool mine = false;
    bool revealed = false;
    bool flagged = false;
    int adjacentMines = 0;
};

class Minesweeper {
public:
    Minesweeper(int rows, int cols, int mines)
        : rows_(rows), cols_(cols), mineCount_(mines), board_(rows, std::vector<Cell>(cols)) {
        placeMines();
        calculateAdjacencies();
    }

    void run() {
        std::cout << "\nCommands:\n"
                  << "  r <row> <col>  -> reveal cell\n"
                  << "  f <row> <col>  -> toggle flag\n"
                  << "  q              -> quit\n\n";

        while (true) {
            printBoard(false);

            if (revealedSafeCells_ == rows_ * cols_ - mineCount_) {
                std::cout << "\nYou win! You revealed all safe cells.\n";
                printBoard(true);
                break;
            }

            std::cout << "\nEnter command: ";
            std::string line;
            std::getline(std::cin, line);
            if (line.empty()) {
                continue;
            }

            std::stringstream ss(line);
            char cmd;
            ss >> cmd;

            if (cmd == 'q' || cmd == 'Q') {
                std::cout << "Goodbye!\n";
                break;
            }

            int r, c;
            if (!(ss >> r >> c)) {
                std::cout << "Invalid input. Example: r 2 3\n";
                continue;
            }

            if (!inBounds(r, c)) {
                std::cout << "Out of bounds. Row in [0, " << rows_ - 1
                          << "], Col in [0, " << cols_ - 1 << "]\n";
                continue;
            }

            if (cmd == 'f' || cmd == 'F') {
                toggleFlag(r, c);
            } else if (cmd == 'r' || cmd == 'R') {
                if (!reveal(r, c)) {
                    std::cout << "\nBoom! You hit a mine. Game over.\n";
                    printBoard(true);
                    break;
                }
            } else {
                std::cout << "Unknown command. Use r, f, or q.\n";
            }
        }
    }

private:
    int rows_;
    int cols_;
    int mineCount_;
    int revealedSafeCells_ = 0;
    std::vector<std::vector<Cell>> board_;

    bool inBounds(int r, int c) const {
        return r >= 0 && r < rows_ && c >= 0 && c < cols_;
    }

    void placeMines() {
        std::vector<int> positions(rows_ * cols_);
        for (int i = 0; i < static_cast<int>(positions.size()); ++i) {
            positions[i] = i;
        }

        std::mt19937 rng(static_cast<unsigned int>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        std::shuffle(positions.begin(), positions.end(), rng);

        for (int i = 0; i < mineCount_; ++i) {
            int pos = positions[i];
            int r = pos / cols_;
            int c = pos % cols_;
            board_[r][c].mine = true;
        }
    }

    void calculateAdjacencies() {
        const int dr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        const int dc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                if (board_[r][c].mine) {
                    continue;
                }
                int count = 0;
                for (int i = 0; i < 8; ++i) {
                    int nr = r + dr[i];
                    int nc = c + dc[i];
                    if (inBounds(nr, nc) && board_[nr][nc].mine) {
                        ++count;
                    }
                }
                board_[r][c].adjacentMines = count;
            }
        }
    }

    void toggleFlag(int r, int c) {
        Cell& cell = board_[r][c];
        if (cell.revealed) {
            std::cout << "Cannot flag a revealed cell.\n";
            return;
        }
        cell.flagged = !cell.flagged;
    }

    bool reveal(int r, int c) {
        Cell& start = board_[r][c];
        if (start.revealed) {
            std::cout << "Cell already revealed.\n";
            return true;
        }
        if (start.flagged) {
            std::cout << "Cell is flagged. Unflag it first if you want to reveal it.\n";
            return true;
        }
        if (start.mine) {
            return false;
        }

        floodReveal(r, c);
        return true;
    }

    void floodReveal(int startR, int startC) {
        const int dr[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
        const int dc[8] = {-1, 0, 1, -1, 1, -1, 0, 1};

        std::queue<std::pair<int, int>> q;
        q.push({startR, startC});

        while (!q.empty()) {
            int r = q.front().first;
            int c = q.front().second;
            q.pop();

            Cell& cell = board_[r][c];
            if (cell.revealed || cell.flagged) {
                continue;
            }
            if (cell.mine) {
                continue;
            }

            cell.revealed = true;
            ++revealedSafeCells_;

            if (cell.adjacentMines != 0) {
                continue;
            }

            for (int i = 0; i < 8; ++i) {
                int nr = r + dr[i];
                int nc = c + dc[i];
                if (inBounds(nr, nc) && !board_[nr][nc].revealed && !board_[nr][nc].mine) {
                    q.push({nr, nc});
                }
            }
        }
    }

    void printBoard(bool revealAll) const {
        std::cout << "\n   ";
        for (int c = 0; c < cols_; ++c) {
            std::cout << c << ' ';
        }
        std::cout << "\n";

        for (int r = 0; r < rows_; ++r) {
            std::cout << r << (r < 10 ? "  " : " ");
            for (int c = 0; c < cols_; ++c) {
                const Cell& cell = board_[r][c];
                char ch = '#';

                if (revealAll) {
                    if (cell.mine) ch = '*';
                    else if (cell.adjacentMines == 0) ch = '.';
                    else ch = static_cast<char>('0' + cell.adjacentMines);
                } else {
                    if (cell.flagged) ch = 'F';
                    else if (!cell.revealed) ch = '#';
                    else if (cell.adjacentMines == 0) ch = '.';
                    else ch = static_cast<char>('0' + cell.adjacentMines);
                }

                std::cout << ch << ' ';
            }
            std::cout << "\n";
        }
    }
};

int main() {
    std::cout << "=== Console Minesweeper (C++) ===\n";

    int rows, cols, mines;
    std::cout << "Enter rows cols mines (example: 9 9 10): ";
    if (!(std::cin >> rows >> cols >> mines)) {
        std::cout << "Invalid input.\n";
        return 1;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (rows <= 0 || cols <= 0) {
        std::cout << "Rows and columns must be positive.\n";
        return 1;
    }

    int maxMines = rows * cols - 1;
    if (mines <= 0 || mines > maxMines) {
        std::cout << "Mines must be between 1 and " << maxMines << ".\n";
        return 1;
    }

    Minesweeper game(rows, cols, mines);
    game.run();

    return 0;
}