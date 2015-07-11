#include "flywheel.h"

#include <API.h>
#include "lowpass.h"
#include "utils.h"




// TODO: tune success intervals, priorities, and checking period.

#define FLYWHEEL_READY_ERROR_INTERVAL 1.0f
#define FLYWHEEL_READY_DERIVATIVE_INTERVAL 1.0f

#define FLYWHEEL_ACTIVE_PRIORITY 3
#define FLYWHEEL_READY_PRIORITY 2

#define FLYWHEEL_ACTIVE_DELAY 20
#define FLYWHEEL_READY_DELAY 200

#define FLYWHEEL_CHECK_READY_PERIOD 20




//
// (private)
// The state, parameters, and working variables of a flywheel.
//
typedef struct FlywheelData
{
	float target;		// Target speed in rpm.
	float measured;		// Measured speed in rpm.
	float derivative;	// Rate at which the measured speed had changed.
	float error;		// Difference in the target and the measured speed in rpm.
	float action;		// Controller output sent to the (smart) motors.

	int reading;		// Previous encoder value.
	unsigned long microTime;		// The time in microseconds of the last update.
	float timeChange;		// The time difference between updates, in seconds.

	float gain;		// Gain proportional constant for integrating controller.
	float gearing;		// Ratio of flywheel RPM per encoder RPM.

	bool ready;
	unsigned long delay;

	Mutex targetMutex;		// Mutex for updating the target speed.
	TaskHandle task;		// Handle to the controlling task.
	Encoder encoder;		// Encoder used to measure the rpm.
	LowPass filter;		// Low-pass filter used to filter the measured rpm.
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

	data->ready = true;
	data->delay = FLYWHEEL_READY_DELAY;

	data->targetMutex = mutexCreate();
	data->task = taskCreate(task, 1000000, data, FLYWHEEL_READY_PRIORITY);	// TODO: What stack size should be set?
	data->encoder = encoderInit(setup.encoderPortTop, setup.encoderPortBottom, setup.encoderReverse);
	data->filter = lowPassInit(setup.smoothing);

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
	int tick = 0;
	while (1)
	{
		tick = FLYWHEEL_CHECK_READY_PERIOD;
		while (tick)
		{
			update(data);
			delay(data->delay);
			--tick;
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
// Implementation: encoder change -> rpm -> low-pass filter
// Requires: timeChange
//
void measure(FlywheelData *data)
{
	int reading = encoderGet(data->encoder);
	float rpm = (reading - data->reading) / 360 * data->gearing / data->timeChange;
	float oldMeasured = data->measured;

	data->measured = lowPassUpdate(data->filter, rpm);
	data->derivative = (data->measured - oldMeasured) / data->timeChange;
}


//
// (private)
// Function: Calculates the error, and the action required to minimise the error.
// Implementation: Proportionally integral controller.
// Requires: timeChange, measured
//
void controllerUpdate(FlywheelData *data)
{
	mutexTake(data->targetMutex, -1);	// TODO: Find out what block time is suitable, or needeed at all.
	data->error = data->measured - data->target;
	mutexGive(data->targetMutex);
	data->action += data->timeChange * data->gain * data->error;	// TODO: try take-back-half controller.
}


//
// (private)
// Function: Checks if flywheel should be ready or active, and changes the flywhee's state if necessary.
// Implementation: Checks whether the RPM and its derivative are within their respective ready intervals.
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
	data->ready = true;
	data->delay = FLYWHEEL_READY_DELAY;
	taskPrioritySet(data->task, FLYWHEEL_READY_PRIORITY);
	// TODO: Signal ready?
}


//
// (private)
// Function: set the flywheel into ready state, with slower updates and lower priority. Also signals the change.
//
void readify(FlywheelData *data)
{
	data->ready = false;
	data->delay = FLYWHEEL_ACTIVE_DELAY;
	taskPrioritySet(data->task, FLYWHEEL_ACTIVE_PRIORITY);
	// TODO: Signal not ready?
}