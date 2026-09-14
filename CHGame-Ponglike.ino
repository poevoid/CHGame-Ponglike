/*
 * HelloGraphics - the smallest useful CHGfx sketch.
 *
 * CHGfx keeps a 16-colour, 4 bpp framebuffer in SRAM. You draw into it,
 * then call display() once and the whole frame goes out over DMA in a
 * single burst. Nothing reaches the panel until you ask.
 *
 * Board: CHGame (CH32X035G8U6) + ST7735 128x128
 * Set Tools > Optimize > Faster (-O2).
 */
#include <CHGfx.h>
#include <Arduino.h>

extern "C" {
#include "ch32x035.h"
}
#define WIDTH 128
#define HEIGHT 128
constexpr uint8_t BUTTON_A = 1u << 0;
constexpr uint8_t BUTTON_B = 1u << 1;
constexpr uint8_t UP       = 1u << 2;
constexpr uint8_t DOWN     = 1u << 3;
constexpr uint8_t LEFT     = 1u << 4;
constexpr uint8_t RIGHT    = 1u << 5;
constexpr uint8_t START    = 1u << 6;
constexpr uint8_t SELECT   = 1u << 7;

/* Colours are PALETTE INDICES, 0..15 - not RGB565. Naming them keeps
 * the drawing code readable. */
enum : uint8_t {
  BLACK = 0,
  DARKGREY,
  GREY,
  LIGHTGREY,
  WHITE,
  RED,
  ORANGE,
  YELLOW,
  GREEN,
  DARKGREEN,
  CYAN,
  BLUE,
  NAVY,
  MAGENTA,
  PURPLE,
  PINK
};

static const uint16_t palette[16] = {
  0x0000, 0x18E3, 0x4208, 0xC618, 0xFFFF,
  0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x0400,
  0x07FF, 0x001F, 0x0010, 0xF81F, 0x8010, 0xFC9F
};

enum class Screen : uint8_t {
  Title,
  Game,
  Gameover,
  Win
};

struct Paddle {
  int x, y;
  int width, height;
  int score;
  uint8_t color;
};

struct Ball {
  int x, y;
  int size;
  uint8_t color;
  bool right;
  bool down;
  bool up;
};

uint8_t currentButtonState = 0;
uint8_t previousButtonState = 0;


Paddle player = { 0, 64, 8, 32, 0, GREEN };
Paddle oppo = { 120, 64, 8, 32, 0, RED };
Ball ball = { 64, 64, 8, MAGENTA, true, true, false };
Screen currentscreen = { Screen::Title };


uint8_t buttonsState() {
  uint8_t buttons;
  if (digitalRead(PIN_BTN_A) == LOW) {
    buttons |= BUTTON_A;
  }
  if (digitalRead(PIN_BTN_B) == LOW) {
    buttons |= BUTTON_B;
  }
  if (digitalRead(PIN_BTN_LEFT) == LOW) {
    buttons |= LEFT;
  }
  if (digitalRead(PIN_BTN_RIGHT) == LOW) {
    buttons |= RIGHT;
  }
  if (digitalRead(PIN_BTN_UP) == LOW) {
    buttons |= UP;
  }
  if (digitalRead(PIN_BTN_DOWN) == LOW) {
    buttons |= DOWN;
  }
  if (digitalRead(PIN_BTN_SELECT) == LOW) {
    buttons |= SELECT;
  }
  if (digitalRead(PIN_BTN_START) == LOW) {
    buttons |= START;
  }
  return buttons;
}
void buttonsBegin()
{
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_START, INPUT_PULLUP);
  pinMode(PIN_BTN_SELECT, INPUT_PULLUP);

  currentButtonState = buttonsState();
  previousButtonState = currentButtonState;
}
bool anyPressed(uint8_t buttons) {
  return (buttonsState() & buttons) != 0;
}
bool pressed(uint8_t buttons) {
  return (buttonsState() & buttons) == buttons;
}
bool notPressed(uint8_t buttons) {
  return (buttonsState() & buttons) == 0;
}

bool justPressed(uint8_t buttons) {
  return (!(previousButtonState & buttons) && (currentButtonState & buttons));
}
bool justReleased(uint8_t buttons) {
  return (!(previousButtonState & buttons) && !(currentButtonState & buttons));
}
void pollButtons() {
  previousButtonState = currentButtonState;
  currentButtonState = buttonsState();
}
void resetGame() {
  player.score = 0;
  oppo.score = 0;
  player.y = HEIGHT / 2 - player.height;
  ball.x = WIDTH / 2;
  ball.right = false;
}

void playerInput() {
  if (pressed(UP)) {
    player.y -= 3;
  }
  if (pressed(DOWN)) {
    player.y += 3;
  }
  //-----player bounds-----------|
  if (player.y > HEIGHT - player.height) {  //|
    player.y = HEIGHT - player.height;      //|
  }                                         //|
  if (player.y < 0) {                       //|
    player.y = 0;                           //|
  }                                         //|
                                            //---------------------------//|
}
void ballPhysics() {
  if (ball.right) {
    ball.x += 2;
  } else {
    ball.x -= 2;
  }
  if (ball.down) {
    ball.y += 2;
  }
  if (ball.up) {
    ball.y -= 2;
  }
  if (ball.y <= 0) {
    ball.down = true;
    ball.up = false;
    //buzzer.beep(20);
  }
  if (ball.y >= HEIGHT) {
    ball.down = false;
    ball.up = true;
    //buzzer.beep(20);
  }
}

