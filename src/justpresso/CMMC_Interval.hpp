#ifndef CMMCVTimer_h
#define CMMCVTimer_h
#include <Arduino.h>
#include <functional>

typedef std::function<void(void)> void_callback_t;

class CMMC_Interval
{
  private:
    unsigned long _prev;
    unsigned long _now;
    unsigned long _threshold_ms;
    void_callback_t  _after_cb;

	public:

    CMMC_Interval() {
      _prev = millis();
      _now  = millis();
    };

    ~CMMC_Interval() {
    };

    void mark() {
      _prev = millis();
      _now = millis();
    }

    // void every_ms(unsigned long ms, void_callback_t cb) {
    //   _threshold_ms = ms;
    //   _now  = millis();
    //   unsigned long diff = _now - _prev;
    //   if (diff >= _threshold_ms) {
    //     _prev = millis();
    //     cb();
    //   }
    // }

    void after(unsigned long ms, void_callback_t cb) {
      _prev = millis();
      _threshold_ms = ms;
      if (_after_cb) {
        Serial.println("FORCE CALL AFTER CB");
        _after_cb();
      }
      _after_cb = cb;
    }

    void loop() {
      if (_after_cb) {
        unsigned long diff = millis() - _prev;
        // Serial.printf("DIFF %lu, THRESHOLD = %lu\r\n", diff, _threshold_ms);
        if (diff >= _threshold_ms) {
            _after_cb();
            _after_cb = NULL;
        }
        yield();
      }
    }
};


#endif
