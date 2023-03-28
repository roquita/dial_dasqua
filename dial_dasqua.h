#pragma once


#define DIAL_SAMPLING_US 10
#define DIAL_IDLE_REQUIRED_US (50 * 1000)
#define DIAL_TIMEOUT_US (120*1000)
#define DIAL_IDLE_TICKS (DIAL_IDLE_REQUIRED_US / DIAL_SAMPLING_US)
#define DIAL_TIMEOUT_TICKS (DIAL_TIMEOUT_US / DIAL_SAMPLING_US)

typedef enum {
  MM = 0,
  INCH = 1,
} dial_unit_t;

typedef enum {
  NEGATIVE = -1,
  POSITIVE = 1,
} dial_sign_t;

typedef struct {
  float value;
  dial_unit_t unit;
  dial_sign_t sign;
} dial_output_t;

typedef struct {
  int clk_pin;
  int data_pin;
  int req_pin;
} dial_dasqua_t;

void dial_init(dial_dasqua_t* dev);
dial_output_t dial_get_value(dial_dasqua_t* dev);