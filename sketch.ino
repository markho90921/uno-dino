
#include <Arduino_LED_Matrix.h>
#include <Arduino_RouterBridge.h>

Arduino_LED_Matrix matrix;

const int WIDTH = 13, HEIGHT = 8;
const int PLAYER_X = 2, GROUND_Y = 6;

// Difficulty settings
const int INITIAL_INTERVAL = 300, MIN_INTERVAL = 140;
const int SPEED_STEP = 20, SCORE_PER_LEVEL = 3;

// Jump settings
const int JUMP_STEP_TIME = 150;
const int jumpHeights[] = {1,2,3,3,3,2,1,0};
const int JUMP_STEPS = sizeof(jumpHeights)/sizeof(jumpHeights[0]);

// Screen settings
const int GAME_OVER_TIME = 1500, SCORE_PAGE_TIME = 2200;

// Power-up settings
const int POWERUP_Y = 3, POWERUP_CHANCE = 33;
const int POWERUP_BLINK_TIME = 120, POWERUP_OFFSET = 5;

// Boost settings
const int BOOST_Y = 1, BOOST_DURATION = 6500;
const int BOOST_MULTIPLIER = 3, BOOST_WARNING = 1000;

// Landing settings
const int LANDING_STEP_TIME = 80, SAFE_LANDING_GAP = 7;

// Game states
enum GameState {
  TITLE,
  PLAYING,
  GAME_OVER,
  SHOW_SCORE
};

enum MovementState {
  NORMAL_MOVEMENT,
  BOOSTING,
  WAITING_TO_LAND,
  LANDING
};

enum ObstacleType {
  NORMAL,
  TALL,
  FLYING,
  WIDE
};

enum SoundEvent {
  SILENT,
  START_SOUND,
  JUMP_SOUND,
  BOOST_SOUND,
  DEATH_SOUND,
  LAND_SOUND
};

GameState gameState = TITLE;
MovementState movementState = NORMAL_MOVEMENT;
ObstacleType obstacleType = NORMAL;

// Game variables
int playerY = GROUND_Y;
int obstacleX = WIDTH-1;
int powerupX = -1;

int score = 0;
int bestScore = 0;
int obstacleInterval = INITIAL_INTERVAL;
int groundPhase = 0;

bool jumping = false;
bool powerupActive = false;
bool firstPowerupPending = true;

// Timers
unsigned long jumpStart = 0;
unsigned long lastObstacleMove = 0;
unsigned long gameOverStart = 0;
unsigned long powerupSpawnTime = 0;
unsigned long boostStart = 0;
unsigned long landingStart = 0;
unsigned long scoreStart = 0;

// Sound variables
int soundSequence = 0;
int lastSound = SILENT;

// ==============================
// Bitmap fonts
// ==============================

