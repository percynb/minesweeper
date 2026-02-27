/*
  Arduino 8x8 Minesweeper (Serial + LED)

  - Board size: 8x8
  - Play from Serial Monitor by typing commands:
      r <row> <col>   -> reveal cell (example: r 2 5)
      f <row> <col>   -> toggle flag (example: f 6 1)
      n               -> start a new game

  Row and column range: 0..7

  LED behavior (LED_BUILTIN):
    - OFF while game is running
    - ON  when player wins
    - FAST BLINK when player hits a mine (game over)
*/

const byte SIZE = 8;
const byte MINES = 10;

struct Cell {
  bool mine;
  bool revealed;
  bool flagged;
  byte adjacent;
};

Cell board[SIZE][SIZE];
bool gameOver = false;
bool gameWon = false;
unsigned long lastBlinkMs = 0;
bool blinkState = false;

// ---------- Utility ----------
bool inBounds(int r, int c) {
  return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}

void clearBoard() {
  for (byte r = 0; r < SIZE; r++) {
    for (byte c = 0; c < SIZE; c++) {
      board[r][c].mine = false;
      board[r][c].revealed = false;
      board[r][c].flagged = false;
      board[r][c].adjacent = 0;
    }
  }
}

byte countAdjacentMines(byte row, byte col) {
  byte count = 0;
  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      if (dr == 0 && dc == 0) continue;
      int nr = row + dr;
      int nc = col + dc;
      if (inBounds(nr, nc) && board[nr][nc].mine) {
        count++;
      }
    }
  }
  return count;
}

void placeMines() {
  byte placed = 0;
  while (placed < MINES) {
    byte r = random(SIZE);
    byte c = random(SIZE);
    if (!board[r][c].mine) {
      board[r][c].mine = true;
      placed++;
    }
  }

  for (byte r = 0; r < SIZE; r++) {
    for (byte c = 0; c < SIZE; c++) {
      board[r][c].adjacent = countAdjacentMines(r, c);
    }
  }
}

void printBoard(bool revealAll) {
  Serial.println();
  Serial.println(F("   0 1 2 3 4 5 6 7"));
  for (byte r = 0; r < SIZE; r++) {
    Serial.print(r);
    Serial.print(F("  "));
    for (byte c = 0; c < SIZE; c++) {
      char out;
      Cell &cell = board[r][c];

      if (revealAll && cell.mine) {
        out = '*';
      } else if (cell.revealed) {
        if (cell.mine) out = '*';
        else if (cell.adjacent == 0) out = '.';
        else out = '0' + cell.adjacent;
      } else if (cell.flagged) {
        out = 'F';
      } else {
        out = '#';
      }

      Serial.print(out);
      Serial.print(' ');
    }
    Serial.println();
  }
}

void floodReveal(byte row, byte col) {
  if (!inBounds(row, col)) return;

  Cell &cell = board[row][col];
  if (cell.revealed || cell.flagged) return;

  cell.revealed = true;
  if (cell.adjacent != 0) return;

  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      if (dr == 0 && dc == 0) continue;
      int nr = row + dr;
      int nc = col + dc;
      if (inBounds(nr, nc)) {
        floodReveal(nr, nc);
      }
    }
  }
}

bool checkWin() {
  for (byte r = 0; r < SIZE; r++) {
    for (byte c = 0; c < SIZE; c++) {
      if (!board[r][c].mine && !board[r][c].revealed) {
        return false;
      }
    }
  }
  return true;
}

void startNewGame() {
  clearBoard();
  placeMines();
  gameOver = false;
  gameWon = false;
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println(F("\n=== New 8x8 Minesweeper Game ==="));
  Serial.println(F("Commands: r row col | f row col | n"));
  printBoard(false);
}

void revealCell(byte row, byte col) {
  if (!inBounds(row, col)) {
    Serial.println(F("Invalid coordinate (use 0..7)."));
    return;
  }

  Cell &cell = board[row][col];
  if (cell.revealed) {
    Serial.println(F("Cell already revealed."));
    return;
  }
  if (cell.flagged) {
    Serial.println(F("Cell is flagged. Unflag first."));
    return;
  }

  if (cell.mine) {
    gameOver = true;
    gameWon = false;
    Serial.println(F("BOOM! You hit a mine."));
    printBoard(true);
    Serial.println(F("Type 'n' to start a new game."));
    return;
  }

  floodReveal(row, col);

  if (checkWin()) {
    gameOver = true;
    gameWon = true;
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println(F("Congratulations! You cleared the board."));
    printBoard(true);
    Serial.println(F("Type 'n' to start a new game."));
    return;
  }

  printBoard(false);
}

void toggleFlag(byte row, byte col) {
  if (!inBounds(row, col)) {
    Serial.println(F("Invalid coordinate (use 0..7)."));
    return;
  }

  Cell &cell = board[row][col];
  if (cell.revealed) {
    Serial.println(F("Cannot flag a revealed cell."));
    return;
  }

  cell.flagged = !cell.flagged;
  printBoard(false);
}

void parseCommand() {
  if (!Serial.available()) return;

  char cmd = tolower(Serial.read());

  if (cmd == '\n' || cmd == '\r' || cmd == ' ') {
    return;
  }

  if (cmd == 'n') {
    while (Serial.available()) Serial.read();
    startNewGame();
    return;
  }

  int row = Serial.parseInt();
  int col = Serial.parseInt();
  while (Serial.available()) Serial.read();

  if (row < 0 || row >= SIZE || col < 0 || col >= SIZE) {
    Serial.println(F("Use format: r 0..7 0..7 or f 0..7 0..7"));
    return;
  }

  if (gameOver) {
    Serial.println(F("Game is over. Type 'n' for a new game."));
    return;
  }

  if (cmd == 'r') {
    revealCell((byte)row, (byte)col);
  } else if (cmd == 'f') {
    toggleFlag((byte)row, (byte)col);
  } else {
    Serial.println(F("Unknown command. Use r, f, or n."));
  }
}

void updateLedState() {
  if (!gameOver) {
    digitalWrite(LED_BUILTIN, LOW);
    return;
  }

  if (gameWon) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    // Lost game => blink LED quickly.
    unsigned long now = millis();
    if (now - lastBlinkMs >= 200) {
      lastBlinkMs = now;
      blinkState = !blinkState;
      digitalWrite(LED_BUILTIN, blinkState ? HIGH : LOW);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(9600);
  while (!Serial) {
    ;  // Needed on some boards (like Leonardo)
  }

  randomSeed(analogRead(A0));
  startNewGame();
}

void loop() {
  parseCommand();
  updateLedState();
}
