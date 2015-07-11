#ifndef UTILS_H_
#define UTILS_H_


#ifdef __cplusplus
extern "C" {
#endif


//
// Updates the given variable with the current time in microseconds,
// and returns the time difference in seconds.
//
float timeUpdate(unsigned long *microTime);


// End C++ export structure
#ifdef __cplusplus
}
#endif

// End include guard
#endif
