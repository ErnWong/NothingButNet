#ifndef LOWPASS_H_
#define LOWPASS_H_

#ifdef __cplusplus
extern "C" {
#endif


//
// Reference type of an initialized low-pass filter.
//
typedef void* LowPass;

//
// Initializes a low-pass filter with a time constant.
//
LowPass lowPassInit(float timeConstant);

//
// Updates the filter for the next value, and returns the new output.
//
float lowPassUpdate(LowPass filter, float input);

//
// Resets the output of the low-pass filter to the given value,
// and sets the previous time to the current time.
//
void lowPassReset(LowPass filter, float value);

//
// Sets the time constant of the low-pass filter.
//
void lowPassAdjust(LowPass filter, float timeConstant);


// End C++ export structure
#ifdef __cplusplus
}
#endif

// End include guard
#endif
