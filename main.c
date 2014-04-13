/* USB Keyboard Firmware code for generic Teensy Keyboards
 * Copyright (c) 2012 Fredrik Atmer, Bathroom Epiphanies Inc
 * http://www.bathroomepiphanies.com
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <avr/pgmspace.h>
#include "lib/usb_keyboard_debug.h"
#include "lib/print.h"
#include "hw_interface.h"
#include KEYBOARD_MODEL

// Attention key to enter "magic mode".
#define MAGIC_KEY  KC_RGUI

struct {uint8_t type; uint8_t value;} layout[] = KEYBOARD_LAYOUT;
struct {uint8_t pressed; uint8_t bounce;} key[NKEY];

uint8_t queue[7] = {0,0,0,0,0,0,0};
uint8_t mod_keys = 0;
uint8_t magic_mode = 0;

void init(void);
void send(void);
void key_press(uint8_t k);
void key_release(uint8_t k);
void debug_print(void);

ISR(SCAN_INTERRUPT_FUNCTION) {
  poll_timer_disable();
  for(uint8_t r = 0, k = 0; r < NROW; r++) {
    pull_row(r);
    for(uint8_t c = 0; c < NCOL; c++, k++) {
      key[k].bounce |= probe_column(c);
      if(key[k].bounce == 0b01111111 && !key[k].pressed)
        key_press(k);
      if(key[k].bounce == 0b10000000 &&  key[k].pressed)
        key_release(k);
      
      key[k].bounce <<= 1;
    }
  }
  release_rows();
  // if(mod_keys == (uint8_t)(KC_LSFT | KC_RSFT))
  //   jump_bootloader();

  // lights on in magic mode
  if (magic_mode) {
    update_leds(6);
  } else {
    update_leds(keyboard_leds);
  }
#ifdef DEBUG
  debug_print();
#endif  
  poll_timer_enable();
}

int main(void) {
  init();
  poll_timer_enable();
  for(ever);
}

void send(void) {
  uint8_t i;
  for(i = 0; i < 6; i++)
    keyboard_keys[i] = queue[i];
  keyboard_modifier_keys = mod_keys;
  usb_keyboard_send();
}

// Low-level key press.
void ll_key_press(uint8_t k) {
  uint8_t i;
  for(i = 5; i > 0; i--) 
    queue[i] = queue[i-1];
  queue[0] = k;
  send();
}

void ll_modifier_press(uint8_t mod) {
  mod_keys |= mod;
  send();
}

// Low-level key release.
void ll_key_release(uint8_t k) {
  uint8_t i;
  for(i = 0; i < 6; i++) 
    if(queue[i]==k)
      break;
  for(; i < 6; i++)
    queue[i] = queue[i+1];
  send();
}

void ll_modifier_release(uint8_t mod) {
  mod_keys &= ~mod;
  send();
}

uint8_t is_magic_key(uint8_t k) {
  struct {uint8_t type; uint8_t value;} magic = MAGIC_KEY;
  return layout[k].type == magic.type
      && layout[k].value == magic.value;
}

// Hook function invoked for key presses when we are in
// magic mode.
void magic_key_press(uint8_t k) {
  if (is_magic_key(k)) {
    magic_mode = 0;
  }
  if (layout[k].value == KEY_X) {
    ll_key_press(KEY_X);
    ll_key_release(KEY_X);
    ll_key_press(KEY_X);
    ll_key_release(KEY_X);
  }
}

void key_press(uint8_t k) {
  key[k].pressed = true;
  if (magic_mode) {
    magic_key_press(k);
  } else if (is_magic_key(k)) {
    magic_mode = 1;
  } else if(IS_MODIFIER(layout[k])) {
    ll_modifier_press(layout[k].value);
  } else {
    ll_key_press(layout[k].value);
  }
}

void key_release(uint8_t k) {
  key[k].pressed = false;
  if (magic_mode) {
    // magic_key_release?
  } else if(IS_MODIFIER(layout[k])) {
    ll_modifier_release(layout[k].value);
  } else {
    ll_key_release(layout[k].value);
  }
}

void init(void) {
  usb_init();
  while(!usb_configured());
  keyboard_init();
  mod_keys = 0;
  for(uint8_t k = 0; k < NKEY; k++)
    key[k].bounce = key[k].pressed = 0x00;
  sei();
}

uint8_t debug_counter = 0;
void debug_print(void) {
  debug_counter++;
  if(debug_counter > 100) {
    debug_counter = 0;
    for(uint8_t i = 0; i < 7; i++)
      phex(queue[i]);
    print("\n");
    for(uint8_t k = 0; k < NKEY; k++)
      phex(key[k].bounce);
    print("\n");
  }
}
