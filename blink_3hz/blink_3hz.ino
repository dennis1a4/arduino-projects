void setup() {
  // Initialize the built-in LED pin as an output
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  // Turn the LED on
  digitalWrite(LED_BUILTIN, HIGH);
  delay(167);  // Wait for 167ms
  
  // Turn the LED off
  digitalWrite(LED_BUILTIN, LOW);
  delay(166);  // Wait for 166ms (total cycle = 333ms for 3 Hz)
}
