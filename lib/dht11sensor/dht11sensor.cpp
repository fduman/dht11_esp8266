#include <dht11sensor.h>
#include <Arduino.h>

DHT11::DHT11(uint32_t pin)
: _pin(pin)
{
}

DHT11::~DHT11()
{
}

void DHT11::initialize()
{
  pinMode(_pin, INPUT_PULLUP);
}

DHT11Result DHT11::prepareErrorResult(String errorMessage){
  DHT11Result result;
  result.hasError = true;
  result.errorMessage = errorMessage;
  return result;
}

uint32_t DHT11::pulse(uint32_t state, uint32_t timeout)
{
  uint32_t maxloops = microsecondsToClockCycles(timeout) / 16;
  uint32_t numloops = 0;

  while (digitalRead(_pin) == state) {
		if (numloops++ == maxloops)
			return 0;
  }

  return numloops;
}

DHT11Result DHT11::read()
{
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  delay(20);

  uint32_t cycles[80];

  noInterrupts();
  {
    pinMode(_pin, INPUT_PULLUP);

    if (pulseIn(_pin, HIGH) == 0) {
      return prepareErrorResult("Timeout waiting for start signal LOW pulse");
    }

    for (int i = 0; i < 80; i+= 2) {
      cycles[i] = pulse(LOW, 150);
      cycles[i + 1] = pulse(HIGH, 150);
    }

    interrupts();
  }

  uint32_t data[5];
  data[0] = data[1] = data[2] = data[3] = data[4] = 0;

  for (int i = 0; i < 40; ++i) {
    uint32_t lowCycles  = cycles[2 * i];
    uint32_t highCycles = cycles[2 * i + 1];

    data[i/8] <<= 1;

    if (highCycles > lowCycles) {
      data[i/8] |= 1;
    }
  }

  uint32_t calculatedParity = (data[0] + data[1] + data[2] + data[3]) & 0xFF;

  if (data[4] != calculatedParity)
  {
    return prepareErrorResult("Parity check failed");
  }

  DHT11Result result;
  result.hasError = false;
  result.errorMessage = "";
  result.humidity = data[0];
  result.temperatureC = data[2];
  return result;
}
