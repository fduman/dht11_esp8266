#include <Arduino.h>

struct DHT11Result {
  uint32_t humidity;
  uint32_t temperatureC;
  bool hasError;
  String errorMessage;
};

class DHT11
{
  public:
    DHT11(uint32_t pin);

    ~DHT11();

    void initialize();

    DHT11Result read();

  protected:
    DHT11Result prepareErrorResult(String errorMessage);
    uint32_t pulse(uint32_t state, uint32_t timeout);
    uint32_t _pin;
};
