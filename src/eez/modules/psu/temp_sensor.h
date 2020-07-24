/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#pragma once

#define TEMP_SENSORS \
    TEMP_SENSOR(AUX, QUES_TEMP, SCPI_ERROR_AUX_TEMP_SENSOR_TEST_FAILED), \
    TEMP_SENSOR(CH1, QUES_ISUM_TEMP, SCPI_ERROR_CH1_TEMP_SENSOR_TEST_FAILED), \
    TEMP_SENSOR(CH2, QUES_ISUM_TEMP, SCPI_ERROR_CH2_TEMP_SENSOR_TEST_FAILED), \
	TEMP_SENSOR(CH3, QUES_ISUM_TEMP, SCPI_ERROR_CH3_TEMP_SENSOR_TEST_FAILED), \
	TEMP_SENSOR(CH4, QUES_ISUM_TEMP, SCPI_ERROR_CH4_TEMP_SENSOR_TEST_FAILED), \
	TEMP_SENSOR(CH5, QUES_ISUM_TEMP, SCPI_ERROR_CH5_TEMP_SENSOR_TEST_FAILED), \
	TEMP_SENSOR(CH6, QUES_ISUM_TEMP, SCPI_ERROR_CH6_TEMP_SENSOR_TEST_FAILED)

namespace eez {
namespace psu {
namespace temp_sensor {

////////////////////////////////////////////////////////////////////////////////

static const int MAX_NUM_TEMP_SENSORS = 7;

////////////////////////////////////////////////////////////////////////////////

#define TEMP_SENSOR(NAME, QUES_REG_BIT, SCPI_ERROR) NAME
enum Type {
	TEMP_SENSORS,
	NUM_TEMP_SENSORS
};
#undef TEMP_SENSOR

////////////////////////////////////////////////////////////////////////////////

class TempSensor {
public:
	TempSensor(Type type, const char *name, int ques_bit, int scpi_error);

    Type type;
	const char *name;
	int ques_bit;
	int scpi_error;

	TestResult g_testResult;

	bool isInstalled();
	
	Channel *getChannel();

	void init();
	bool test();

	float read();

private:
    float doRead();

	void testTemperatureValidity(float value);
};

////////////////////////////////////////////////////////////////////////////////

extern TempSensor sensors[NUM_TEMP_SENSORS];

}
}
} // namespace eez::psu::temp_sensor
