#ifndef HIH61XX_H
#define HIH61XX_H

#define HIH61XX_VERSION "1.0.1"

#define HIH61XX_DEFAULT_ADDRESS 0x27

//#if HIH61XX_I2C_LIBRARY == Wire
//#include <Wire.h>
//#elif HIH61XX_I2C_LIBRARY == SoftWire
//#include <SoftWire.h>
//#else
//#error "Unknown I2C library: " ## HIH61XX_I2C_LIBRARY
//#endif

#include <Wire.h>
#include <SoftWire.h>

#include <AsyncDelay.h>

template <class T> class HIH61xx {
//class HIH61xx {
public:
	static const uint8_t defaultAddress = HIH61XX_DEFAULT_ADDRESS;
	static const uint8_t powerUpDelay_ms = 75; // Data sheet indicates 60ms
	static const uint8_t conversionDelay_ms = 45; // "Typically 36.65ms"

	HIH61xx(T &i2c);

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

	bool initialise(uint8_t sda, uint8_t scl, uint8_t power = 255);
	bool initialise(uint8_t power = 255);

	void start(void); // To include power-up (later), start sampling
	void process(void); // Call often to process state machine
	void finish(void); // Force completion and power-down
	void timeoutDetected(void);

private:
	enum state_t {
		off,
		poweringUp, // power applied, waiting for timeout
		converting, // Conversion started, waiting for completion
		reading, // Ready to read results
		poweringDown,
		finished, // Results read
	};

	enum status_t {
		statusNormal = 0,    // Defined by HIH61xx device
		statusStaleData = 1, // Defined by HIH61xx device
		statusCmdMode = 2,   // Defined by HIH61xx device
		statusNotUsed = 3,   // Defined by HIH61xx device
		statusUninitialised = 4,
		statusTimeout = 5,
	};

	uint8_t _address;
	uint8_t _powerPin;
	state_t _state;
	T &_i2c;

	int16_t _ambientTemp;
	uint16_t _relHumidity;
	uint8_t _status;
	AsyncDelay _delay;
};


template <class T>
HIH61xx<T>::HIH61xx(T &i2c) : _address(defaultAddress),
						 _powerPin(255),
						 _state(off),
						 _i2c(i2c),
						 _ambientTemp(32767),
						 _relHumidity(65535),
						 _status(statusUninitialised)
{
	;
}


template <class T>
bool HIH61xx<T>::initialise(uint8_t sdaPin, uint8_t sclPin, uint8_t powerPin)
{
	_i2c.setSda(sdaPin);
	_i2c.setScl(sclPin);
	_i2c.begin();  // Sets up pin mode for SDA and SCL
	_powerPin = powerPin;
	if (_powerPin != 255) {
		pinMode(_powerPin, OUTPUT);
		digitalWrite(_powerPin, LOW);
	}
	// TODO: check presence of HIH61xx

	// Use the delay so that even when always on the power-up delay is
	// observed from initialisation
	_delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);

	return true;
}


template <class T>
bool HIH61xx<T>::initialise(uint8_t powerPin)
{
	_powerPin = powerPin;
	if (_powerPin != 255) {
		pinMode(_powerPin, OUTPUT);
		digitalWrite(_powerPin, LOW);
	}
	// TODO: check presence of HIH61xx

	// Use the delay so that even when always on the power-up delay is
	// observed from initialisation
	_delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);

	return true;
}


template <class T>
void HIH61xx<T>::start(void)
{
	if (_powerPin != 255) {
		digitalWrite(_powerPin, HIGH);
		_delay.start(powerUpDelay_ms, AsyncDelay::MILLIS);
	}
	_state = poweringUp;
}


template <class T>
void HIH61xx<T>::process(void)
{
	switch (_state) {
	case off:
		// Stay powered off until told to turn on
		break;

	case poweringUp:
		if (_delay.isExpired()) {
			uint8_t s = _i2c.start(defaultAddress, SoftWire::writeMode);
			_i2c.stop();
			if (s == SoftWire::timedOut)
				timeoutDetected();
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
			uint8_t data[4];
			if (_i2c.start(_address, SoftWire::readMode) ||
				_i2c.readThenAck(data[0]) ||
				_i2c.readThenAck(data[1]) ||
				_i2c.readThenAck(data[2]) ||
				_i2c.readThenNack(data[3])) {
				    _i2c.stop();
				    timeoutDetected();
				    break;
			}
			_i2c.stop();
			_status = data[0] >> 6;
			uint16_t rawHumidity = ((((uint16_t)data[0] & 0x3F) << 8) |
									(uint16_t)data[1]);
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


template <class T>
void HIH61xx<T>::finish(void)
{
	_i2c.stop(); // Release SDA and SCL
	if (_powerPin != 255)
		digitalWrite(_powerPin, LOW);
	_state = finished;
}


template <class T>
void HIH61xx<T>::timeoutDetected(void)
{
	finish();
	_ambientTemp = 32767;
	_relHumidity = 65535;
	_status = statusTimeout;
}

#endif
