/*
Lacrosse anemometer TX20 protocol converter to Davis anemometer protocol
Use an ESP32 (ADC 0-3.3V output)
Serge CLAUS
Version 0.1
GPL V3

Inputs:
  TX20 data
Outputs:
  OUT0: Wind speed (frequency)
  ADC0: Wind direction
*/

// 1600 rotations per hour or 2.25 seconds per rotation
// equals 1 mp/h wind speed (1 mp/h = 1609/3600 m/s)
// speed (m/s) = rotations * 1135.24 / delta t

void setup() {
}

void loop() {
}
