/*
 * This file is part of Model-F 5170 Converter.
 *
 * Copyright 2023 Paul Bramhall (paulwamp@gmail.com)
 *
 * Portions of this specific file are licensed under the MIT License.
 * The original source can be found at: https://github.com/Zheoni/pico-simon
 *
 * Model-F 5170 Converter is free software: you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * Model-F 5170 Converter is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Model-F 5170 Converter.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "buzzer.h"

#include <stdlib.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/time.h"

#define TOP_MAX 65534

note READY_SEQUENCE[] = {
    /* Sound when Initialised */
    {400, 20},
    {500, 20},
    {600, 20},
    {700, 20},
    {800, 20},
    {900, 20},
    {1000, 20},
    BUZZER_END_SEQUENCE};

note LOCK_LED[] = {
    /* Beep for Lock Light State Change */
    {0, 50},
    {1600, 150},
    BUZZER_END_SEQUENCE};

static uint curr_playing_id = 0;
static uint pwm_gpio = 0;

struct non_blocking_seq {
  uint callid;
  uint current;
  note *notes;
};

static volatile uint running_non_blocking_sequences = 0;

/**
 * Calculate the top and div values for a sound depending
 * on the sysclock.
 *
 * Got help from the mycropython function machine_pwm_freq
 */
sound buzzer_calc_sound(uint freq) {
  uint32_t source_hz = clock_get_hz(clk_sys);

  // div is a 12 bit decimal fixed point number 8(integer).4(fractional)

  // div is multiplied by 16 because to "forget" the fractional part and work
  // with a regular integer, keeping in mind that our real integer part is
  // div / 16.
  uint32_t div16_top = 16 * source_hz / freq;
  uint32_t top = 1;
  while (true) {
    // try small prime factors to get close to desired frequency

    // the 16 are to keep the integer part of at least 1
    if (div16_top >= 16 * 5 && div16_top % 5 == 0 && top * 5 <= TOP_MAX) {
      div16_top /= 5;
      top *= 5;
    } else if (div16_top >= 16 * 3 && div16_top % 3 == 0 &&
               top * 3 <= TOP_MAX) {
      div16_top /= 3;
      top *= 3;
    } else if (div16_top >= 16 * 2 && top * 2 <= TOP_MAX) {
      div16_top /= 2;
      top *= 2;
    } else {
      break;
    }
  }

  // if integer part is less then 1 or greater than 256
  if (div16_top < 16 || div16_top >= 256 * 16) {
    return 0;
  }

  sound s = (sound)(((uint16_t)div16_top) << 16 | ((uint16_t)top));
  return s;
}

inline static bool not_end(note n) { return n.s != 0 || n.d != 0; }

void buzzer_calc_sound_sequence(note *notes_src, note *notes_dest) {
  uint current = 0;
  while (not_end(notes_src[current])) {
    notes_dest[current].d = notes_src[current].d;
    notes_dest[current].s = buzzer_calc_sound(notes_src[current].s);
    ++current;
  }
  notes_dest[current].d = 0;
  notes_dest[current].s = 0;
}

void buzzer_init(uint buzzer_gpio) {
  pwm_gpio = buzzer_gpio;

  // Tell the BUZZER pin that the PWM is in charge
  gpio_set_function(pwm_gpio, GPIO_FUNC_PWM);
  // Get which slice the pin is connected to
  uint slice_num = pwm_gpio_to_slice_num(pwm_gpio);
  // Initialize the slice
  pwm_config config = pwm_get_default_config();
  pwm_init(slice_num, &config, false);

  buzzer_calc_sound_sequence(READY_SEQUENCE, READY_SEQUENCE);
  buzzer_calc_sound_sequence(LOCK_LED, LOCK_LED);
}

/**
 * The sound is a 32 bit integer.
 * The first half represent the div as a 12 bit integer
 * with a padding of 4 0s before the value.
 * The second half represent the top as a 16 bit integer.
 *
 * This 2 values make up the frequency. The frecuency is
 * calculated with (sysclock)/(div * top).
 * But div is special. The first 8 bits is the integer
 * value and the other 4 bits are the fractional part.
 *
 * Then div is a decimal number: 8bits.4bits
 */
void buzzer_play_sound(sound s) {
  if (s == 0) return;
  uint16_t div, top;
  div = (s & 0x0FFF0000) >> 16;
  top = s & 0x0000FFFF;

  uint slice_num = pwm_gpio_to_slice_num(pwm_gpio);
  pwm_hw->slice[slice_num].div = div;
  pwm_hw->slice[slice_num].top = top;
  pwm_set_gpio_level(pwm_gpio, top / 2);
  pwm_set_enabled(slice_num, 1);
}

void buzzer_stop_sound(void) {
  uint slice_num = pwm_gpio_to_slice_num(pwm_gpio);
  pwm_set_enabled(slice_num, 0);
}

static int64_t _buzzer_non_blocking_callback(alarm_id_t id, void *user_data) {
  // Unused parameter 'id': Callback does not require using the 'id' parameter
  (void)id;

  struct non_blocking_seq *call = (struct non_blocking_seq *)user_data;

  if (call->callid == curr_playing_id) {
    buzzer_stop_sound();
    call->current += 1;

    if (not_end(call->notes[call->current])) {
      buzzer_play_sound(call->notes[call->current].s);
      return -((int64_t)call->notes[call->current].d) * 1000;
    } else {
      free(call);
      uint32_t state = save_and_disable_interrupts();
      --running_non_blocking_sequences;
      restore_interrupts(state);
      return 0;
    }
  } else {
    free(call);
    uint32_t state = save_and_disable_interrupts();
    --running_non_blocking_sequences;
    restore_interrupts(state);
    return 0;
  }
}

bool buzzer_play_sound_sequence_non_blocking(note *notes) {
  // if ( is_playing == 0 ) {
  // is_playing = 1;
  if (not_end(notes[0])) {
    curr_playing_id = rand();
    struct non_blocking_seq *call =
        (struct non_blocking_seq *)malloc(sizeof(struct non_blocking_seq));
    call->callid = curr_playing_id;
    call->current = 0;
    call->notes = notes;
    if (call == NULL) return false;

    uint32_t state = save_and_disable_interrupts();
    ++running_non_blocking_sequences;
    restore_interrupts(state);

    buzzer_stop_sound();
    buzzer_play_sound(notes[0].s);
    if (add_alarm_in_ms(notes[0].d, &_buzzer_non_blocking_callback, call,
                        true) == -1) {
      free(call);
      state = save_and_disable_interrupts();
      --running_non_blocking_sequences;
      restore_interrupts(state);
      return false;
    }
  }
  // }
  return true;
}