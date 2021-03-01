/*
* EEZ PSU Firmware
* Copyright (C) 2020-present, Envox d.o.o.
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

#include <string.h>
#include <memory.h>

#include "./dlog_file.h"

namespace eez {
namespace dlog_file {

static const uint32_t MAGIC1 = 0x2D5A4545;
static const uint32_t MAGIC2 = 0x474F4C44;

////////////////////////////////////////////////////////////////////////////////

void Parameters::initYAxis(int yAxisIndex) {
	memcpy(&yAxes[yAxisIndex], &yAxis, sizeof(yAxis));
	yAxes[yAxisIndex].unit = UNIT_UNKNOWN;
	yAxes[yAxisIndex].dataType = DATA_TYPE_FLOAT;
	yAxes[yAxisIndex].transformOffset = 0;
	yAxes[yAxisIndex].transformScale = 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

Writer::Writer(uint8_t *buffer, uint32_t bufferSize)
    : m_buffer(buffer)
    , m_bufferSize(bufferSize)
	, m_bufferIndex(0)
	, m_fileLength(0)
	, m_bitMask(0)
	, m_bits(0)
	, m_dataOffset(0)
{
}

void Writer::reset() {
	m_bufferIndex = 0;
	m_fileLength = 0;
	m_bitMask = 0;
	m_bits = 0;
	m_dataOffset = 0;
}

void Writer::writeFileHeaderAndMetaFields(const Parameters &parameters) {
    // header
    writeUint32(MAGIC1);
    writeUint32(MAGIC2);
    writeUint16(VERSION2);
    writeUint16(parameters.numYAxes);
    uint32_t savedBufferIndex = m_bufferIndex;
    writeUint32(0);

    // meta fields
    writeStringField(FIELD_ID_COMMENT, parameters.comment);

	writeUint32Field(FIELD_ID_START_TIME, parameters.startTime);
	writeDoubleField(FIELD_ID_DURATION, parameters.finalDuration);
	m_finalDurationFieldOffset = m_bufferIndex - 8;

	writeUint8Field(FIELD_ID_DATA_CONTAINS_SAMPLE_VALIDITY_BIT, parameters.dataContainsSampleValidityBit ? 1 : 0);

    writeUint8Field(FIELD_ID_X_UNIT, parameters.xAxis.unit);
    writeFloatField(FIELD_ID_X_STEP, parameters.xAxis.step);
    writeUint8Field(FIELD_ID_X_SCALE_TYPE, parameters.xAxis.scaleType);
    writeFloatField(FIELD_ID_X_RANGE_MIN, parameters.xAxis.range.min);
    writeFloatField(FIELD_ID_X_RANGE_MAX, parameters.xAxis.range.max);
    writeStringField(FIELD_ID_X_LABEL, parameters.xAxis.label);

    bool writeChannelFields[MAX_NUM_OF_CHANNELS];
    for (uint8_t channelIndex = 0; channelIndex < MAX_NUM_OF_CHANNELS; channelIndex++) {
        writeChannelFields[channelIndex] = false;
    }

    if (parameters.yAxis.unit != UNIT_UNKNOWN) {
        writeUint8FieldWithIndex(FIELD_ID_Y_UNIT, parameters.yAxis.unit, 0);
		writeUint8FieldWithIndex(FIELD_ID_Y_DATA_TYPE, parameters.yAxis.dataType, 0);
        writeFloatFieldWithIndex(FIELD_ID_Y_RANGE_MIN, parameters.yAxis.range.min, 0);
        writeFloatFieldWithIndex(FIELD_ID_Y_RANGE_MAX, parameters.yAxis.range.max, 0);
        writeStringFieldWithIndex(FIELD_ID_Y_LABEL, parameters.yAxis.label, 0);
        writeUint8FieldWithIndex(FIELD_ID_Y_CHANNEL_INDEX, 0, 0);
		writeDoubleFieldWithIndex(FIELD_ID_Y_TRANSFORM_OFFSET, parameters.yAxis.transformOffset, 0);
		writeDoubleFieldWithIndex(FIELD_ID_Y_TRANSFORM_SCALE, parameters.yAxis.transformScale, 0);
    }

    for (uint8_t yAxisIndex = 0; yAxisIndex < parameters.numYAxes; yAxisIndex++) {
        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].unit != parameters.yAxis.unit) {
            writeUint8FieldWithIndex(FIELD_ID_Y_UNIT, parameters.yAxes[yAxisIndex].unit, yAxisIndex + 1);
        }

        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].dataType != parameters.yAxis.dataType) {
            writeUint8FieldWithIndex(FIELD_ID_Y_DATA_TYPE, parameters.yAxes[yAxisIndex].dataType, yAxisIndex + 1);
        }

        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].range.min != parameters.yAxis.range.min) {
            writeFloatFieldWithIndex(FIELD_ID_Y_RANGE_MIN, parameters.yAxes[yAxisIndex].range.min, yAxisIndex + 1);
        }

        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].range.max != parameters.yAxis.range.max) {
            writeFloatFieldWithIndex(FIELD_ID_Y_RANGE_MAX, parameters.yAxes[yAxisIndex].range.max, yAxisIndex + 1);
        }

        if (parameters.yAxis.unit == UNIT_UNKNOWN || strcmp(parameters.yAxes[yAxisIndex].label, parameters.yAxis.label) != 0) {
            writeStringFieldWithIndex(FIELD_ID_Y_LABEL, parameters.yAxes[yAxisIndex].label, yAxisIndex + 1);
        }

		writeUint8FieldWithIndex(FIELD_ID_Y_CHANNEL_INDEX, parameters.yAxes[yAxisIndex].channelIndex + 1, yAxisIndex + 1);
		if (
			parameters.yAxes[yAxisIndex].channelIndex >= 0 && 
			parameters.yAxes[yAxisIndex].channelIndex < MAX_NUM_OF_CHANNELS && 
			parameters.yAxes[yAxisIndex].dataType == DATA_TYPE_FLOAT
		) {
			writeChannelFields[parameters.yAxes[yAxisIndex].channelIndex] = true;
		}

        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].transformOffset != parameters.yAxis.transformOffset) {
            writeDoubleFieldWithIndex(FIELD_ID_Y_TRANSFORM_OFFSET, parameters.yAxes[yAxisIndex].transformOffset, yAxisIndex + 1);
        }

        if (parameters.yAxis.unit == UNIT_UNKNOWN || parameters.yAxes[yAxisIndex].transformScale != parameters.yAxis.transformScale) {
            writeDoubleFieldWithIndex(FIELD_ID_Y_TRANSFORM_SCALE, parameters.yAxes[yAxisIndex].transformScale, yAxisIndex + 1);
        }
    }

    writeUint8Field(FIELD_ID_Y_SCALE_TYPE, parameters.yAxisScaleType);

    for (uint8_t channelIndex = 0; channelIndex < MAX_NUM_OF_CHANNELS; channelIndex++) {
        if (writeChannelFields[channelIndex]) {
            const ChannelInfo &channelInfo = parameters.channels[channelIndex];
            writeUint16FieldWithIndex(FIELD_ID_CHANNEL_MODULE_TYPE, channelInfo.moduleType, channelIndex + 1);
            writeUint16FieldWithIndex(FIELD_ID_CHANNEL_MODULE_REVISION, channelInfo.moduleRevision, channelIndex + 1);
        }
    }

	writeUint32Field(FILE_ID_TEXT_INDEX_FILE_OFFSET, 0);
	m_textIndexFileOffset = m_bufferIndex - 4;
	
	writeUint32Field(FILE_ID_TEXT_FILE_OFFSET, 0);
	m_textFileOffset = m_bufferIndex - 4;

    writeUint16(0); // end of meta fields section

    // write beginning of data offset
	m_dataOffset = 4 * ((m_bufferIndex + 3) / 4);
    m_bufferIndex = savedBufferIndex;
    writeUint32(m_dataOffset);
    m_bufferIndex = m_dataOffset;
}

void Writer::writeBit(int bit) {
    if (m_bitMask == 0) {
        m_bitMask = 0x80;
    } else {
        m_bitMask >>= 1;
    }

    if (bit) {
        m_bits |= m_bitMask;
    }

    if (m_bitMask == 1) {
        flushBits();
    }
}

void Writer::writeUint8(uint8_t value) {
    *(m_buffer + (m_bufferIndex % m_bufferSize)) = value;
    m_bufferIndex++;
    m_fileLength++;
}

void Writer::writeUint16(uint16_t value) {
	auto p = (uint8_t *)&value;
	writeUint8(*p++);
	writeUint8(*p);
}

void Writer::writeUint32(uint32_t value) {
	auto p = (uint8_t *)&value;
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p);
}

void Writer::writeUint64(uint64_t value) {
	auto p = (uint8_t *)&value;
    writeUint8(*p++);
    writeUint8(*p++);
    writeUint8(*p++);
    writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p);
}

void Writer::writeFloat(float value) {
	auto p = (uint8_t *)&value;
    writeUint8(*p++);
    writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p);
}

void Writer::writeDouble(double value) {
	auto p = (uint8_t *)&value;
    writeUint8(*p++);
    writeUint8(*p++);
    writeUint8(*p++);
    writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p++);
	writeUint8(*p);
}

void Writer::writeInt16(uint8_t *value) {
	if (value) {
		writeUint8(*value++);
		writeUint8(*value);
	} else {
		writeUint8(0);
		writeUint8(0);
	}
}

void Writer::writeInt24(uint8_t *value) {
	if (value) {
		writeUint8(*value++);
		writeUint8(*value++);
		writeUint8(*value);
	} else {
		writeUint8(0);
		writeUint8(0);
		writeUint8(0);
	}
}

void Writer::writeUint8Field(uint8_t id, uint8_t value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(value);
}

void Writer::writeUint8FieldWithIndex(uint8_t id, uint8_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t));
    writeUint8(id);
    writeUint8(index);
    writeUint8(value);
}

void Writer::writeUint16Field(uint8_t id, uint16_t value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint16_t));
    writeUint8(id);
    writeUint16(value);
}

void Writer::writeUint16FieldWithIndex(uint8_t id, uint16_t value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t));
    writeUint8(id);
    writeUint8(index);
    writeUint16(value);
}

void Writer::writeUint32Field(uint8_t id, uint16_t value) {
	writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t));
	writeUint8(id);
	writeUint32(value);
}

void Writer::writeFloatField(uint8_t id, float value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeFloat(value);
}

void Writer::writeFloatFieldWithIndex(uint8_t id, float value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(float));
    writeUint8(id);
    writeUint8(index);
    writeFloat(value);
}

void Writer::writeDoubleField(uint8_t id, double value) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(double));
    writeUint8(id);
    writeDouble(value);
}

void Writer::writeDoubleFieldWithIndex(uint8_t id, double value, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(double));
    writeUint8(id);
    writeUint8(index);
    writeDouble(value);
}

void Writer::writeStringField(uint8_t id, const char *str) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    while (*str) {
        writeUint8(*str++);
    }
}

void Writer::writeStringFieldWithIndex(uint8_t id, const char *str, uint8_t index) {
    writeUint16(sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint8_t) + (uint16_t)strlen(str));
    writeUint8(id);
    writeUint8(index);
    while (*str) {
        writeUint8(*str++);
    }
}

void Writer::flushBits() {
    if (m_bitMask != 0) {
        writeUint8(m_bits);
        m_bits = 0;
        m_bitMask = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

Reader::Reader(uint8_t *buffer)
	: m_buffer(buffer)
	, m_offset(0)
	, m_dataOffset(0)
{
}

bool Reader::readFileHeaderAndMetaFields(Parameters &parameters, uint32_t &headerRemaining) {
	uint32_t magic1 = readUint32();
	uint32_t magic2 = readUint32();
	m_version = readUint16();

	if (!(magic1 == MAGIC1 && magic2 == MAGIC2 && (m_version == VERSION1 || m_version == VERSION2))) {
		return false;
	}

	if (m_version == VERSION1) {
		m_dataOffset = DLOG_VERSION1_HEADER_SIZE;

		readUint16(); // flags
		m_columns = readUint32();
		parameters.period = readFloat();
		parameters.duration = readFloat();
		parameters.startTime = readUint32();

		return true;
	} else {
		readUint16(); // No. of columns
			
		m_dataOffset = readUint32();

		if (DLOG_VERSION1_HEADER_SIZE < m_dataOffset) {
			headerRemaining = m_dataOffset - DLOG_VERSION1_HEADER_SIZE;
		} else {
			headerRemaining = 0;
		}
	}

	return true;
}

bool Reader::readRemainingFileHeaderAndMetaFields(Parameters &parameters) {
	bool invalidHeader = false;

	// defaults
	parameters.startTime = 0;
	parameters.finalDuration = 0;
	parameters.dataContainsSampleValidityBit = 0;

	while (!invalidHeader && m_offset < m_dataOffset) {
		uint16_t fieldLength = readUint16();
		if (fieldLength == 0) {
			break;
		}

		if (m_offset - sizeof(uint16_t) + fieldLength > m_dataOffset) {
			invalidHeader = true;
			break;
		}

		uint8_t fieldId = readUint8();

		uint16_t fieldDataLength = fieldLength - sizeof(uint16_t) - sizeof(uint8_t);

		if (fieldId == FIELD_ID_COMMENT) {
			if (fieldDataLength > MAX_COMMENT_LENGTH) {
				invalidHeader = true;
				break;
			}
			for (int i = 0; i < fieldDataLength; i++) {
				parameters.comment[i] = readUint8();
			}
			parameters.comment[MAX_COMMENT_LENGTH] = 0;
		}
		else if (fieldId == FIELD_ID_START_TIME) {
			parameters.startTime = readUint32();
		}
		else if (fieldId == FIELD_ID_DURATION) {
			parameters.finalDuration = readDouble();
		}
		else if (fieldId == FIELD_ID_DATA_CONTAINS_SAMPLE_VALIDITY_BIT) {
			parameters.dataContainsSampleValidityBit = readUint8();
		}
		else if (fieldId == FIELD_ID_X_UNIT) {
			parameters.xAxis.unit = (Unit)readUint8();
		}
		else if (fieldId == FIELD_ID_X_STEP) {
			parameters.xAxis.step = readFloat();
		}
		else if (fieldId == FIELD_ID_X_SCALE_TYPE) {
			parameters.xAxis.scaleType = (ScaleType)readUint8();
		}
		else if (fieldId == FIELD_ID_X_RANGE_MIN) {
			parameters.xAxis.range.min = readFloat();
		}
		else if (fieldId == FIELD_ID_X_RANGE_MAX) {
			parameters.xAxis.range.max = readFloat();
		}
		else if (fieldId == FIELD_ID_X_LABEL) {
			if (fieldDataLength > MAX_LABEL_LENGTH) {
				invalidHeader = true;
				break;
			}
			for (int i = 0; i < fieldDataLength; i++) {
				parameters.xAxis.label[i] = readUint8();
			}
			parameters.xAxis.label[MAX_LABEL_LENGTH] = 0;
		}
		else if (fieldId >= FIELD_ID_Y_UNIT && fieldId < FIELD_ID_Y_LAST_FIELD) {
			int8_t yAxisIndex = (int8_t)readUint8();
			if (yAxisIndex > MAX_NUM_OF_Y_AXES) {
				invalidHeader = true;
				break;
			}

			fieldDataLength -= sizeof(uint8_t);

			yAxisIndex--;
			if (yAxisIndex >= parameters.numYAxes) {
				parameters.numYAxes = yAxisIndex + 1;
				parameters.initYAxis(yAxisIndex);
			}

			YAxis &destYAxis = yAxisIndex >= 0 ? parameters.yAxes[yAxisIndex] : parameters.yAxis;

			if (fieldId == FIELD_ID_Y_UNIT) {
				destYAxis.unit = (Unit)readUint8();
			}
			else if (fieldId == FIELD_ID_Y_DATA_TYPE) {
				destYAxis.dataType = (DataType)readUint8();
			}
			else if (fieldId == FIELD_ID_Y_RANGE_MIN) {
				destYAxis.range.min = readFloat();
			}
			else if (fieldId == FIELD_ID_Y_RANGE_MAX) {
				destYAxis.range.max = readFloat();
			}
			else if (fieldId == FIELD_ID_Y_LABEL) {
				if (fieldDataLength > MAX_LABEL_LENGTH) {
					invalidHeader = true;
					break;
				}
				for (int i = 0; i < fieldDataLength; i++) {
					destYAxis.label[i] = readUint8();
				}
				destYAxis.label[MAX_LABEL_LENGTH] = 0;
			}
			else if (fieldId == FIELD_ID_Y_CHANNEL_INDEX) {
				destYAxis.channelIndex = (int16_t)(readUint8()) - 1;
			}
			else if (fieldId == FIELD_ID_Y_TRANSFORM_OFFSET) {
				destYAxis.transformOffset = readDouble();
			}
			else if (fieldId == FIELD_ID_Y_TRANSFORM_SCALE) {
				destYAxis.transformScale = readDouble();
			}
			else {
				// unknown field, skip
				m_offset += fieldDataLength;
			}
		}
		else if (fieldId == FIELD_ID_Y_SCALE_TYPE) {
			parameters.yAxisScaleType = (ScaleType)readUint8();
		}
		else if (fieldId == FIELD_ID_CHANNEL_MODULE_TYPE) {
			uint8_t channelIndex = readUint8(); // channel index
			uint16_t moduleType = readUint16(); // module type
			if (channelIndex < MAX_NUM_OF_CHANNELS) {
				parameters.channels[channelIndex].moduleType = moduleType;
			}			
		}
		else if (fieldId == FIELD_ID_CHANNEL_MODULE_REVISION) {
			uint8_t channelIndex = readUint8(); // channel index
			uint16_t moduleRevision = readUint16(); // module revision
			if (channelIndex < MAX_NUM_OF_CHANNELS) {
				parameters.channels[channelIndex].moduleRevision = moduleRevision;
			}			
		}
		else if (fieldId == FILE_ID_TEXT_INDEX_FILE_OFFSET) {
			parameters.textIndexFileOffset = readUint32();
		} 
		else if (fieldId == FILE_ID_TEXT_FILE_OFFSET) {
			parameters.textFileOffset = readUint32();
		} 
		else {
			// unknown field, skip
			m_offset += fieldDataLength;
		}
	}

	parameters.period = parameters.xAxis.step;
	parameters.duration = parameters.xAxis.range.max - parameters.xAxis.range.min;

	return !invalidHeader;
}

uint8_t Reader::readUint8() {
	uint32_t i = m_offset;
	m_offset += 1;
	return m_buffer[i];
}

uint16_t Reader::readUint16() {
	uint32_t i = m_offset;
	m_offset += 2;
	return (m_buffer[i + 1] << 8) | m_buffer[i];
}

uint32_t Reader::readUint32() {
	uint32_t i = m_offset;
	m_offset += 4;
	return (m_buffer[i + 3] << 24) | (m_buffer[i + 2] << 16) | (m_buffer[i + 1] << 8) | m_buffer[i];
}

float Reader::readFloat() {
	uint32_t value = readUint32();
	return *((float *)&value);
}

double Reader::readDouble() {
	double result;
	uint8_t *p = (uint8_t *)&result;
	uint8_t *q = m_buffer + m_offset;
	m_offset += 8;

	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p++ = *q++;
	*p = *q;

	return result;
}

} // namespace dlog_file
} // namespace eez
