#include "dial_dasqua.h"

#define DIAL_X1_PIN 14
#define DIAL_X2_PIN 27
#define DIAL_X3_PIN 25
#define DIAL_REQ_PIN 26
dial_dasqua_t mydial = { DIAL_X1_PIN, DIAL_X2_PIN, DIAL_X3_PIN, DIAL_REQ_PIN };

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  dial_init(&mydial);
}

void loop() { 
  
  // put your main code here, to run repeatedly:
  dial_output_t output;
  dial_error_t err = dial_get_value(&mydial, &output);
  if (err == DIAL_OK) {
    Serial.print("Value: ");
    Serial.print(output.value, 5);
    Serial.print("\t");
    Serial.print("Unit: ");
    Serial.println(output.unit == MM ? "mm" : "inch");
  } else {
    Serial.print("Error: ");
    Serial.println(err);
  }

  delay(1000);
  
}
