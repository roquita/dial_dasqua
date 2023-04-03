#include "esp32-hal-gpio.h"
#include "Arduino.h"
#include "dial_dasqua.h"

typedef struct {
  uint64_t nextPin_val[DIAL_MAX_RISING_FLAGS];
  uint64_t prevPin_val[DIAL_MAX_RISING_FLAGS];
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

static dial_error_t dial_wait_for_idle() {

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
      return DIAL_TIMEOUT_IDLE;
    }

    delayMicroseconds(DIAL_SAMPLING_US);
  }

  return DIAL_OK;
}

static void dial_start_continuous_request(dial_dasqua_t* dev) {

  x1_rising_detected = false;
  x2_rising_detected = false;
  x3_rising_detected = false;
  attachInterrupt(dev->x1_pin, x1_isr, RISING);
  attachInterrupt(dev->x2_pin, x2_isr, RISING);
  attachInterrupt(dev->x3_pin, x3_isr, RISING);
  digitalWrite(dev->req_pin, 0);
}
static void dial_stop_continuous_request(dial_dasqua_t* dev) {
  digitalWrite(dev->req_pin, 1);
  detachInterrupt(dev->x1_pin);
  detachInterrupt(dev->x2_pin);
  detachInterrupt(dev->x3_pin);
}

static dial_error_t dial_wait_for_data_and_clk(dial_dasqua_t* dev, dial_clk_sensing_t* clk_sensing) {
  // SENSE ON EVERY RISING FLAG ON ALL LINES DURING 80MS
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
    if (x1_sensing.rising_flags > DIAL_MAX_RISING_FLAGS) {
      break;
    }

    if (x2_rising_detected) {
      x2_rising_detected = false;
      x2_sensing.prevPin_val[x2_sensing.rising_flags] = digitalRead(dev->x1_pin);
      x2_sensing.nextPin_val[x2_sensing.rising_flags] = digitalRead(dev->x3_pin);
      x2_sensing.rising_flags++;
    }
    if (x2_sensing.rising_flags > DIAL_MAX_RISING_FLAGS) {
      break;
    }

    if (x3_rising_detected) {
      x3_rising_detected = false;
      x3_sensing.prevPin_val[x3_sensing.rising_flags] = digitalRead(dev->x2_pin);
      x3_sensing.nextPin_val[x3_sensing.rising_flags] = digitalRead(dev->x1_pin);
      x3_sensing.rising_flags++;
    }
    if (x3_sensing.rising_flags > DIAL_MAX_RISING_FLAGS) {
      break;
    }

    // DETECT MAX ACTIVE TIME
    active_ticks++;
    delayMicroseconds(DIAL_SAMPLING_US);
  }

  // BASED EN PREVIOUS FLAGS SENSED, GET  WHICH ONE ARE DATA AND CLK LINES
  // CLK MUST HAVE 28(BIG DIAL) OR 52(LITTLE DIAL) FLAGS SAVED
  // DATA LINES HAS NO CRITERIA TO GET IT
  // THIRD LINE MUST HAVE 0 FLAHS SAVED
  bool x1_isnt_clk = (x1_sensing.rising_flags != 28 && x1_sensing.rising_flags != 52);
  bool x2_isnt_clk = (x2_sensing.rising_flags != 28 && x2_sensing.rising_flags != 52);
  bool x3_isnt_clk = (x3_sensing.rising_flags != 28 && x3_sensing.rising_flags != 52);
  bool x1_is_none = x1_sensing.rising_flags == 0;
  bool x2_is_none = x2_sensing.rising_flags == 0;
  bool x3_is_none = x3_sensing.rising_flags == 0;

  if (x1_is_none) {
    if (x2_isnt_clk) {
      memcpy(clk_sensing, &x3_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = false;
    } else {
      memcpy(clk_sensing, &x2_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = true;
    }
  } else if (x2_is_none) {
    if (x3_isnt_clk) {
      memcpy(clk_sensing, &x1_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = false;
    } else {
      memcpy(clk_sensing, &x3_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = true;
    }
  } else if (x3_is_none) {
    if (x1_isnt_clk) {
      memcpy(clk_sensing, &x2_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = false;
    } else {
      memcpy(clk_sensing, &x1_sensing, sizeof(dial_clk_sensing_t));
      clk_sensing->next_is_data = true;
    }
  } else {
    return DIAL_UNRECOGNIZED_PINS;
  }

  return DIAL_OK;
}

static dial_error_t dial_get_device(dial_clk_sensing_t* clk_sensing, dial_devices_t* device) {
  // GET DEVICE
  if (clk_sensing->rising_flags == 52) {
    *device = DIAL_LITTLE;
  } else if (clk_sensing->rising_flags == 28) {
    *device = DIAL_BIG;
  } else {
    return DIAL_UNRECOGNIZED_DEVICE;
  }

  return DIAL_OK;
}

static void dial_get_databits(dial_clk_sensing_t* clk_sensing, uint64_t* databits) {
  // ORDER DATABITS
  uint64_t* reverse_databits = (clk_sensing->next_is_data) ? (clk_sensing->nextPin_val) : (clk_sensing->prevPin_val);
  uint64_t ordered_databits = 0;
  for (int i = 0; i < 64; i++) {
    ordered_databits |= ((uint64_t)reverse_databits[i] << i);
  }

  *databits = ordered_databits;
}

static void dial_short_parse_databits(uint64_t databits, dial_output_t* output) {

  // GROUP BITS
  uint64_t group_one = (databits >> 48) & 0b1111;
  uint64_t group_three = (databits >> 40) & 0b1111;
  uint64_t group_four = (databits >> 36) & 0b1111;
  uint64_t group_five = (databits >> 32) & 0b1111;
  uint64_t group_six = (databits >> 28) & 0b1111;
  uint64_t group_seven = (databits >> 24) & 0b1111;
  uint64_t group_eight = (databits >> 20) & 0b1111;
  uint64_t group_nine = (databits >> 16) & 0b1111;

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
}

static void dial_big_parse_databits(uint64_t databits, dial_output_t* output) {

  // GROUP BITS
  uint64_t group_one = (databits >> 24) & 0b1111;
  uint64_t group_two = (databits >> 20) & 0b1111;
  uint64_t group_three = (databits >> 16) & 0b1111;
  uint64_t group_four = (databits >> 12) & 0b1111;
  uint64_t group_five = (databits >> 8) & 0b1111;
  uint64_t group_six = (databits >> 4) & 0b1111;

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
}

dial_error_t dial_get_value(dial_dasqua_t* dev, dial_output_t* output) {
  dial_error_t err;

  dial_start_continuous_request(dev);

  err = dial_wait_for_idle();
  if (err != DIAL_OK) {
    dial_stop_continuous_request(dev);
    return err;
  }

  dial_clk_sensing_t clk_sensing;
  err = dial_wait_for_data_and_clk(dev, &clk_sensing);
  dial_stop_continuous_request(dev);
  if (err != DIAL_OK) {
    return err;
  }

  dial_devices_t device;
  err = dial_get_device(&clk_sensing, &device);
  if (err != DIAL_OK) {
    return err;
  }

  uint64_t databits;
  dial_get_databits(&clk_sensing, &databits);

  if (device == DIAL_LITTLE) {
    dial_short_parse_databits(databits, output);
  } else {
    dial_big_parse_databits(databits, output);
  }

  return DIAL_OK;
}