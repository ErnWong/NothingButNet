#include "lowpass.h"

#include <API.h>
#include "utils.h"




//
// (private)
// The state and parameters of a low-pass filter.
//
typedef struct LowPassData
{
	float out;
	unsigned long microTime;
	float timeConstant;
}
LowPassData;


//
// Initializes a low-pass filter with a time constant.
//
LowPass lowPassInit(float initial, float timeConstant)
{
	LowPassData *data = malloc(sizeof(LowPassData));

	data->out = initial;
	data->timeConstant = timeConstant;
	data->microTime = micros();

	return (LowPass)data;
}


//
// Updates the filter for the next value, and returns the new output.
//
float lowPassUpdate(LowPass filter, float input)
{
	LowPassData *data = filter;

	float timeChange = timeUpdate(&data->microTime);
	float factor = timeChange / (data->timeConstant + timeChange);
	float output = input * factor + data->out * (1 - factor);

	data->out = output;
	return output;
}


//
// Resets the output of the low-pass filter to the given value,
// and sets the time to the current time.
//
void lowPassReset(LowPass filter, float value)
{
	LowPassData *data = filter;

	data->out = value;
	data->microTime = micros();
}


//
// Sets the time constant for the low-pass filter.
//
void lowPassAdjust(LowPass filter, float timeConstant)
{
	LowPassData *data = filter;

	data->timeConstant = timeConstant;
}