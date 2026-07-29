/*
 * Autonomous Snake demo for Adafruit 20x4 HD44780 + I2C backpack
 * on Arduino Leonardo (SDA=D2, SCL=D3).
 *
 * Library: Adafruit LiquidCrystal (I2C mode, address offset 0).
 */

#include "Adafruit_LiquidCrystal.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const uint8_t COLS = 20;
static const uint8_t ROWS = 4;
static const uint8_t SNAKE_LEN = 8;
static const uint16_t STEP_MS = 250;
static const uint8_t I2C_ADDR_OFFSET = 0;

// CGRAM indices
static const uint8_t CHAR_HEAD_N = 0;
static const uint8_t CHAR_HEAD_E = 1;
static const uint8_t CHAR_HEAD_S = 2;
static const uint8_t CHAR_HEAD_W = 3;
static const uint8_t CHAR_BODY_V = 4;  // N/S body
static const uint8_t CHAR_BODY_H = 5;  // E/W body
static const uint8_t CHAR_FOOD = 6;

enum Dir : int8_t { DIR_N = 0, DIR_E = 1, DIR_S = 2, DIR_W = 3 };

struct Seg {
  int8_t x;
  int8_t y;
};

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
Adafruit_LiquidCrystal lcd(I2C_ADDR_OFFSET);

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
Seg snake[SNAKE_LEN];
uint8_t headIdx = 0;  // snake[headIdx] is head; tail is (headIdx+1)%SNAKE_LEN
Dir dir = DIR_E;
Seg food;
unsigned long lastStepMs = 0;

// Frame buffers for dirty-cell LCD flush
char frame[ROWS][COLS];
char prevFrame[ROWS][COLS];

// ---------------------------------------------------------------------------
// Custom glyphs (5x8), thinner than a full block
// ---------------------------------------------------------------------------
static const byte glyphHeadN[8] = {
  B00100,
  B01110,
  B01110,
  B01110,
  B00100,
  B00000,
  B00000,
  B00000
};
static const byte glyphHeadE[8] = {
  B00000,
  B01100,
  B01110,
  B01111,
  B01110,
  B01100,
  B00000,
  B00000
};
static const byte glyphHeadS[8] = {
  B00000,
  B00000,
  B00000,
  B00100,
  B01110,
  B01110,
  B01110,
  B00100
};
static const byte glyphHeadW[8] = {
  B00000,
  B00110,
  B01110,
  B11110,
  B01110,
  B00110,
  B00000,
  B00000
};
static const byte glyphBodyV[8] = {
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100,
  B00100
};
static const byte glyphBodyH[8] = {
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B00000,
  B00000,
  B00000
};
static const byte glyphFood[8] = {
  B00000,
  B00000,
  B00100,
  B01110,
  B00100,
  B00000,
  B00000,
  B00000
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline int8_t wrapX(int8_t x) {
  return (int8_t)((x + COLS) % COLS);
}

static inline int8_t wrapY(int8_t y) {
  return (int8_t)((y + ROWS) % ROWS);
}

/** Shortest signed wrap delta from `from` toward `to` on a ring of `size`. */
static int8_t wrapDelta(int8_t from, int8_t to, uint8_t size) {
  int8_t d = (int8_t)(to - from);
  int8_t half = (int8_t)(size / 2);
  if (d > half) {
    d = (int8_t)(d - size);
  } else if (d < -half) {
    d = (int8_t)(d + size);
  }
  return d;
}

static void dirDelta(Dir d, int8_t &dx, int8_t &dy) {
  dx = 0;
  dy = 0;
  switch (d) {
    case DIR_N: dy = -1; break;
    case DIR_E: dx = 1; break;
    case DIR_S: dy = 1; break;
    case DIR_W: dx = -1; break;
  }
}

static uint8_t headChar(Dir d) {
  switch (d) {
    case DIR_N: return CHAR_HEAD_N;
    case DIR_E: return CHAR_HEAD_E;
    case DIR_S: return CHAR_HEAD_S;
    case DIR_W: return CHAR_HEAD_W;
  }
  return CHAR_HEAD_E;
}

static uint8_t bodyChar(Dir d) {
  return (d == DIR_N || d == DIR_S) ? CHAR_BODY_V : CHAR_BODY_H;
}

static uint8_t tailIdx() {
  return (uint8_t)((headIdx + 1) % SNAKE_LEN);
}

/** True if (x,y) is occupied by snake; optionally ignore vacating tail cell. */
static bool occupied(int8_t x, int8_t y, bool ignoreTail) {
  uint8_t skip = ignoreTail ? tailIdx() : 255;
  for (uint8_t i = 0; i < SNAKE_LEN; i++) {
    if (i == skip) {
      continue;
    }
    if (snake[i].x == x && snake[i].y == y) {
      return true;
    }
  }
  return false;
}

static void spawnFood() {
  do {
    food.x = (int8_t)random(COLS);
    food.y = (int8_t)random(ROWS);
  } while (occupied(food.x, food.y, false));
}

static void pushUniqueDir(Dir preferred[], uint8_t &n, Dir d) {
  for (uint8_t i = 0; i < n; i++) {
    if (preferred[i] == d) {
      return;
    }
  }
  preferred[n++] = d;
}

// ---------------------------------------------------------------------------
// Drawing — rebuild desired frame, flush only changed cells
// ---------------------------------------------------------------------------
static void clearFrame(char buf[ROWS][COLS]) {
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      buf[r][c] = ' ';
    }
  }
}

