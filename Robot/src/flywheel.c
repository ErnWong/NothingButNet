#include "flywheel.h"

#include <API.h>
#include "utils.h"




// TODO: tune success intervals, priorities, and checking period.

#define FLYWHEEL_READY_ERROR_INTERVAL 1.0f      // The +/- interval for which the error needs to lie to be considered 'ready'.
#define FLYWHEEL_READY_DERIVATIVE_INTERVAL 1.0f // The +/- interval for which the measured derivative needs to lie to be considered 'ready'.

#define FLYWHEEL_ACTIVE_PRIORITY 3              // Priority of the update task during active mode
#define FLYWHEEL_READY_PRIORITY 2               // Priority of the update task during ready mode

#define FLYWHEEL_ACTIVE_DELAY 20                // Delay for each update during active mode
#define FLYWHEEL_READY_DELAY 200                // Delay for each update during ready mode

#define FLYWHEEL_CHECK_READY_PERIOD 20          // Number of updates before rechecking its ready state




//
// (private)
// The state, parameters, and working variables of a flywheel.
//
typedef struct FlywheelData		// TODO: look at packing and alignment
{

	float target;                       // Target speed in rpm.
	float measured;                     // Measured speed in rpm.
	float derivative;                   // Rate at which the measured speed had changed.
	float error;                        // Difference in the target and the measured speed in rpm.
	float action;                       // Controller output sent to the (smart) motors.

	int reading;                        // Previous encoder value.
	unsigned long microTime;            // The time in microseconds of the last update.
	float timeChange;                   // The time difference between updates, in seconds.

	float gain;                         // Gain proportional constant for integrating controller.
	float gearing;                      // Ratio of flywheel RPM per encoder RPM.
	float encoderTicksPerRevolution;    // Number of ticks each time the encoder completes one revolution
	float smoothing;                    // Amount of smoothing applied to the flywheel RPM, which is the low-pass filter time constant in seconds.

	bool ready;                         // Whether the controller is in ready mode (true, flywheel at the right speed) or active mode (false), which affects task priority and update rate.
	unsigned long delay;

	Mutex targetMutex;                  // Mutex for updating the target speed.
	TaskHandle task;                    // Handle to the controlling task.
	Encoder encoder;                    // Encoder used to measure the rpm.
}
FlywheelData;




//
// Private functions, forward declarations.
//

void update(FlywheelData *data);
void measure(FlywheelData *data);
void controllerUpdate(FlywheelData *data);
void checkReady(FlywheelData *data);
void activate(FlywheelData *data);
void readify(FlywheelData *data);




//
// Initializes a flywheel.
//
Flywheel flywheelInit(FlywheelSetup setup)
{
	FlywheelData *data = malloc(sizeof(FlywheelData));

	data->target = 0.0f;
	data->measured = 0.0f;
	data->derivative = 0.0f;
	data->error = 0.0f;
	data->action = 0.0f;

	data->reading = 0;
	data->microTime = micros();
	data->timeChange = 0.0f;

	data->gain = setup.gain;
	data->gearing = setup.gearing;
	data->encoderTicksPerRevolution = setup.encoderTicksPerRevolution;
	data->smoothing = setup.smoothing;

	data->ready = true;
	data->delay = FLYWHEEL_READY_DELAY;

	data->targetMutex = mutexCreate();
	data->task = taskCreate(task, 1000000, data, FLYWHEEL_READY_PRIORITY);	// TODO: What stack size should be set?
	data->encoder = encoderInit(setup.encoderPortTop, setup.encoderPortBottom, setup.encoderReverse);

	return (Flywheel)data;
}


//
// Sets the target reference speed for the flywheel.
//
void flywheelSet(Flywheel flywheel, float speed)
{
	FlywheelData *data = flywheel;

	mutexTake(data->targetMutex, -1); // TODO: figure out how long the block time should be.
	data->target = speed;
	mutexGive(data->targetMutex);
	
	if (data->ready)
	{
		activate(data);
	}
}


//
// (private)
// The task that updates the flywheel.
//
void task(void *flywheel)
{
	FlywheelData *data = flywheel;
	int i = 0;
	while (1)
	{
		i = FLYWHEEL_CHECK_READY_PERIOD;
		while (i)
		{
			update(data);
			delay(data->delay);
			--i;
		}
		checkReady(data);
	}
}


//
// (private)
// Updates the flywheel.
//
void update(FlywheelData *data)
{
	data->timeChange = timeUpdate(&data->microTime);
	measure(data);
	controllerUpdate(data);
	// TODO: update smart motor group.
}


//
// (private)
// Function: Measures the RPM of the flywheel, and the rate it changes.
// Implement: encoder change -> rpm -> low-pass filter
// Requires: timeChange (preferably calculated immediately before this function)
//
void measure(FlywheelData *data)
{
	int reading = encoderGet(data->encoder);
	int ticks = reading - data->reading;

	// Raw rpm
	float rpm = ticks / data->encoderTicksPerRevolution * data->gearing / data->timeChange;
	
	// Low-pass filter
	float measureChange = (rpm - data->measured) * data->timeChange / data->smoothing;

	// Update
	data->reading = reading;
	data->measured += measureChange;
	data->derivative = measureChange / data->timeChange;
}


//
// (private)
// Function: Calculates the error, and the action required to minimise the error.
// Implement: Proportionally integral controller.
// Requires: timeChange, measured
//
void controllerUpdate(FlywheelData *data)
{
	// Calculate error
	mutexTake(data->targetMutex, -1);	// TODO: Find out what block time is suitable, or needeed at all.
	data->error = data->measured - data->target;
	mutexGive(data->targetMutex);

	// Integrate
	data->action += data->timeChange * data->gain * data->error;	// TODO: try take-back-half controller.
}


//
// (private)
// Function: Checks if flywheel should be ready or active, and changes the flywhee's state if necessary.
// Implement: Checks whether the RPM and its derivative are within their respective ready intervals.
// Requires: error, derivative
//
void checkReady(FlywheelData *data)
{
	bool errorReady = -FLYWHEEL_READY_ERROR_INTERVAL < data->error < FLYWHEEL_READY_ERROR_INTERVAL;
	bool derivativeReady = -FLYWHEEL_READY_DERIVATIVE_INTERVAL < data->derivative < FLYWHEEL_READY_DERIVATIVE_INTERVAL;
	bool ready = errorReady && derivativeReady;

	if (ready && !data->ready)
	{
		readify(data);
	}
	else if (!ready && data->ready)
	{
		activate(data);
	}
}


//
// (private)
// Function: set the flywheel into active state, with faster updates and higher priority. Also signals the change.
//
void activate(FlywheelData *data)
{
	data->ready = false;
	data->delay = FLYWHEEL_ACTIVE_DELAY;
	taskPrioritySet(data->task, FLYWHEEL_ACTIVE_PRIORITY);
	// TODO: Signal not ready?
}


//
// (private)
// Function: set the flywheel into ready state, with slower updates and lower priority. Also signals the change.
//
void readify(FlywheelData *data)
{
	data->ready = true;
	data->delay = FLYWHEEL_READY_DELAY;
	taskPrioritySet(data->task, FLYWHEEL_READY_PRIORITY);
	// TODO: Signal ready?
}