const uint8_t DIGITS[10][5][3] = {
  {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
  {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}},
  {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},
  {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},
  {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},
  {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
  {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},
  {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
  {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},
  {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}
};

const uint8_t RUN_TEXT[3][5][3] = {
  {{1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,0,1}},
  {{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},
  {{1,0,1},{1,1,1},{1,1,1},{1,1,1},{1,0,1}}
};

// ==============================
// Sound events and high score
// ==============================

void playEvent(int event) {
  soundSequence = (soundSequence + 1) % 30000;
  lastSound = event;
}

// Compact integer RPC response:
// sequence, sound ID, best score
int getStatus() {
  return soundSequence*10000
       + lastSound*1000
       + bestScore;
}

int setBest(int value) {
  if (value >= 0 && value <= 999 && value > bestScore) {
    bestScore = value;
  }

  return bestScore;
}

// ==============================
// Reset game
// ==============================

void resetGame() {
  playerY = GROUND_Y;
  obstacleX = WIDTH-1;
  obstacleType = NORMAL;

  score = 0;
  obstacleInterval = INITIAL_INTERVAL;
  groundPhase = 0;

  jumping = false;
  gameState = PLAYING;
  movementState = NORMAL_MOVEMENT;

  powerupActive = false;
  firstPowerupPending = true;
  powerupX = -1;

  jumpStart = 0;
  gameOverStart = 0;
  boostStart = 0;
  landingStart = 0;
  scoreStart = 0;

  lastObstacleMove = millis();

  playEvent(START_SOUND);
}

void showScore(unsigned long now) {
  gameState = SHOW_SCORE;
  scoreStart = now;
}

// ==============================
// Keyboard input
// ==============================

bool jump() {
  static bool seeded = false;

  if (!seeded) {
    randomSeed(micros());
    seeded = true;
  }

  if (gameState == TITLE || gameState == SHOW_SCORE) {
    resetGame();
    return true;
  }

  if (gameState == GAME_OVER) {
    showScore(millis());
    return true;
  }

  if (movementState != NORMAL_MOVEMENT || jumping) {
    return false;
  }

  jumpStart = millis();
  jumping = true;

  playEvent(JUMP_SOUND);

  return true;
}

// ==============================
// Score and difficulty
// ==============================

void addScore() {
  if (score < 999) {
    score++;
  }

  if (score > bestScore) {
    bestScore = score;
  }

  obstacleInterval =
      INITIAL_INTERVAL -
      (score/SCORE_PER_LEVEL)*SPEED_STEP;

  if (obstacleInterval < MIN_INTERVAL) {
    obstacleInterval = MIN_INTERVAL;
  }
}

int getMoveInterval() {
  return movementState == BOOSTING
      ? obstacleInterval/BOOST_MULTIPLIER
      : obstacleInterval;
}

// ==============================
// Obstacle settings
// ==============================

int obstacleWidth() {
  return obstacleType == WIDE ? 2 : 1;
}

int obstacleRight() {
  return obstacleX + obstacleWidth()-1;
}

// ==============================
// Random obstacle generation
// ==============================

void spawnObstacle(unsigned long now) {
  obstacleX = WIDTH-1 + random(0,7);

  int roll = random(0,100);

  obstacleType =
      roll < 30 ? NORMAL :
      roll < 55 ? TALL :
      roll < 80 ? FLYING :
      WIDE;

  powerupActive = false;

  if (movementState != NORMAL_MOVEMENT) {
    return;
  }

  if (firstPowerupPending ||
      random(0,100) < POWERUP_CHANCE) {

    powerupActive = true;
    firstPowerupPending = false;

    powerupX = obstacleX;
    obstacleX = powerupX + POWERUP_OFFSET;

    powerupSpawnTime = now;
  }
}

bool isObstacleRow(int y) {
  switch (obstacleType) {

    case NORMAL:
    case WIDE:
      return y == GROUND_Y;

    case TALL:
      return y == GROUND_Y ||
             y == GROUND_Y-1;

    case FLYING:
      return y == GROUND_Y-2 ||
             y == GROUND_Y-3;
  }

  return false;
}

// ==============================
// Collision detection
// ==============================

bool checkCollision() {
  return obstacleX <= PLAYER_X &&
         obstacleRight() >= PLAYER_X &&
         isObstacleRow(playerY);
}

// ==============================
// Boost power-up
// ==============================

void activateBoost(unsigned long now) {
  powerupActive = false;
  jumping = false;

  movementState = BOOSTING;

  playerY = BOOST_Y;
  boostStart = now;
  lastObstacleMove = now;

  playEvent(BOOST_SOUND);
}

void checkPowerup(unsigned long now) {
  if (powerupActive &&
      movementState == NORMAL_MOVEMENT &&
      powerupX == PLAYER_X &&
      playerY == POWERUP_Y) {

    activateBoost(now);
  }
}

// ==============================
// Safe landing system
// ==============================

bool canLandSafely() {
  // Both cells of a wide obstacle must pass.
  return obstacleRight() < PLAYER_X ||
         obstacleX - PLAYER_X >= SAFE_LANDING_GAP;
}

void updateBoostTimer(unsigned long now) {
  if (movementState == BOOSTING &&
      now-boostStart >= BOOST_DURATION) {

    movementState = WAITING_TO_LAND;

    lastObstacleMove = now;
  }
}

void updateLanding(unsigned long now) {

  if (movementState == WAITING_TO_LAND) {

    playerY = BOOST_Y;

    if (canLandSafely()) {
      movementState = LANDING;
      landingStart = now;
    }
  }

  if (movementState == LANDING) {

    // Abort descent if an obstacle reaches
    // the player's column unexpectedly.

    if (obstacleX <= PLAYER_X &&
        obstacleRight() >= PLAYER_X) {

      movementState = WAITING_TO_LAND;
      playerY = BOOST_Y;
      return;
    }

    int step =
        (now-landingStart)/LANDING_STEP_TIME;

    playerY = BOOST_Y + step;

    if (playerY >= GROUND_Y) {
      playerY = GROUND_Y;
      movementState = NORMAL_MOVEMENT;
      jumping = false;

      playEvent(LAND_SOUND);
    }
  }
}

// ==============================
// Normal jump
// ==============================

void updateJump(unsigned long now) {
  if (movementState != NORMAL_MOVEMENT ||
      !jumping) {
    return;
  }

  int step =
      (now-jumpStart)/JUMP_STEP_TIME;

  if (step >= JUMP_STEPS) {
    jumping = false;
    playerY = GROUND_Y;
  } else {
    playerY = GROUND_Y - jumpHeights[step];
  }
}

// ==============================
// World scrolling
// ==============================

void moveWorld(unsigned long now) {
  if (now-lastObstacleMove <
      (unsigned long)getMoveInterval()) {
    return;
  }

  lastObstacleMove = now;

  groundPhase = (groundPhase+1)%4;

  int previousRight = obstacleRight();

  obstacleX--;

  if (powerupActive && --powerupX < 0) {
    powerupActive = false;
  }

  // Score only after the entire obstacle passes.
  if (previousRight == PLAYER_X &&
      obstacleRight() < PLAYER_X) {

    addScore();
  }

  // Spawn next obstacle after leaving the screen.
  if (obstacleRight() < 0) {
    spawnObstacle(now);
  }
}

// ==============================
// Number rendering
// ==============================

void drawDigit(
    uint8_t frame[HEIGHT][WIDTH],
    int digit,
    int startX,
    int startY
) {

  for (int y=0; y<5; y++) {
    for (int x=0; x<3; x++) {
      frame[startY+y][startX+x] =
          DIGITS[digit][y][x];
    }
  }
}

void drawNumber(
    uint8_t frame[HEIGHT][WIDTH],
    int value,
    int y
) {
  drawDigit(frame, value/100, 1, y);
  drawDigit(frame, (value/10)%10, 5, y);
  drawDigit(frame, value%10, 9, y);
}

// ==============================
// Title screen
// ==============================

void drawTitle(unsigned long now) {
  uint8_t frame[HEIGHT][WIDTH] = {};

  for (int n=0; n<3; n++) {
    for (int y=0; y<5; y++) {
      for (int x=0; x<3; x++) {
        frame[y+1][1+4*n+x] =
            RUN_TEXT[n][y][x];
      }
    }
  }

  // Blinking start indicator
  if ((now/350)%2 == 0) {
    frame[7][5] = 1;
    frame[7][6] = 1;
    frame[7][7] = 1;
  }

  matrix.renderBitmap(frame, HEIGHT, WIDTH);
}

// ==============================
// Score screen
// ==============================

void drawScore(unsigned long now) {
  uint8_t frame[HEIGHT][WIDTH] = {};

  bool showBest =
      ((now-scoreStart)/SCORE_PAGE_TIME)%2 != 0;

  if (showBest) {

    // Crown marks the high score.
    frame[0][4] = 1;
    frame[0][6] = 1;
    frame[0][8] = 1;

    for (int x=4; x<=8; x++) {
      frame[1][x] = 1;
    }

    drawNumber(frame, bestScore, 3);

  } else {

    drawNumber(frame, score, 1);

  }

  matrix.renderBitmap(frame, HEIGHT, WIDTH);
}

// ==============================
// Game Over screen
// ==============================

void drawGameOver() {
  uint8_t frame[HEIGHT][WIDTH] = {};

  for (int y=0; y<7; y++) {
    int d = y<3 ? 3-y : y-3;

    frame[y][6-d] = 1;
    frame[y][6+d] = 1;
  }

  matrix.renderBitmap(frame, HEIGHT, WIDTH);
}

// ==============================
// Gameplay rendering
// ==============================

void drawGame(unsigned long now) {
  uint8_t frame[HEIGHT][WIDTH] = {};

  // Ground
  for (int x=0; x<WIDTH; x++) {
    frame[7][x] = 1;
  }

  // Draw obstacles
  for (int x=obstacleX; x<=obstacleRight(); x++) {

    if (x>=0 && x<WIDTH) {

      for (int y=0; y<HEIGHT; y++) {
        if (isObstacleRow(y)) {
          frame[y][x] = 1;
        }
      }
    }
  }

  // Blinking power-up
  if (powerupActive &&
      powerupX>=0 &&
      powerupX<WIDTH &&
      ((now-powerupSpawnTime)/POWERUP_BLINK_TIME)%2==0) {

    frame[POWERUP_Y][powerupX] = 1;
  }

  // Blink the player during the last second of Boost
  bool warning =
      movementState == BOOSTING &&
      now-boostStart >= BOOST_DURATION-BOOST_WARNING;

  if (!warning || ((now-boostStart)/100)%2==0) {
    frame[playerY][PLAYER_X] = 1;
  }

  matrix.renderBitmap(frame, HEIGHT, WIDTH);
}

// ==============================
// Setup
// ==============================

void setup() {
  matrix.begin();
  matrix.clear();

  Bridge.begin();

  Bridge.provide_safe("jump", jump);
  Bridge.provide_safe("get_status", getStatus);
  Bridge.provide_safe("set_best", setBest);
}

// ==============================
// Main game loop
// ==============================

void loop() {
  unsigned long now = millis();

  // Title screen
  if (gameState == TITLE) {
    drawTitle(now);
    delay(10);
    return;
  }

  // Game Over screen
  if (gameState == GAME_OVER) {

    if (now-gameOverStart >= GAME_OVER_TIME) {
      showScore(now);
      drawScore(now);
    } else {
      drawGameOver();
    }

    delay(10);
    return;
  }

  // Score screen
  if (gameState == SHOW_SCORE) {
    drawScore(now);
    delay(10);
    return;
  }

  updateBoostTimer(now);
  updateJump(now);

  // Check pickup before scrolling.
  checkPowerup(now);

  // Check collision before obstacle leaves the column.
  if (checkCollision()) {

    gameState = GAME_OVER;
    gameOverStart = now;

    playEvent(DEATH_SOUND);

    drawGameOver();
    delay(10);
    return;
  }

  moveWorld(now);
  checkPowerup(now);
  updateLanding(now);

  // Check collision after scrolling.
  if (checkCollision()) {

    gameState = GAME_OVER;
    gameOverStart = now;

    playEvent(DEATH_SOUND);

    drawGameOver();
    delay(10);
    return;
  }

  drawGame(now);

  delay(10);
}
