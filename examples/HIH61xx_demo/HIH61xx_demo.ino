#include <SoftWire.h>
#include <HIH61xx.h>
#include <AsyncDelay.h>

SoftWire sw(SDA, SCL);
HIH61xx<SoftWire> hih(sw);
AsyncDelay samplingInterval;

void setup(void)
{
#if F_CPU >= 12000000UL
    Serial.begin(115200);
#else
	Serial.begin(9600);
#endif

    // The pin numbers for SDA/SCL can be overridden at runtime.
	// sw.setSda(sdaPin);
	// sw.setScl(sclPin);
	sw.begin();  // Sets up pin mode for SDA and SCL

	hih.initialise();
	samplingInterval.start(3000, AsyncDelay::MILLIS);
}


bool printed = true;
void loop(void)
{
	if (samplingInterval.isExpired() && !hih.isSampling()) {
		hih.start();
		printed = false;
		samplingInterval.repeat();
		Serial.println("Sampling started");
	}

	hih.process();

	if (hih.isFinished() && !printed) {
		printed = true;
		// Print saved values
		Serial.print("RH: ");
		Serial.print(hih.getRelHumidity() / 100.0);
		Serial.println(" %");
		Serial.print("Ambient: ");
		Serial.print(hih.getAmbientTemp() / 100.0);
		Serial.println(" deg C");
		Serial.print("Status: ");
		Serial.println(hih.getStatus());
	}

}
