#pragma once


#define DIAL_SAMPLING_US 10//10
#define DIAL_OK_IDLE_US  (80*1000)//(50 * 1000)
#define DIAL_OK_IDLE_TICKS (DIAL_OK_IDLE_US / DIAL_SAMPLING_US)
#define DIAL_TIMEOUT_IDLE_US (250*1000)
#define DIAL_TIMEOUT_IDLE_TICKS (DIAL_TIMEOUT_IDLE_US / DIAL_SAMPLING_US)
#define DIAL_ACTIVE_US (80*1000) // (80*1000)
#define DIAL_ACTIVE_TICKS (DIAL_ACTIVE_US / DIAL_SAMPLING_US)
#define DIAL_MAX_RISING_FLAGS 64


//#define DIAL_TIMEOUT_US (120*1000)
//#define DIAL_TIMEOUT_TICKS (DIAL_TIMEOUT_US / DIAL_SAMPLING_US)


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
  int x1_pin;
  int x2_pin;
  int x3_pin;
  int req_pin;
} dial_dasqua_t;

typedef enum{
  DIAL_OK=0,
  DIAL_TIMEOUT_IDLE,
  DIAL_UNRECOGNIZED_PINS,
  DIAL_INVALID_RISING_PULSES,
}dial_error_t;

dial_error_t dial_init(dial_dasqua_t* dev);
dial_error_t dial_get_value(dial_dasqua_t* dev, dial_output_t* output);