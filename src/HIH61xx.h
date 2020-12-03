#ifndef HIH61XX_H
#define HIH61XX_H

#define HIH61XX_VERSION "2.0.2"

#define HIH61XX_DEFAULT_ADDRESS 0x27

#include <AsyncDelay.h>


template <class T> class HIH61xx {
  public:
    enum status_t {
      statusNormal = 0,    // Defined by HIH61xx device
      statusStaleData = 1, // Defined by HIH61xx device
      statusCmdMode = 2,   // Defined by HIH61xx device
      statusNotUsed = 3,   // Defined by HIH61xx device
      statusUninitialised = 4,
      statusTimeout = 5,
    };

    static const uint8_t defaultAddress = HIH61XX_DEFAULT_ADDRESS;
    static const uint8_t powerUpDelay_ms = 75; // Data sheet indicates 60ms
    static const uint8_t conversionDelay_ms = 45; // "Typically 36.65ms"

    HIH61xx(T &i2c, uint8_t address = defaultAddress);

    inline int16_t getAmbientTemp(void) const {
      return _ambientTemp;
    }
    inline uint16_t getRelHumidity(void) const {
      return _relHumidity;
    }
    inline uint8_t getStatus(void) const {
      return _status;
    }

    inline bool isFinished(void) const {
      return _state == finished;
    }

    // start called, results not ready
    inline bool isSampling(void) const {
      return !(_state == off || _state == finished);
    }

    inline bool isPowerOff(void) const {
      return (_state == off || _state == finished);
    }

	inline void setPowerUpErrorHandler(void (*handler)(HIH61xx& hih)) {
		_powerUpErrorHandler = handler;
	}

	inline void setReadErrorHandler(void (*handler)(HIH61xx& hih)) {
		_readErrorHandler = handler;
	}

    void initialise(uint8_t power = 255);

    void start(void); // To include power-up (later), start sampling
    void process(void); // Call often to process state machine
    void finish(void); // Force completion and power-down

    bool read(void); // Simple blocking read


  private:
    enum state_t {
      off,
      poweringUp, // power applied, waiting for timeout
      converting, // Conversion started, waiting for completion
      reading, // Ready to read results
      poweringDown,
      finished, // Results read
    };

    uint8_t _address;
    uint8_t _powerPin;
    state_t _state;
    T &_i2c;

    int16_t _ambientTemp;
    uint16_t _relHumidity;
    status_t _status;
    AsyncDelay _delay;

	void (*_powerUpErrorHandler)(HIH61xx& hih);
	void (*_readErrorHandler)(HIH61xx& hih);

    void errorDetected(void);
};


template <class T> HIH61xx<T>::HIH61xx(T &i2c, uint8_t address) : _address(address),
  _powerPin(255),
  _state(off),
  _i2c(i2c),
  _ambientTemp(32767),
  _relHumidity(65535),
  _status(statusUninitialised),
  _powerUpErrorHandler(nullptr),
  _readErrorHandler(nullptr)
{
  ;
}


template <class T> void HIH61xx<T>::initialise(uint8_t powerPin)
{
  _powerPin = powerPin;
  if (_powerPin != 255) {
    pinMode(_powerPin, OUTPUT);
    digitalWrite(_powerPin, LOW);
  }
  _delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);

  return;
}


template <class T> void HIH61xx<T>::start(void)
{
  if (_powerPin != 255) {
    digitalWrite(_powerPin, HIGH);
    _delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);
  }
  _state = poweringUp;
}


template <class T> void HIH61xx<T>::process(void)
{
  switch (_state) {
    case off:
      // Stay powered off until told to turn on
      break;

    case poweringUp:
      if (_delay.isExpired()) {
        _i2c.beginTransmission(_address);
        int errStatus;
        if ((errStatus = _i2c.endTransmission()) != 0) {
          errorDetected();
		  if (_powerUpErrorHandler) {
			  _powerUpErrorHandler(*this);
		  }
        }
        else {
          _delay.start(conversionDelay_ms, AsyncDelay::MILLIS);
          _state = converting;
        }
      }
      break;

    case converting:
      if (_delay.isExpired()) {
        _state = reading;
      }
      break;

    case reading:
      {
        const uint8_t bytesRequested = 4;
        uint8_t data[bytesRequested];
        int bytesRead;
        if ((bytesRead = _i2c.requestFrom(_address, bytesRequested)) != bytesRequested) {
          errorDetected();
		  if (_readErrorHandler) {
			  _readErrorHandler(*this);
		  }
          break;
        }
        else {
          for (uint8_t i = 0; i < bytesRequested; ++i)
            data[i] = _i2c.read();
        }
        _status = (status_t)(data[0] >> 6);
        uint16_t rawHumidity = ((((uint16_t)data[0] & 0x3F) << 8) | (uint16_t)data[1]);
        uint16_t rawTemp = ((uint16_t)data[2] << 6) | ((uint16_t)data[3] >> 2);

        _relHumidity = (long(rawHumidity) * 10000L) / 16382;
        _ambientTemp = ((long(rawTemp) * 16500L) / 16382) - 4000;
      }
      _state = poweringDown;
      break;

    case poweringDown:
      finish(); // Sets state to finished
      break;

    case finished:
      // Do nothing, remain in this state
      break;
  }
}


template <class T> void HIH61xx<T>::finish(void)
{
  //_i2c.stop(); // Release SDA and SCL
  if (_powerPin != 255)
    digitalWrite(_powerPin, LOW);
  _state = finished;
}


template <class T> bool HIH61xx<T>::read(void)
{
  start();
  while (!isFinished())
    process();
  return _status == statusNormal;
}


template <class T> void HIH61xx<T>::errorDetected(void)
{
  finish();
  _ambientTemp = 32767;
  _relHumidity = 65535;
  _status = statusTimeout;
}

#endif