void playerCollisions() {
  if (ball.x == player.x + player.width && player.y < ball.y + ball.size && player.y + player.height > ball.y) {
    if (player.y + player.height / 2 == ball.y) {  //if player hits dead center
      ball.down = false;
      ball.up = false;
    }
    if (player.y + player.height / 2 > ball.y) {
      ball.down = false;
      ball.up = true;
    }
    if (player.y + player.height / 2 < ball.y) {
      ball.up = false;
      ball.down = true;
    }
    //buzzer.beep(100);

    ball.right = true;
  }
}
void oppoCollisions() {
  if (ball.x == oppo.x && oppo.y < ball.y + ball.size && oppo.y + oppo.height > ball.y) {
    if (oppo.y + oppo.height / 2 == ball.y) {  //if oppo hits dead center
      ball.down = false;
      ball.up = false;
    }
    if (oppo.y + oppo.height / 2 > ball.y) {
      ball.down = false;
      ball.up = true;
    }
    if (oppo.y + oppo.height / 2 < ball.y) {
      ball.up = false;
      ball.down = true;
    }
    //buzzer.beep(100);

    ball.right = false;
  }
}

void scoring() {
  if (ball.x <= 0) {

    //strip.setBrightness(255);
    //strip.setPixelColor(0, red);
    //strip.show();
    oppo.score++;
    //buzzer.beep(500);
    ball.x = 64;
    delay(500);
    //strip.setBrightness(0);
    //strip.show();
  }
  if (ball.x >= WIDTH) {
    //strip.setBrightness(255);
    //strip.setPixelColor(0, green);
    //strip.show();
    player.score++;
    //buzzer.beep(500);
    ball.x = 64;
    delay(500);
    //strip.setBrightness(0);
    //strip.show();
  }
  if (oppo.score == 11) {
    currentscreen = Screen::Gameover;
  }
  if (player.score == 11) {
    currentscreen = Screen::Win;
  }
}
void oppoAutomation() {
  if (ball.x > WIDTH / 2 - 16 || random(0, 20) == 1) {
    if (ball.y < oppo.y || random(0, 20) == 8 && oppo.y > 1) {
      oppo.y -= 4;
    }
    if (ball.y > oppo.y + oppo.height || random(0, 20) == 7 && oppo.y < HEIGHT - oppo.height) {
      oppo.y += 4;
    }
  }
}
void gameloop() {
  switch (currentscreen) {
    case Screen::Title:
      Gfx.print(8, 10, "CH Pong-like", PINK);
      resetGame();
      if (justPressed(BUTTON_A) || justPressed(START)) {
        currentscreen = Screen::Game;
      }
      break;

    case Screen::Game:
      Gfx.fillRect(player.x, player.y, player.width, player.height, player.color);
      Gfx.fillRect(oppo.x, oppo.y, oppo.width, oppo.height, oppo.color);
      Gfx.fillCircle(ball.x, ball.y, ball.size, ball.color);
      Gfx.drawLine(WIDTH / 2, 0, WIDTH/2, HEIGHT, WHITE);
      playerInput();
      ballPhysics();
      playerCollisions();
      oppoCollisions();
      scoring();
      oppoAutomation();
      Gfx.print(4, 4, player.score, WHITE);
      Gfx.print(118, 4, oppo.score, WHITE);
      break;

    case Screen::Gameover:
      Gfx.print(45, 2, "YOU LOSE, TRY \n AGAIN?", WHITE);
      resetGame();
      if (justPressed(BUTTON_B) || justPressed(START) || justPressed(BUTTON_A)) {
        currentscreen = Screen::Game;
      }
      break;

    case Screen::Win:
      Gfx.print(45, 2, "YOU WIN!!!", WHITE);
      resetGame();
      if (justPressed(BUTTON_B) || justPressed(START) || justPressed(BUTTON_A)) {
        currentscreen = Screen::Game;
      }

      break;
  }
}
void setup() {
  Gfx.begin();  // 24 MHz SPI, 16 bpp output
  Gfx.setPalette(palette, 16);
  buttonsBegin();
}

void loop() {
  /* A blinking cursor, redrawing only the 8x12 box it lives in.
     * Sending 96 bytes instead of 32768 is the whole idea behind
     * displayRect(): cost scales with area, not with cleverness. */
  // static bool on = false;
  // on = !on;
  // Gfx.fillRect(112, 10, 8, 12, on ? WHITE : NAVY);
  // Gfx.displayRect(112, 10, 8, 12);
  // delay(400);
  Gfx.clear(BLACK);
  pollButtons();
  gameloop();
  Gfx.display();  // one DMA burst, ~11 ms
  delay(10);
}
