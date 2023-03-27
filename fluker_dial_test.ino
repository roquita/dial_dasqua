#include "dial_dasqua.h"

#define DIAL_CLK_PIN 14
#define DIAL_DATA_PIN 27
dial_dasqua_t mydial = { DIAL_CLK_PIN, DIAL_DATA_PIN };

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  dial_init(&mydial);
}

void loop() {
  // put your main code here, to run repeatedly:
  dial_output_t output = dial_get_value(&mydial);
  Serial.print("Value: ");
  Serial.print(output.value, 4);
  Serial.print("\t");
  Serial.print("Unit: ");
  Serial.println(output.unit == MM ? "mm" : "inch");
  delay(100);
}
