/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 @file    flashSlave.c
 @brief   FSM for flashing slave firmaware
 ******************************************************************************
 @attention

 <h2><center>&copy; Copyright (c) 2019 Envox d.o.o.
 All rights reserved.</center></h2>

 This software component is licensed under BSD 3-Clause license,
 the "License"; You may not use this file except in compliance with the
 License. You may obtain a copy of the License at:
 opensource.org/licenses/BSD-3-Clause

 ******************************************************************************
 */
/* USER CODE END Header */
#include "flashSlave.h"

#include <memory.h>

uint8_t CMD_GET = 0x00;
uint8_t CMD_GET_VERSION = 0x01;
uint8_t CMD_ID = 0x02;
uint8_t CMD_READ_MEMORY = 0x11;
uint8_t CMD_GO = 0x21;
uint8_t CMD_WRITE_MEMORY = 0x31;
uint8_t CMD_ERASE = 0x43;
uint8_t CMD_EXTENDED_ERASE = 0x44;
uint8_t CMD_WRITE_PROTECT = 0x63;
uint8_t CMD_WRITE_UNPROTECT = 0x73;
uint8_t CMD_READOUT_PROTECT = 0x82;
uint8_t CMD_READOUT_UNPROTECT = 0x92;
uint8_t ENTER_BOOTLOADER = 0x7F;
uint8_t CRC_MASK = 0xFF;

UART_HandleTypeDef *phuart = &huart7;

uint8_t rxBuffer[200], rxData[2],rxIndex;
uint8_t rxReady = 0;
uint8_t gotACK = 0;
uint8_t gotNACK = 0;
uint8_t ACK = 0x79;
uint8_t NACK = 0x1F;

long maxTimeout = 1000;
long timeoutNextMax = 200;
long timeout, timeoutNext;
int flashStatus = STAND_BY;
uint8_t crc;

void sendDataAndCRC(uint8_t data) {
	uint8_t sendData[1];
	sendData[0] = data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
	sendData[0] = CRC_MASK ^ data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
}

void sendDataNoCRC(uint8_t data) {
	uint8_t sendData[1];
	sendData[0] = data;
	HAL_UART_Transmit(phuart, sendData, 1, 20);
}

void resetSlave(void) {
	// not implemented -- I do manual reset
	timeoutNext = HAL_GetTick();
	flashStatus = WAIT_FOR_NEXT_TRANSMIT;
}

uint8_t flashSlaveFSM() {
	switch (flashStatus) {
	case STAND_BY:
		// statements
		resetSlave();
		flashStatus = SEND_START;
		break;
	case WAIT_FOR_RESPONSE:
		if (HAL_UART_Receive(phuart, rxData, 1, 1)) {
			byteFromSlave();
		}
		if (rxReady) {
			if (gotACK) {
				flashStatus = GOT_ACK;
				rxReady = 0;
				gotACK = 0;
				gotNACK = 0;
			} else if (gotNACK) {
				flashStatus = GOT_NACK;
				rxReady = 0;
				gotACK = 0;
				gotNACK = 0;
			}
		}
		if (HAL_GetTick() - timeout > maxTimeout) {
			flashStatus = RESPONSE_TIMEOUT;
		}
		break;
	case SEND_START:
		sendDataNoCRC(ENTER_BOOTLOADER);
		timeout = HAL_GetTick();
		flashStatus = WAIT_FOR_RESPONSE;
		break;
	case GOT_ACK:
		flashStatus = DATA_READY;
		break;
	case GOT_NACK:
		flashStatus = DATA_READY;
		break;
	case RESPONSE_TIMEOUT:
		flashStatus = STAND_BY;
		break;
	case DATA_READY:
		timeout = HAL_GetTick();
		timeoutNext = HAL_GetTick();
		flashStatus = SEND_GET;
		break;
	case WAIT_FOR_NEXT_TRANSMIT:
		if ((HAL_GetTick() - timeoutNext) > timeoutNextMax) {
			flashStatus = SEND_GET;
		}
		break;
	case SEND_GET:
		sendDataAndCRC(CMD_ID);
		timeout = HAL_GetTick();
		timeoutNext = HAL_GetTick();
		flashStatus = WAIT_FOR_RESPONSE;
		break;
	default:
		flashStatus = STAND_BY;
	}
	return flashStatus;
}

void byteFromSlave(void) {
	// Clear Rx_Buffer before receiving new data
	if (rxIndex == 0) {
		memset(rxBuffer, '\0', 200);
	}
	// Clear Rx_Buffer before receiving new data
	if (rxIndex >= 200) {
		memset(rxBuffer, '\0', 200);
		rxIndex = 0;
	}
	rxBuffer[rxIndex] = rxData[0];
	rxIndex++;
	if (rxData[0] == ACK || rxData[0] == NACK) {
		rxReady = 1;
		if (rxData[0] == ACK) {
			gotACK = 1;
			rxIndex = 0;
		}
		if (rxData[0] == NACK) {
			gotNACK = 1;
			rxIndex = 0;
		}
	}
}
