#ifndef FLYWHEEL_H_
#define FLYWHEEL_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


//
// Reference type for an initialized flywheel.
//
typedef void* Flywheel;

//
// Setup parameters for a flywheel.
//
typedef struct FlywheelSetup
{
	float gearing;		// Ratio of flywheel RPM per encoder RPM.
	float gain;		// Gain proportional constant for integrating controller.
	float smoothing;		// Amount of smoothing applied to the flywheel RPM, as the low-pass time constant in seconds.
	unsigned char encoderPortTop;		// Digital port number where the encoder's top wire is connected.
	unsigned char encoderPortBottom;		// Digital port number where the encoder's bottom wire is connected.  
	bool encoderReverse;		// Whether the encoder values should be reversed.
}
FlywheelSetup;

//
// Initializes a flywheel.
//
Flywheel flywheelInit(FlywheelSetup setup);

//
// Sets the target reference speed for the flywheel.
//
void flywheelSet(Flywheel flywheel, float rpm);


// End C++ export structure
#ifdef __cplusplus
}
#endif

// End include guard
#endif
