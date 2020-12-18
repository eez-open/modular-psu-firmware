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

#pragma once

/* DLOG File Format V1

OFFSET    TYPE    WIDTH    DESCRIPTION
----------------------------------------------------------------------
0               U32     4        MAGIC1 = 0x2D5A4545L

4               U32     4        MAGIC2 = 0x474F4C44L

8               U16     2        VERSION = 0x0001L

10              U16     2        Flags: RRRR RRRR RRRR RRRJ
                                    J - Jitter enabled
                                    R - Reserverd

12              U32     4        Columns: RRRR RRRR RPIU RPIU RPIU RPIU RPIU RPIU
                                                     Ch6  Ch5  Ch4  Ch3  Ch2  Ch1
                                    U - voltage
                                    I - current
                                    P - power
                                    R - reserved

16              Float   4        Period

20              Float   4        Duration

24              U32     4        Start time, timestamp

28+(n*N+m)*4    Float   4        n-th row and m-th column value, N - number of columns
*/

#include <stdint.h>

#include "./unit.h"

namespace eez {
namespace dlog_file {

static const uint16_t VERSION1 = 1;
static const uint16_t VERSION2 = 2;
static const uint32_t DLOG_VERSION1_HEADER_SIZE = 28;

static const int MAX_COMMENT_LENGTH = 128;
static const int MAX_NUM_OF_Y_AXES = 18;
static const int MAX_NUM_OF_CHANNELS = 6;

enum Fields {
    FIELD_ID_COMMENT = 1,

    FIELD_ID_X_UNIT = 10,
    FIELD_ID_X_STEP = 11,
    FIELD_ID_X_RANGE_MIN = 12,
    FIELD_ID_X_RANGE_MAX = 13,
    FIELD_ID_X_LABEL = 14,
    FIELD_ID_X_SCALE = 15, // 0 - linear, 1 - logarithmic

    FIELD_ID_Y_UNIT = 30,
    FIELD_ID_Y_RANGE_MIN = 32,
    FIELD_ID_Y_RANGE_MAX = 33,
    FIELD_ID_Y_LABEL = 34,
    FIELD_ID_Y_CHANNEL_INDEX = 35,
    FIELD_ID_Y_SCALE = 36,

    FIELD_ID_CHANNEL_MODULE_TYPE = 50,
    FIELD_ID_CHANNEL_MODULE_REVISION = 51
};

struct Range {
    float min;
    float max;
};

static const int MAX_LABEL_LENGTH = 32;

enum Scale {
    SCALE_LINEAR,
    SCALE_LOGARITHMIC
};

struct XAxis {
    Unit unit;
    float step;
    Scale scale;
    Range range;
	char label[MAX_LABEL_LENGTH + 1] = { 0 };
};

struct YAxis {
    Unit unit;
    Range range;
	char label[MAX_LABEL_LENGTH + 1] = { 0 };
    int8_t channelIndex;
};

struct ChannelInfo {
    uint16_t moduleType;
    uint16_t moduleRevision;
};

struct Parameters {
    char comment[MAX_COMMENT_LENGTH + 1];

    XAxis xAxis;

    YAxis yAxis;
    Scale yAxisScale;

    uint8_t numYAxes;
    YAxis yAxes[MAX_NUM_OF_Y_AXES];

    ChannelInfo channels[MAX_NUM_OF_CHANNELS];

	float period;
	float duration;

	void initYAxis(int yAxisIndex);
};

struct Writer {
public:
    Writer(uint8_t *buffer, uint32_t bufferSize);

	void reset();

    void writeFileHeaderAndMetaFields(const Parameters &parameters);

	void writeFloat(float value);
	void writeBit(int bit);
	void flushBits();

	uint8_t *getBuffer() { return m_buffer; }
	uint32_t getBufferIndex() { return m_bufferIndex; }
	uint32_t getDataOffset() { return m_dataOffset; }
	uint32_t getFileLength() { return m_fileLength; }
    uint32_t getBitMask() { return m_bitMask; }

private:
    uint8_t *m_buffer;
    uint32_t m_bufferSize;
    uint32_t m_bufferIndex = 0;
    uint32_t m_fileLength = 0;
    uint32_t m_bitMask = 0;
    uint32_t m_bits = 0;
	uint32_t m_dataOffset = 0;

    void writeUint8(uint8_t value);
    void writeUint16(uint16_t value);
    void writeUint32(uint32_t value);
    void writeUint8Field(uint8_t id, uint8_t value);
    void writeUint8FieldWithIndex(uint8_t id, uint8_t value, uint8_t index);
    void writeUint16Field(uint8_t id, uint16_t value);
    void writeUint16FieldWithIndex(uint8_t id, uint16_t value, uint8_t index);
    void writeFloatField(uint8_t id, float value);
    void writeFloatFieldWithIndex(uint8_t id, float value, uint8_t index);
    void writeStringField(uint8_t id, const char *str);
    void writeStringFieldWithIndex(uint8_t id, const char *str, uint8_t index);
};

struct Reader {
public:
	Reader(uint8_t *buffer);

	bool readFileHeaderAndMetaFields(Parameters &parameters, uint32_t &headerRemaining);
	bool readRemainingFileHeaderAndMetaFields(Parameters &parameters); // call this only in case of VERSION2

	uint16_t getVersion() { return m_version; }
	uint32_t getDataOffset() { return m_dataOffset;  }

	uint32_t getColumns() { return m_columns; }

private:
	uint8_t *m_buffer;
	uint16_t m_version;
	uint32_t m_offset;
	uint32_t m_dataOffset;
	uint32_t m_columns;

	uint8_t readUint8();
	uint16_t readUint16();
	uint32_t readUint32();
	float readFloat();
};

} // namespace dlog_file
} // namespace eez
