#include "core_pins.h"
#include <Arduino.h>
#include <cstddef>

#define MACRO_CTRL 0x0100
#define MACRO_SHIFT 0x0200

constexpr uint32_t DEBOUNCE_DELAY{15}; // ms

constexpr uint8_t R1{10};
constexpr uint8_t R2{9};
constexpr uint8_t R3{8};
constexpr uint8_t R4{7};

constexpr uint8_t C1{6};
constexpr uint8_t C2{23};
constexpr uint8_t C3{22};
constexpr uint8_t C4{21};
constexpr uint8_t C5{20};
constexpr uint8_t C6{19};
constexpr uint8_t C7{18};
constexpr uint8_t C8{17};
constexpr uint8_t C9{16};
constexpr uint8_t C10{13};
constexpr uint8_t C11{12};
constexpr uint8_t C12{11};

constexpr int ROW_LEN = 4;
const uint8_t ROWS[] = {R1, R2, R3, R4};

constexpr int COLUMN_LEN = 12;
const uint8_t COLUMNS[] = {C1, C2, C3, C4, C5, C6, C7, C8, C9, C10, C11, C12};

#define SET_MASK(var, val, mask) var = ((var) & (~(mask))) | ((val) & (mask))

typedef struct _letter_state {
  uint32_t letter_keys : 30;
  uint32_t letter_mod2 : 2;
} LetterState;

typedef struct _mods_state {
  uint16_t mod_tab : 1;
  uint16_t mod_ctrl : 1;
  uint16_t mod_shift : 1;
  uint16_t mod_esc : 1;
  uint16_t mod_win1_l : 1;
  uint16_t mod_win2_l : 1;
  uint16_t mod_alt_l : 1;
  uint16_t mod_space : 1;
  uint16_t mod_alt_r : 1;
  uint16_t mod_win_r : 1;
  uint16_t mod_compose : 1;
  uint16_t mod_enter : 1;
  uint16_t mod_question : 1;
  uint16_t mod_backslash : 1;
  uint16_t mod_del : 1;
} ModState;

typedef struct _keyboard_state {
  LetterState letters;
  ModState mods;
} KeyBoardState;

KeyBoardState current_state{};
KeyBoardState prev_state{};

// raw current reading to check debounce status
uint64_t current_reading{};
uint32_t lastDebounceTime[4 * 12];

const uint16_t LAYOUTS[4][30] = {
    // 0 layer
    {KEY_Q, KEY_W, KEY_E, KEY_R,         KEY_T,      KEY_Y,    KEY_U, KEY_I,
     KEY_O, KEY_P, KEY_A, KEY_S,         KEY_D,      KEY_F,    KEY_G, KEY_H,
     KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_Z,      KEY_X,    KEY_C, KEY_V,
     KEY_B, KEY_N, KEY_M, KEY_COMMA,     KEY_PERIOD, KEY_SLASH},
    // L layer
    {KEY_1,
     KEY_2,
     KEY_3,
     KEY_4,
     KEY_5,
     KEY_6,
     KEY_7,
     KEY_8,
     KEY_9,
     KEY_0,
     KEYPAD_7,
     KEYPAD_5,
     KEYPAD_3,
     KEYPAD_1,
     KEYPAD_9,
     KEYPAD_0,
     KEYPAD_2,
     KEYPAD_4,
     KEYPAD_6,
     KEYPAD_8,
     KEY_TILDE,
     KEY_X,
     KEY_I | MACRO_CTRL,
     KEY_PERIOD | MACRO_CTRL,
     KEY_B,
     KEY_N,
     KEY_M,
     KEY_TILDE,
     KEY_MINUS,
     KEY_EQUAL},
    // R layer
    {KEY_F1,
     KEY_F2,
     KEY_F3,
     KEY_F4,
     KEY_T,
     KEY_Y,
     KEY_U,
     KEY_I | MACRO_CTRL,
     KEY_PERIOD | MACRO_CTRL,
     KEY_P,
     KEY_F5,
     KEY_F6,
     KEY_F7,
     KEY_F8,
     KEY_G,
     KEY_H,
     KEY_LEFT,
     KEY_DOWN,
     KEY_UP,
     KEY_RIGHT,
     KEY_F9,
     KEY_F10,
     KEY_F11,
     KEY_F12,
     KEY_B,
     KEY_N,
     KEY_M,
     KEY_COMMA,
     KEY_PERIOD,
     KEY_SLASH},
    // RL layer
    {KEY_Q,
     KEY_W,
     KEY_E,
     KEY_R,
     KEY_T,
     KEY_Y,
     KEY_U,
     KEY_I,
     KEY_O,
     KEY_P,
     KEY_A,
     KEY_MEDIA_PREV_TRACK,
     KEY_MEDIA_NEXT_TRACK,
     KEY_MEDIA_PLAY_PAUSE,
     KEY_G,
     KEY_H,
     KEY_MEDIA_VOLUME_DEC,
     KEY_MEDIA_VOLUME_INC,
     KEY_MEDIA_MUTE,
     KEY_SEMICOLON,
     KEY_Z,
     KEY_X,
     KEY_C,
     KEY_V,
     KEY_B,
     KEY_N,
     KEY_M,
     KEY_COMMA,
     KEY_PERIOD,
     KEY_SLASH},
};