static void buildFrame() {
  clearFrame(frame);

  // Body (all but head), oriented toward the next segment toward the head
  for (uint8_t i = 1; i < SNAKE_LEN; i++) {
    uint8_t idx = (uint8_t)((headIdx + SNAKE_LEN - i) % SNAKE_LEN);
    uint8_t nextIdx = (uint8_t)((headIdx + SNAKE_LEN - (i - 1)) % SNAKE_LEN);
    int8_t dx = wrapDelta(snake[idx].x, snake[nextIdx].x, COLS);
    int8_t dy = wrapDelta(snake[idx].y, snake[nextIdx].y, ROWS);
    Dir segDir;
    if (dy < 0) {
      segDir = DIR_N;
    } else if (dy > 0) {
      segDir = DIR_S;
    } else if (dx < 0) {
      segDir = DIR_W;
    } else {
      segDir = DIR_E;
    }
    frame[snake[idx].y][snake[idx].x] = (char)bodyChar(segDir);
  }

  Seg &h = snake[headIdx];
  frame[h.y][h.x] = (char)headChar(dir);

  if (!(food.x == h.x && food.y == h.y)) {
    frame[food.y][food.x] = (char)CHAR_FOOD;
  }
}

static void flushFrame(bool forceAll) {
  for (uint8_t r = 0; r < ROWS; r++) {
    for (uint8_t c = 0; c < COLS; c++) {
      char ch = frame[r][c];
      if (forceAll || ch != prevFrame[r][c]) {
        lcd.setCursor(c, r);
        lcd.write(ch);
        prevFrame[r][c] = ch;
      }
    }
  }
}

static void draw(bool forceAll) {
  buildFrame();
  flushFrame(forceAll);
}

// ---------------------------------------------------------------------------
// AI — wrap-aware chase with body avoidance
// ---------------------------------------------------------------------------
static Dir chooseMove() {
  Seg &h = snake[headIdx];
  int8_t dx = wrapDelta(h.x, food.x, COLS);
  int8_t dy = wrapDelta(h.y, food.y, ROWS);

  Dir preferred[4];
  uint8_t n = 0;

  // Prefer primary axis (larger abs delta), then secondary, then remaining.
  if (abs(dx) >= abs(dy)) {
    if (dx > 0) {
      pushUniqueDir(preferred, n, DIR_E);
    } else if (dx < 0) {
      pushUniqueDir(preferred, n, DIR_W);
    }
    if (dy > 0) {
      pushUniqueDir(preferred, n, DIR_S);
    } else if (dy < 0) {
      pushUniqueDir(preferred, n, DIR_N);
    }
  } else {
    if (dy > 0) {
      pushUniqueDir(preferred, n, DIR_S);
    } else if (dy < 0) {
      pushUniqueDir(preferred, n, DIR_N);
    }
    if (dx > 0) {
      pushUniqueDir(preferred, n, DIR_E);
    } else if (dx < 0) {
      pushUniqueDir(preferred, n, DIR_W);
    }
  }

  pushUniqueDir(preferred, n, DIR_N);
  pushUniqueDir(preferred, n, DIR_E);
  pushUniqueDir(preferred, n, DIR_S);
  pushUniqueDir(preferred, n, DIR_W);

  for (uint8_t i = 0; i < n; i++) {
    int8_t mdx, mdy;
    dirDelta(preferred[i], mdx, mdy);
    int8_t nx = wrapX((int8_t)(h.x + mdx));
    int8_t ny = wrapY((int8_t)(h.y + mdy));
    if (!occupied(nx, ny, true)) {
      return preferred[i];
    }
  }

  // All collide — still move so the demo never hard-stalls
  return preferred[0];
}

static void stepGame() {
  dir = chooseMove();

  int8_t dx, dy;
  dirDelta(dir, dx, dy);

  Seg &h = snake[headIdx];
  Seg newHead;
  newHead.x = wrapX((int8_t)(h.x + dx));
  newHead.y = wrapY((int8_t)(h.y + dy));

  bool ate = (newHead.x == food.x && newHead.y == food.y);

  uint8_t newHeadIdx = tailIdx();
  snake[newHeadIdx] = newHead;
  headIdx = newHeadIdx;

  if (ate) {
    spawnFood();
  }
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------
static void initSnake() {
  // Horizontal snake centered, facing east: head at mid, body west.
  // Ring order from head toward tail: headIdx, headIdx-1, ..., tailIdx.
  int8_t midX = COLS / 2;
  int8_t midY = ROWS / 2;
  headIdx = 0;
  dir = DIR_E;

  Seg ordered[SNAKE_LEN];
  for (uint8_t i = 0; i < SNAKE_LEN; i++) {
    ordered[i].x = wrapX((int8_t)(midX - (int8_t)i));
    ordered[i].y = midY;
  }

  snake[0] = ordered[0];  // head
  for (uint8_t i = 1; i < SNAKE_LEN; i++) {
    snake[SNAKE_LEN - i] = ordered[i];  // neck..tail → indices 7..1
  }
}

void setup() {
  randomSeed(analogRead(A0));

  lcd.begin(COLS, ROWS);
  lcd.setBacklight(HIGH);

  lcd.createChar(CHAR_HEAD_N, glyphHeadN);
  lcd.createChar(CHAR_HEAD_E, glyphHeadE);
  lcd.createChar(CHAR_HEAD_S, glyphHeadS);
  lcd.createChar(CHAR_HEAD_W, glyphHeadW);
  lcd.createChar(CHAR_BODY_V, glyphBodyV);
  lcd.createChar(CHAR_BODY_H, glyphBodyH);
  lcd.createChar(CHAR_FOOD, glyphFood);

  lcd.clear();
  clearFrame(prevFrame);

  initSnake();
  spawnFood();

  draw(true);
  lastStepMs = millis();
}

void loop() {
  unsigned long now = millis();
  if ((now - lastStepMs) < STEP_MS) {
    return;
  }
  lastStepMs = now;

  stepGame();
  draw(false);
}
