#include "Arduino.h"
#include "dial_dasqua.h"

void dial_init(dial_dasqua_t* dev) {
  pinMode(dev->clk_pin, INPUT);
  pinMode(dev->data_pin, INPUT);
}

dial_output_t dial_get_value(dial_dasqua_t* dev) {
  // 80 ms exec

  // WAIT FOR IDLE
  int ticks = DIAL_IDLE_TICKS;
  while (ticks > 0) {
    int clk_val = digitalRead(dev->clk_pin);
    ticks = (clk_val == 0) ? DIAL_IDLE_TICKS : ticks - 1;
    delayMicroseconds(DIAL_SAMPLING_US);
  }

  // GET ALL 28 BITS
  uint32_t data_bits = 0;
  int quant_bits = 28;
  int clk_val_prev = 0;
  int clk_val_curr = 0;
  int timeout_ticks = 0;
  while (quant_bits > 0) {
    clk_val_prev = clk_val_curr;
    clk_val_curr = digitalRead(dev->clk_pin);

    if (clk_val_prev == 0 && clk_val_curr == 1) {
      uint32_t data_val = (uint32_t)digitalRead(dev->data_pin);
      data_val = data_val << (28 - 1);
      data_bits |= data_val;
      data_bits = (data_bits >> 1);
      quant_bits--;
    }
    
    timeout_ticks++;
    if (timeout_ticks >= DIAL_TIMEOUT_TICKS) {
      break;
    }

    delayMicroseconds(DIAL_SAMPLING_US);
  }

  // GROUP BITS
  int group_one = (data_bits >> 24) & 0b1111;
  int group_two = (data_bits >> 20) & 0b1111;
  int group_three = (data_bits >> 16) & 0b1111;
  int group_four = (data_bits >> 12) & 0b1111;
  int group_five = (data_bits >> 8) & 0b1111;
  int group_six = (data_bits >> 4) & 0b1111;

  // DECODE DATA
  dial_output_t output;

  if (group_one == 0) {
    output.sign = POSITIVE;
    output.unit = MM;
  } else if (group_one == 1) {
    output.sign = POSITIVE;
    output.unit = INCH;
  } else if (group_one == 2) {
    output.sign = NEGATIVE;
    output.unit = MM;
  } else if (group_one == 3) {
    output.sign = NEGATIVE;
    output.unit = INCH;
  } else {
    output.sign = POSITIVE;
    output.unit = MM;
  }

  int raw_val = group_two * 10000 + group_three * 1000 + group_four * 100 + group_five * 10 + group_six;
  float unsigned_val = (output.unit == MM) ? raw_val / 100.0 : raw_val / 10000.0;
  output.value = unsigned_val * ((float)output.sign);
  return output;
}