void initPins() {
  for (const uint8_t *row = ROWS; (row - ROWS) < ROW_LEN; row++) {
    pinMode(*row, OUTPUT);
    digitalWrite(*row, HIGH);
  }
  for (const uint8_t *column = COLUMNS; (column - COLUMNS) < COLUMN_LEN;
       column++) {
    pinMode(*column, INPUT_PULLUP);
  }
}

void readState(int row_index, uint32_t column) {
  // normal keys
  if (row_index < 3) {
    uint32_t column_mask = 0b011111111110;
    uint32_t letters = (column & column_mask) >> 1;
    letters <<= row_index * 10;
    uint32_t letter_mask = 0b1111111111 << (row_index * 10);
    SET_MASK(current_state.letters.letter_keys, letters, letter_mask);
  }

  switch (row_index) {
  case 0:;
    current_state.mods.mod_tab = column & 1;
    current_state.mods.mod_del = (column & (1 << 11)) != 0;
    Serial.printf("DEL: %d\n", current_state.mods.mod_del);
    break;
  case 1:
    current_state.mods.mod_ctrl = column & 1;
    current_state.mods.mod_backslash = (column & (1 << 11)) != 0;
    break;
  case 2:
    current_state.mods.mod_shift = column & 1;
    current_state.mods.mod_question = (column & (1 << 11)) != 0;
    break;
  case 3:
    current_state.mods.mod_esc = (column & 1) != 0;
    current_state.mods.mod_win1_l = (column & (1 << 1)) != 0;
    current_state.mods.mod_win2_l = (column & (1 << 2)) != 0;
    current_state.mods.mod_alt_l = (column & (1 << 3)) != 0;
    current_state.letters.letter_mod2 = (column & (0b1001 << 4)); // 4 en 7
    SET_MASK(current_state.letters.letter_mod2,
             ((column & (0b1000 << 4)) != 0) << 1, 0b10);
    SET_MASK(current_state.letters.letter_mod2, (column & (0b0001 << 4)) != 0,
             0b01);
    current_state.mods.mod_space = (column & (1 << 6)) != 0;
    current_state.mods.mod_alt_r = (column & (1 << 8)) != 0;
    current_state.mods.mod_win_r = (column & (1 << 9)) != 0;
    current_state.mods.mod_compose = (column & (1 << 10)) != 0;
    current_state.mods.mod_enter = (column & (1 << 11)) != 0;
    break;
  default:
    break;
  }
}

void readRow(int row_index) {
  const uint32_t currentTime = millis();

  digitalWrite(ROWS[row_index], LOW);
  uint32_t column{0};

  for (uint8_t column_index = 0; column_index < COLUMN_LEN; column_index++) {
    const uint32_t array_index = row_index * 12 + column_index;

    uint8_t readingKeyPress = digitalRead(COLUMNS[column_index]) == LOW;
    uint8_t lastKeyState =
        (uint8_t)((current_reading & ((uint64_t)1 << (uint64_t)array_index)) !=
                  0);
    uint32_t *lastDebounce = &lastDebounceTime[array_index];

    // check if new state and reset debounce
    if (readingKeyPress != lastKeyState)
      *lastDebounce = currentTime;

    // debounce ok -> write to column
    if ((currentTime - *lastDebounce) > DEBOUNCE_DELAY)
      column |= (uint32_t)readingKeyPress << (uint32_t)column_index;

    // remember new state
    SET_MASK(current_reading,
             (uint64_t)((uint64_t)readingKeyPress << (uint64_t)array_index),
             (uint64_t)((uint64_t)1 << (uint64_t)array_index));

    Serial.printf("%d (%d: %x, %d),", readingKeyPress, array_index,
                  lastKeyState, currentTime - *lastDebounce);
  }
  Serial.println();

  digitalWrite(ROWS[row_index], HIGH);

  readState(row_index, column);
}

