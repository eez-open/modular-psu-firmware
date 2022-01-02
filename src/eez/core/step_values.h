#pragma once

#include <eez/unit.h>

namespace eez {

enum EncoderMode {
	ENCODER_MODE_MIN,
	ENCODER_MODE_AUTO = ENCODER_MODE_MIN,
	ENCODER_MODE_STEP1,
	ENCODER_MODE_STEP2,
	ENCODER_MODE_STEP3,
	ENCODER_MODE_STEP4,
	ENCODER_MODE_STEP5,
	ENCODER_MODE_MAX = ENCODER_MODE_STEP5
};

struct StepValues {
	int count;
	const float *values;
	Unit unit;
	struct {
		bool accelerationEnabled;
		float range;
		float step;
		EncoderMode mode;
	} encoderSettings;
};

} // namespace eez