#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include <string.h>
#include <stdio.h>

#define QUEUE_SIZE         (uint32_t) 10
#define READ_CPLT_MSG      (uint32_t) 1
#define WRITE_CPLT_MSG     (uint32_t) 2

#define SD_TIMEOUT 30 * 1000

#define SD_DEFAULT_BLOCK_SIZE 512

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize (BYTE);
DSTATUS SD_status (BYTE);
DRESULT SD_read (BYTE, BYTE*, DWORD, UINT);
DRESULT SD_write (BYTE, const BYTE*, DWORD, UINT);
DRESULT SD_ioctl (BYTE, BYTE, void*);

const Diskio_drvTypeDef  SD_Driver = {
	SD_initialize,
	SD_status,
	SD_read,
	SD_write,
	SD_ioctl,
};

static int SD_CheckStatusWithTimeout(uint32_t timeout) {
	uint32_t timer;

	/* block until SDIO peripheral is ready again or a timeout occur */
	timer = osKernelSysTick();
	while( osKernelSysTick() - timer < timeout) {
		if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
			return 0;
		}
	}

	return -1;
}

static DSTATUS SD_CheckStatus(BYTE lun) {
	Stat = STA_NOINIT;

	if(BSP_SD_GetCardState() == SD_TRANSFER_OK) {
		Stat &= ~STA_NOINIT;
	}

	return Stat;
}

/**
  * @brief  Initializes a Drive
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_initialize(BYTE lun) {
	Stat = STA_NOINIT;

	/*
	* check that the kernel has been started before continuing
	* as the osMessage API will fail otherwise
	*/
	if(osKernelRunning()) {
		if(BSP_SD_Init() == MSD_OK) {
			Stat = SD_CheckStatus(lun);
		}
	}

	return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  lun : not used
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_status(BYTE lun) {
	return SD_CheckStatus(lun);
}

/**
  * @brief  Reads Sector(s)
  * @param  lun : not used
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count) {
	DRESULT res = RES_ERROR;
	uint32_t timer;

	/*
	* ensure the SDCard is ready for a new operation
	*/
	if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0) {
		return res;
	}

    /* Fast path cause destination buffer is correctly aligned */
    uint8_t ret = BSP_SD_ReadBlocks((uint32_t*)buff, (uint32_t)(sector), count, SD_TIMEOUT);
    if (ret == MSD_OK) {
        timer = osKernelSysTick();
        /* block until SDIO IP is ready or a timeout occur */
		while (osKernelSysTick() - timer < SD_TIMEOUT) {
			if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
				res = RES_OK;
				break;
			}
		}
    }

    return res;
}

/**
  * @brief  Writes Sector(s)
  * @param  lun : not used
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count) {
	DRESULT res = RES_ERROR;
	uint32_t timer;

	/*
	* ensure the SDCard is ready for a new operation
	*/
	if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0) {
		return res;
	}

	if (BSP_SD_WriteBlocks((uint32_t*)buff, (uint32_t) (sector), count, SD_TIMEOUT) == MSD_OK) {
		timer = osKernelSysTick();
		/* block until SDIO IP is ready or a timeout occur */
		while (osKernelSysTick() - timer < SD_TIMEOUT) {
			if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
				res = RES_OK;
				break;
			}
		}
	}

	return res;
}

/**
  * @brief  I/O control operation
  * @param  lun : not used
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff) {
	DRESULT res = RES_ERROR;
	BSP_SD_CardInfo CardInfo;

	if (Stat & STA_NOINIT) {
		return RES_NOTRDY;
	}

	switch (cmd) {
	/* Make sure that no pending write process */
	case CTRL_SYNC :
		res = RES_OK;
		break;

	/* Get number of sectors on the disk (DWORD) */
	case GET_SECTOR_COUNT :
		BSP_SD_GetCardInfo(&CardInfo);
		*(DWORD*)buff = CardInfo.LogBlockNbr;
		res = RES_OK;
		break;

	/* Get R/W sector size (WORD) */
	case GET_SECTOR_SIZE :
		BSP_SD_GetCardInfo(&CardInfo);
		*(WORD*)buff = CardInfo.LogBlockSize;
		res = RES_OK;
		break;

	/* Get erase block size in unit of sector (DWORD) */
	case GET_BLOCK_SIZE :
		BSP_SD_GetCardInfo(&CardInfo);
		*(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
		res = RES_OK;
		break;

	default:
		res = RES_PARERR;
	}

	return res;
}
