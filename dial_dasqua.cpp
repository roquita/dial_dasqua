#include "esp32-hal-gpio.h"
#include "Arduino.h"
#include "dial_dasqua.h"

typedef struct {
  int nextPin_val[64];
  int prevPin_val[64];
  int rising_flags;
  bool next_is_data;
} dial_clk_sensing_t;

bool x1_rising_detected = false;
bool x2_rising_detected = false;
bool x3_rising_detected = false;

void IRAM_ATTR x1_isr() {
  x1_rising_detected = true;
}
void IRAM_ATTR x2_isr() {
  x2_rising_detected = true;
}
void IRAM_ATTR x3_isr() {
  x3_rising_detected = true;
}

dial_error_t dial_init(dial_dasqua_t* dev) {
  pinMode(dev->x1_pin, INPUT_PULLUP);
  pinMode(dev->x2_pin, INPUT_PULLUP);
  pinMode(dev->x3_pin, INPUT_PULLUP);

  pinMode(dev->req_pin, OUTPUT);
  digitalWrite(dev->req_pin, 1);

  return DIAL_OK;
}

dial_error_t dial_get_value(dial_dasqua_t* dev, dial_output_t* output) {

  // START CONTINUOUS REQUEST
  attachInterrupt(dev->x1_pin, x1_isr, RISING);
  attachInterrupt(dev->x2_pin, x2_isr, RISING);
  attachInterrupt(dev->x3_pin, x3_isr, RISING);
  digitalWrite(dev->req_pin, 0);

  // UNKNOWN DIGITAL PINS
  dial_clk_sensing_t* clk_sensing;

  // WAIT FOR IDLE
  int sys_idle_ticks = 0;
  int sys_timeout_idle_ticks = 0;
  bool sys_got_ok_idle = false;

  while (!sys_got_ok_idle) {

    // DETECT  OK IDLE
    bool rising_detected = x1_rising_detected || x2_rising_detected || x3_rising_detected;
    x1_rising_detected = false;
    x2_rising_detected = false;
    x3_rising_detected = false;

    if (rising_detected) {
      sys_idle_ticks = 0;
    } else {
      sys_idle_ticks++;
    }

    sys_got_ok_idle = sys_idle_ticks > DIAL_OK_IDLE_TICKS;

    // DETECT TIMEOUT IDLE
    if (!sys_got_ok_idle) {
      sys_timeout_idle_ticks++;
    }

    if (sys_timeout_idle_ticks > DIAL_TIMEOUT_IDLE_TICKS) {
      // STOPS CONTINUOUS REQUEST
      digitalWrite(dev->req_pin, 1);
      detachInterrupt(dev->x1_pin);
      detachInterrupt(dev->x2_pin);
      detachInterrupt(dev->x3_pin);

      return DIAL_TIMEOUT_IDLE;
    }

    delayMicroseconds(DIAL_SAMPLING_US);
  }

  // GET ALL BITS (50ms)
  dial_clk_sensing_t x1_sensing = { { 0 }, { 0 }, 0, false };
  dial_clk_sensing_t x2_sensing = { { 0 }, { 0 }, 0, false };
  dial_clk_sensing_t x3_sensing = { { 0 }, { 0 }, 0, false };
  int active_ticks = 0;

  while (active_ticks < DIAL_ACTIVE_TICKS) {

    if (x1_rising_detected) {
      x1_rising_detected = false;
      x1_sensing.prevPin_val[x1_sensing.rising_flags] = digitalRead(dev->x3_pin);
      x1_sensing.nextPin_val[x1_sensing.rising_flags] = digitalRead(dev->x2_pin);
      x1_sensing.rising_flags++;
    }
    if (x1_sensing.rising_flags >= DIAL_MAX_RISING_FLAGS) {
      break;
    }

    if (x2_rising_detected) {
      x2_rising_detected = false;
      x2_sensing.prevPin_val[x2_sensing.rising_flags] = digitalRead(dev->x1_pin);
      x2_sensing.nextPin_val[x2_sensing.rising_flags] = digitalRead(dev->x3_pin);
      x2_sensing.rising_flags++;
    }
    if (x2_sensing.rising_flags >= DIAL_MAX_RISING_FLAGS) {
      break;
    }

    if (x3_rising_detected) {
      x3_rising_detected = false;
      x3_sensing.prevPin_val[x3_sensing.rising_flags] = digitalRead(dev->x2_pin);
      x3_sensing.nextPin_val[x3_sensing.rising_flags] = digitalRead(dev->x1_pin);
      x3_sensing.rising_flags++;
    }
    if (x3_sensing.rising_flags >= DIAL_MAX_RISING_FLAGS) {
      break;
    }

    // DETECT MAX ACTIVE TIME
    active_ticks++;
    delayMicroseconds(DIAL_SAMPLING_US);
  }

  // STOPS CONTINUOUS REQUEST
  digitalWrite(dev->req_pin, 1);
  detachInterrupt(dev->x1_pin);
  detachInterrupt(dev->x2_pin);
  detachInterrupt(dev->x3_pin);

  // DETECT CLK AND DATA LINES
  // 28 and 52 are clk puses per reading on dials outpu
  bool x1_isnt_clk = (x1_sensing.rising_flags != 28 && x1_sensing.rising_flags != 52);
  bool x2_isnt_clk = (x2_sensing.rising_flags != 28 && x2_sensing.rising_flags != 52);
  bool x3_isnt_clk = (x3_sensing.rising_flags != 28 && x3_sensing.rising_flags != 52);
  bool x1_is_none = x1_sensing.rising_flags == 0;
  bool x2_is_none = x2_sensing.rising_flags == 0;
  bool x3_is_none = x3_sensing.rising_flags == 0;

  if (x1_is_none) {
    clk_sensing = x2_isnt_clk ? &x3_sensing : &x2_sensing;
    clk_sensing->next_is_data = x2_isnt_clk ? false : true;
  } else if (x2_is_none) {
    clk_sensing = x3_isnt_clk ? &x1_sensing : &x3_sensing;
    clk_sensing->next_is_data = x3_isnt_clk ? false : true;
  } else if (x3_is_none) {
    clk_sensing = x1_isnt_clk ? &x2_sensing : &x1_sensing;
    clk_sensing->next_is_data = x1_isnt_clk ? false : true;
  } else {
    return DIAL_UNRECOGNIZED_PINS;
  }

  //
  int* data = (clk_sensing->next_is_data) ? (clk_sensing->nextPin_val) : (clk_sensing->prevPin_val);
  // int* data = (clk_sensing->next_is_data) ? (clk_sensing->prevPin_val) : (clk_sensing->nextPin_val);

  if (clk_sensing->rising_flags == 28) {
    uint64_t data_bits = 0;
    for (int i = 0; i < 28; i++) {
      data_bits |= ((uint64_t)data[i] << i);
    }

    // GROUP BITS
    uint64_t group_one = (data_bits >> 24) & 0b1111;
    uint64_t group_two = (data_bits >> 20) & 0b1111;
    uint64_t group_three = (data_bits >> 16) & 0b1111;
    uint64_t group_four = (data_bits >> 12) & 0b1111;
    uint64_t group_five = (data_bits >> 8) & 0b1111;
    uint64_t group_six = (data_bits >> 4) & 0b1111;

    // DECODE DATA
    if (group_one == 0) {
      output->sign = POSITIVE;
      output->unit = MM;
    } else if (group_one == 1) {
      output->sign = POSITIVE;
      output->unit = INCH;
    } else if (group_one == 2) {
      output->sign = NEGATIVE;
      output->unit = MM;
    } else if (group_one == 3) {
      output->sign = NEGATIVE;
      output->unit = INCH;
    } else {
      output->sign = POSITIVE;
      output->unit = MM;
    }

    float raw_val = group_two * 10000.0 + group_three * 1000.0 + group_four * 100.0 + group_five * 10.0 + group_six * 1.0;
    float unsigned_val = (output->unit == MM) ? raw_val / 100.0 : raw_val / 10000.0;
    output->value = unsigned_val * ((float)output->sign);
    return DIAL_OK;

  } else if (clk_sensing->rising_flags == 52) {

    uint64_t data_bits = 0;
    for (int i = 0; i < 52; i++) {
      data_bits |= ((uint64_t)data[i] << i);
    }

    // GROUP BITS
    uint64_t group_one = (data_bits >> 48) & 0b1111;
    uint64_t group_three = (data_bits >> 40) & 0b1111;
    uint64_t group_four = (data_bits >> 36) & 0b1111;
    uint64_t group_five = (data_bits >> 32) & 0b1111;
    uint64_t group_six = (data_bits >> 28) & 0b1111;
    uint64_t group_seven = (data_bits >> 24) & 0b1111;
    uint64_t group_eight = (data_bits >> 20) & 0b1111;
    uint64_t group_nine = (data_bits >> 16) & 0b1111;

    // DECODE DATA
    if (group_one == 0 && group_nine == 8) {
      output->sign = NEGATIVE;
      output->unit = MM;
    } else if (group_one == 0 && group_nine == 0) {
      output->sign = POSITIVE;
      output->unit = MM;
    } else if (group_one == 1 && group_nine == 0) {
      output->sign = POSITIVE;
      output->unit = INCH;
    } else if (group_one == 1 && group_nine == 8) {
      output->sign = NEGATIVE;
      output->unit = INCH;
    } else {
      output->sign = POSITIVE;
      output->unit = MM;
    }

    float raw_val = group_eight * 100000.0 + group_seven * 10000.0 + group_six * 1000.0 + group_five * 100.0 + group_four * 10.0 + group_three * 1.0;
    float unsigned_val = (output->unit == MM) ? raw_val / 1000.0 : raw_val / 100000.0;
    output->value = unsigned_val * ((float)output->sign);
    return DIAL_OK;

  } else {
    return DIAL_INVALID_RISING_PULSES;
  }
}