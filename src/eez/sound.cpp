/*
 * EEZ PSU Firmware
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

#include <math.h>

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)

#include <cmath>
#include <queue>
#include <stdio.h>
#include <SDL.h>
#include <SDL_audio.h>

#elif defined(EEZ_PLATFORM_STM32)

#include <math.h>
#include <dac.h>
#include <tim.h>

#endif


#include <eez/sound.h>

#include <eez/system.h>

// TODO
#include <eez/apps/psu/psu.h>

#include <eez/apps/psu/persist_conf.h>

#define NOTE_B0 31
#define NOTE_C1 33
#define NOTE_CS1 35
#define NOTE_D1 37
#define NOTE_DS1 39
#define NOTE_E1 41
#define NOTE_F1 44
#define NOTE_FS1 46
#define NOTE_G1 49
#define NOTE_GS1 52
#define NOTE_A1 55
#define NOTE_AS1 58
#define NOTE_B1 62
#define NOTE_C2 65
#define NOTE_CS2 69
#define NOTE_D2 73
#define NOTE_DS2 78
#define NOTE_E2 82
#define NOTE_F2 87
#define NOTE_FS2 93
#define NOTE_G2 98
#define NOTE_GS2 104
#define NOTE_A2 110
#define NOTE_AS2 117
#define NOTE_B2 123
#define NOTE_C3 131
#define NOTE_CS3 139
#define NOTE_D3 147
#define NOTE_DS3 156
#define NOTE_E3 165
#define NOTE_F3 175
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 208
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 247
#define NOTE_C4 262
#define NOTE_CS4 277
#define NOTE_D4 294
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_CS5 554
#define NOTE_D5 587
#define NOTE_DS5 622
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_FS5 740
#define NOTE_G5 784
#define NOTE_GS5 831
#define NOTE_A5 880
#define NOTE_AS5 932
#define NOTE_B5 988
#define NOTE_C6 1047
#define NOTE_CS6 1109
#define NOTE_D6 1175
#define NOTE_DS6 1245
#define NOTE_E6 1319
#define NOTE_F6 1397
#define NOTE_FS6 1480
#define NOTE_G6 1568
#define NOTE_GS6 1661
#define NOTE_A6 1760
#define NOTE_AS6 1865
#define NOTE_B6 1976
#define NOTE_C7 2093
#define NOTE_CS7 2217
#define NOTE_D7 2349
#define NOTE_DS7 2489
#define NOTE_E7 2637
#define NOTE_F7 2794
#define NOTE_FS7 2960
#define NOTE_G7 3136
#define NOTE_GS7 3322
#define NOTE_A7 3520
#define NOTE_AS7 3729
#define NOTE_B7 3951
#define NOTE_C8 4186
#define NOTE_CS8 4435
#define NOTE_D8 4699
#define NOTE_DS8 4978

#define DURATION_BETWEEN_NOTES_FACTOR 1.3

namespace eez {
namespace sound {

////////////////////////////////////////////////////////////////////////////////

enum {
	CLICK_TUNE,
	BEEP_TUNE,
	POWER_UP_TUNE,
	POWER_DOWN_TUNE
};

int clickTune[] = {
	632, 16,
	-1 // end!
};

int beepTune[] = {
	NOTE_C6, 4,
	-1 // end!
};

int powerUpTune[] = {
    NOTE_C6, 8,
    NOTE_C6, 2,
    -1 // end!
};

int powerDownTune[] = {
    NOTE_C6, 10, 
    NOTE_A5, 10, 
    NOTE_G5, 5,
    -1 // end!
};

struct Tune {
	int *tune;
	float durationBetweenNotesFactor;
	uint16_t *pSamples;
	unsigned int numSamples;
};

Tune g_tunes[4] = {
	{ clickTune, 1.3f },
	{ beepTune, 1.3f },
	{ powerUpTune, 1.3f },
	{ powerDownTune, 0.75f },
};

static int g_iNextTuneToPlay = -1;

static PlayPowerUpCondition g_playPowerUpCondition;

////////////////////////////////////////////////////////////////////////////////

#define PI 3.14159265f

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)

#define SAMPLE_RATE 44100
uint16_t g_memoryForTuneSamples[125000];

SDL_AudioDeviceID g_dev;

#else

#define SAMPLE_RATE 10000
uint16_t g_memoryForTuneSamples[25000]; // TODO: move this to SDRAM

#endif

unsigned int generateTuneSamples(int *tune, int sampleRate, uint16_t *pMemBuffer, float durationBetweenNotesFactor) {
	unsigned int size = 0;
	for (int i = 0; tune[i] != -1; i += 2) {
		if (i > 0) {
			unsigned int silence = (unsigned int)(durationBetweenNotesFactor * sampleRate / tune[i - 1]);
			size += silence;
			while (silence--) {
				*pMemBuffer++ = 0;
			}
		}
		unsigned int duration = sampleRate / tune[i + 1];
		size += duration;
		float f = 2 * PI * tune[i] / sampleRate;
		for (unsigned int k = 0; k < duration; k++) {
#if defined(EEZ_PLATFORM_SIMULATOR)
			*pMemBuffer++ = (uint16_t)roundf(30000 * sinf(k * f));
#elif defined(EEZ_PLATFORM_STM32)
			*pMemBuffer++ = (uint16_t)roundf(2047.5f + 2047.5f * sinf(k * f));
#endif
		}
	}
	return size;
}


////////////////////////////////////////////////////////////////////////////////

void init() {
	for (unsigned int i = 0; i < sizeof(g_tunes) / sizeof(Tune); i++) {
		g_tunes[i].pSamples = i > 0 ? g_tunes[i - 1].pSamples + g_tunes[i - 1].numSamples : g_memoryForTuneSamples;
		g_tunes[i].numSamples = generateTuneSamples(g_tunes[i].tune, SAMPLE_RATE, g_tunes[i].pSamples, g_tunes[i].durationBetweenNotesFactor);
	}

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)
	SDL_InitSubSystem(SDL_INIT_AUDIO);

	SDL_AudioSpec desiredSpec;

	SDL_memset(&desiredSpec, 0, sizeof(desiredSpec));

	desiredSpec.freq = SAMPLE_RATE;
	desiredSpec.format = AUDIO_S16SYS;
	desiredSpec.channels = 1;
	desiredSpec.samples = 2048;

	SDL_AudioSpec obtainedSpec;

	g_dev = SDL_OpenAudioDevice(NULL, 0, &desiredSpec, &obtainedSpec, 0);
	if (g_dev == 0) {
		printf("Failed to open audio: %s\n", SDL_GetError());
	}
#elif defined(EEZ_PLATFORM_STM32)
	HAL_TIM_Base_Start(&htim6);
#endif
}

void tick() {
	if (g_iNextTuneToPlay == -1) {
		return;
	}

#if defined(EEZ_PLATFORM_SIMULATOR) && !defined(__EMSCRIPTEN__)

	if (g_dev != 0) {
		SDL_QueueAudio(g_dev, g_tunes[g_iNextTuneToPlay].pSamples, g_tunes[g_iNextTuneToPlay].numSamples * 2);
		SDL_PauseAudioDevice(g_dev, 0);
	}

#elif defined(EEZ_PLATFORM_STM32)

	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)g_tunes[g_iNextTuneToPlay].pSamples, g_tunes[g_iNextTuneToPlay].numSamples, DAC_ALIGN_12B_R);

#endif

	g_iNextTuneToPlay = -1;
}

static void playTune(int iTune) {
	if (iTune > g_iNextTuneToPlay) {
		g_iNextTuneToPlay = iTune;
	}
}

void playPowerUp(PlayPowerUpCondition condition) {
	if (condition == PLAY_POWER_UP_CONDITION_NONE) {
		g_playPowerUpCondition = PLAY_POWER_UP_CONDITION_NONE;
	} else if (g_playPowerUpCondition == PLAY_POWER_UP_CONDITION_NONE) {
		g_playPowerUpCondition = condition;
	} else if (
		(condition == PLAY_POWER_UP_CONDITION_WELCOME_PAGE_IS_ACTIVE && g_playPowerUpCondition == PLAY_POWER_UP_CONDITION_TEST_SUCCESSFUL) ||
		(condition == PLAY_POWER_UP_CONDITION_TEST_SUCCESSFUL && g_playPowerUpCondition == PLAY_POWER_UP_CONDITION_WELCOME_PAGE_IS_ACTIVE)
	) {
		g_playPowerUpCondition = PLAY_POWER_UP_CONDITION_NONE;
    	if (psu::persist_conf::isSoundEnabled()) {
			playTune(POWER_UP_TUNE);
    	}
	}
}

void playPowerDown() {
    if (psu::persist_conf::isSoundEnabled()) {
		playTune(POWER_DOWN_TUNE);
    }
}

void playBeep(bool force) {
    if (force || psu::persist_conf::isSoundEnabled()) {
		playTune(BEEP_TUNE);
    }
}

void playClick() {
    if (psu::persist_conf::isClickSoundEnabled()) {
		playTune(CLICK_TUNE);
    }
}

} // namespace sound
} // namespace eez