inline void write_current_mod(uint32_t current, uint32_t prev,
                              uint32_t keycode) {
  if (current ^ prev) {
    if (current)
      Keyboard.press(keycode);
    else
      Keyboard.release(keycode);
  }
}

void write_current_state(void) {
  uint32_t key_state_diff =
      current_state.letters.letter_keys ^ prev_state.letters.letter_keys;

  // General keys
  for (int i = 0; i < 30; i++) {
    if ((key_state_diff >> i) & 0b1) {
      const uint8_t letter_mods = current_state.letters.letter_mod2;
      uint16_t key = LAYOUTS[letter_mods][i] & 0xF0FF;
      uint16_t is_macro = LAYOUTS[letter_mods][i] & 0x0F00;

      if (is_macro) {
        if (is_macro & MACRO_CTRL)
          Keyboard.press(MODIFIERKEY_CTRL);
        if (is_macro & MACRO_SHIFT)
          Keyboard.press(MODIFIERKEY_SHIFT);

        Keyboard.press(key);
        delay(10);
        Keyboard.release(key);

        if (is_macro & MACRO_SHIFT)
          Keyboard.release(MODIFIERKEY_SHIFT);
        if (is_macro & MACRO_CTRL)
          Keyboard.release(MODIFIERKEY_CTRL);
      } else {
        if ((current_state.letters.letter_keys >> i) & 1)
          Keyboard.press(key);
        else
          Keyboard.release(key);
      }
    }
  }

  // Modifiers
  write_current_mod(current_state.mods.mod_space, prev_state.mods.mod_space,
                    KEY_SPACE);
  write_current_mod(current_state.mods.mod_compose, prev_state.mods.mod_compose,
                    MODIFIERKEY_RIGHT_GUI);
  write_current_mod(current_state.mods.mod_win_r, prev_state.mods.mod_win_r,
                    MODIFIERKEY_RIGHT_GUI);
  write_current_mod(current_state.mods.mod_alt_r, prev_state.mods.mod_alt_r,
                    MODIFIERKEY_RIGHT_ALT);
  write_current_mod(current_state.mods.mod_alt_l, prev_state.mods.mod_alt_l,
                    MODIFIERKEY_LEFT_ALT);
  write_current_mod(current_state.mods.mod_win1_l, prev_state.mods.mod_win1_l,
                    MODIFIERKEY_GUI);
  write_current_mod(current_state.mods.mod_enter, prev_state.mods.mod_enter,
                    KEY_ENTER);
  write_current_mod(current_state.mods.mod_esc, prev_state.mods.mod_esc,
                    KEY_ESC);

  write_current_mod(current_state.mods.mod_shift, prev_state.mods.mod_shift,
                    MODIFIERKEY_LEFT_SHIFT);

  write_current_mod(current_state.mods.mod_ctrl, prev_state.mods.mod_ctrl,
                    MODIFIERKEY_LEFT_CTRL);
  write_current_mod(current_state.mods.mod_tab, prev_state.mods.mod_tab,
                    KEY_TAB);
  write_current_mod(current_state.mods.mod_del, prev_state.mods.mod_del,
                    KEY_BACKSPACE);
  write_current_mod(current_state.mods.mod_win2_l, prev_state.mods.mod_win2_l,
                    MODIFIERKEY_LEFT_GUI);

  // Change some modifiers when changing layers
  if (current_state.letters.letter_mod2)
    write_current_mod(current_state.mods.mod_question,
                      prev_state.mods.mod_question, KEY_QUOTE);
  else
    write_current_mod(current_state.mods.mod_question,
                      prev_state.mods.mod_question, KEY_LEFT_BRACE);
  if (current_state.letters.letter_mod2)
    write_current_mod(current_state.mods.mod_backslash,
                      prev_state.mods.mod_backslash, KEY_RIGHT_BRACE);
  else
    write_current_mod(current_state.mods.mod_backslash,
                      prev_state.mods.mod_backslash, KEY_BACKSLASH);
}

int main() {
  Serial.begin(115200);
  initPins();

  while (1) {
    // copy current to prev
    prev_state = current_state;

    for (uint8_t row = 0; row < ROW_LEN; row++)
      readRow(row);
    Serial.println("--------------------------");

    write_current_state();
  }
}
