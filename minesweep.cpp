#include <Arduino.h>
#include <LedControl.h>

const byte SIZE = 8;
const byte MINES = 10;

// ---------- Pin mapping ----------
const byte PIN_DIN = 12;   // MAX7219 DIN
const byte PIN_CLK = 11;   // MAX7219 CLK
const byte PIN_CS  = 10;   // MAX7219 CS

const byte PIN_JOY_X = A0;
const byte PIN_JOY_Y = A1;
const byte PIN_JOY_SW = 2;      // Active LOW with INPUT_PULLUP
const byte PIN_PWR_BTN = 3;     // Active LOW with INPUT_PULLUP

LedControl matrix(PIN_DIN, PIN_CLK, PIN_CS, 1);

struct Cell {
  bool mine;
  bool revealed;
  bool flagged;
  byte adjacent;
};

Cell board[SIZE][SIZE];

bool poweredOn = true;
bool gameOver = false;
bool gameWon = false;

byte cursorRow = 0;
byte cursorCol = 0;

unsigned long lastAnimMs = 0;
bool animPhase = false;

unsigned long lastMoveMs = 0;
const unsigned long MOVE_INTERVAL_MS = 180;
const int JOY_LOW = 350;
const int JOY_HIGH = 700;

bool lastJoySwState = HIGH;
unsigned long joyPressStartMs = 0;
const unsigned long LONG_PRESS_MS = 500;

bool lastPwrBtnState = HIGH;
unsigned long lastPwrDebounceMs = 0;
const unsigned long BTN_DEBOUNCE_MS = 40;

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
      if (inBounds(nr, nc) && board[nr][nc].mine) count++;
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

void floodReveal(byte row, byte col) {
  if (!inBounds(row, col)) return;

  Cell &cell = board[row][col];
  if (cell.revealed || cell.flagged) return;

  cell.revealed = true;
  if (cell.adjacent != 0) return;

  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      if (dr == 0 && dc == 0) continue;
      floodReveal(row + dr, col + dc);
    }
  }
}

bool checkWin() {
  for (byte r = 0; r < SIZE; r++) {
    for (byte c = 0; c < SIZE; c++) {
      if (!board[r][c].mine && !board[r][c].revealed) return false;
    }
  }
  return true;
}

void startNewGame() {
  clearBoard();
  placeMines();
  gameOver = false;
  gameWon = false;
  cursorRow = 0;
  cursorCol = 0;

  Serial.println(F("New game started."));
}

void revealCell(byte row, byte col) {
  if (!inBounds(row, col) || gameOver) return;

  Cell &cell = board[row][col];
  if (cell.revealed || cell.flagged) return;

  if (cell.mine) {
    gameOver = true;
    gameWon = false;
    Serial.println(F("BOOM! Mine hit."));
    return;
  }

  floodReveal(row, col);

  if (checkWin()) {
    gameOver = true;
    gameWon = true;
    Serial.println(F("You win!"));
  }
}

void toggleFlag(byte row, byte col) {
  if (!inBounds(row, col) || gameOver) return;

  Cell &cell = board[row][col];
  if (cell.revealed) return;
  cell.flagged = !cell.flagged;
}

void setMatrixPixel(byte r, byte c, bool on) {
  matrix.setLed(0, r, c, on);
}

void clearMatrix() {
  matrix.clearDisplay(0);
}

void renderBoard() {
  if (!poweredOn) {
    clearMatrix();
    return;
  }

  unsigned long now = millis();
  if (now - lastAnimMs >= 250) {
    lastAnimMs = now;
    animPhase = !animPhase;
  }

  for (byte r = 0; r < SIZE; r++) {
    for (byte c = 0; c < SIZE; c++) {
      bool on = false;
      Cell &cell = board[r][c];

      if (gameOver && !gameWon && cell.mine) {
        on = animPhase;  // blink all mines on loss
      } else if (cell.revealed) {
        on = true;
      } else if (cell.flagged) {
        on = animPhase;  // blink flag
      }

      if (!gameOver && r == cursorRow && c == cursorCol) {
        on = !on && animPhase;  // blinking cursor overlay
      }

      setMatrixPixel(r, c, on);
    }
  }
}

void handlePowerButton() {
  bool current = digitalRead(PIN_PWR_BTN);
  unsigned long now = millis();

  if (current != lastPwrBtnState) {
    lastPwrDebounceMs = now;
    lastPwrBtnState = current;
  }

  if ((now - lastPwrDebounceMs) < BTN_DEBOUNCE_MS) return;

  static bool pressedHandled = false;
  if (current == LOW && !pressedHandled) {
    pressedHandled = true;
    poweredOn = !poweredOn;

    if (poweredOn) {
      Serial.println(F("Power ON"));
      startNewGame();
    } else {
      Serial.println(F("Power OFF"));
      clearMatrix();
    }
  }

  if (current == HIGH) {
    pressedHandled = false;
  }
}

void moveCursor(int dRow, int dCol) {
  int nr = (int)cursorRow + dRow;
  int nc = (int)cursorCol + dCol;

  if (nr < 0) nr = 0;
  if (nr >= SIZE) nr = SIZE - 1;
  if (nc < 0) nc = 0;
  if (nc >= SIZE) nc = SIZE - 1;

  cursorRow = (byte)nr;
  cursorCol = (byte)nc;
}

void handleJoystickMove() {
  if (!poweredOn || gameOver) return;

  unsigned long now = millis();
  if (now - lastMoveMs < MOVE_INTERVAL_MS) return;

  int x = analogRead(PIN_JOY_X);
  int y = analogRead(PIN_JOY_Y);

  int dRow = 0;
  int dCol = 0;

  if (x < JOY_LOW) dCol = -1;
  else if (x > JOY_HIGH) dCol = 1;

  if (y < JOY_LOW) dRow = 1;
  else if (y > JOY_HIGH) dRow = -1;

  if (dRow != 0 || dCol != 0) {
    moveCursor(dRow, dCol);
    lastMoveMs = now;
  }
}

void handleJoystickPress() {
  if (!poweredOn) return;

  bool current = digitalRead(PIN_JOY_SW);
  unsigned long now = millis();

  // Press start
  if (lastJoySwState == HIGH && current == LOW) {
    joyPressStartMs = now;
  }

  // Release => short/long press action
  if (lastJoySwState == LOW && current == HIGH) {
    unsigned long pressMs = now - joyPressStartMs;
    if (!gameOver) {
      if (pressMs >= LONG_PRESS_MS) {
        toggleFlag(cursorRow, cursorCol);
      } else {
        revealCell(cursorRow, cursorCol);
      }
    } else if (gameWon) {
      startNewGame();
    }
  }

  lastJoySwState = current;
}

void setup() {
  Serial.begin(9600);

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_PWR_BTN, INPUT_PULLUP);

  matrix.shutdown(0, false);
  matrix.setIntensity(0, 6);
  clearMatrix();

  randomSeed(analogRead(A2));
  startNewGame();

  Serial.println(F("Minesweeper ready."));
  Serial.println(F("Short press joystick = reveal, long press = flag."));
  Serial.println(F("Power button toggles game on/off."));
}

void loop() {
  handlePowerButton();
  handleJoystickMove();
  handleJoystickPress();
  renderBoard();
}
