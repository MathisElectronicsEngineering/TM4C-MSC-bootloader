/*
 * Copyright (c) 2012 Andrzej Surowiec <emeryth@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <stdint.h>
#include <stdbool.h>

#include "ramdisk.h"
#include "boot_usb_msc.h"
#include "common.h"

#include "inc/hw_flash.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/flash.h"
#include "usblib/usblib.h"
#include "usblib/device/usbdevice.h"

#ifdef DEBUGUART
#include "utils/uartstdio.h"
#endif

#define WBVAL(x) ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define QBVAL(x) ((x) & 0xFF), (((x) >> 8) & 0xFF), (((x) >> 16) & 0xFF), (((x) >> 24) & 0xFF)

#define BLOCK_SIZE 512
#define BYTES_PER_SECTOR 512
#define SECTORS_PER_CLUSTER 4
#define RESERVED_SECTORS 1
#define FAT_COPIES 2
#define ROOT_ENTRIES 512
#define ROOT_ENTRY_LENGTH 32
#define FIRMWARE_BIN_CLUSTER 3
#define DATA_REGION_SECTOR (RESERVED_SECTORS + FAT_COPIES + (ROOT_ENTRIES * ROOT_ENTRY_LENGTH) / BYTES_PER_SECTOR)
#define FIRMWARE_START_SECTOR (DATA_REGION_SECTOR + (firmware_start_cluster - 2) * SECTORS_PER_CLUSTER)

int massStorageDrive = 0;
bool newFirmwareStartSet = false;
unsigned long firmware_start_cluster = FIRMWARE_BIN_CLUSTER;

unsigned char bootSector[] = {
	0xeb, 0x3c, 0x90,                                      // Code to jump to the bootstrap code
	'm', 'k', 'd', 'o', 's', 'f', 's', 0x00,               // OEM ID
	WBVAL(BYTES_PER_SECTOR),                               // Bytes per sector (512)
	SECTORS_PER_CLUSTER,                                   // Sectors per cluster (4)
	WBVAL(RESERVED_SECTORS),                               // Reserved sectors (1)
	FAT_COPIES,                                            // Number of FAT copies (2)
	WBVAL(ROOT_ENTRIES),                                   // Number of possible root entries (512)
	0x00, 0x04,                                            // Small number of sectors (1024)
	0xf8,                                                  // Media descriptor (0xf8 - Fixed disk)
	0x01, 0x00,                                            // Sectors per FAT (1)
	0x20, 0x00,                                            // Sectors per track (32)
	0x40, 0x00,                                            // Number of heads (64)
	0x00, 0x00, 0x00, 0x00,                                // Hidden sectors (0)
	0x00, 0x00, 0x00, 0x00,                                // Large number of sectors (0)
	0x00,                                                  // Drive number (0)
	0x00,                                                  // Reserved
	0x29,                                                  // Extended boot signature
	0x69, 0x17, 0xad, 0x53,                                // Volume serial number
	'F', 'I', 'R', 'M', 'W', 'A', 'R', 'E', ' ', ' ', ' ', // Volume label
	'F', 'A', 'T', '1', '2', ' ', ' ', ' ',                // Filesystem type

	// The last two bytes are just set in 'massStorageRead'
	/*
	0x00, 0x00, // OS boot code

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x55, 0xAA
    */
};

unsigned char fatTable[] = {
    0xF8, 0xFF, 0xFF, 0x03, 0x40, 0x00, 0x05, 0x60, 0x00, 0x07, 0x80, 0x00, 0x09, 0xF0, 0xFF, 0x0B,
    0xC0, 0x00, 0x0D, 0xE0, 0x00, 0x0F, 0x00, 0x01, 0x11, 0x20, 0x01, 0x13, 0x40, 0x01, 0x15, 0x60,
    0x01, 0x17, 0x80, 0x01, 0x19, 0xA0, 0x01, 0x1B, 0xC0, 0x01, 0x1D, 0xE0, 0x01, 0x1F, 0x00, 0x02,
    0x21, 0x20, 0x02, 0x23, 0x40, 0x02, 0x25, 0x60, 0x02, 0x27, 0x80, 0x02, 0x29, 0xA0, 0x02, 0x2B,
    0xC0, 0x02, 0x2D, 0xE0, 0x02, 0x2F, 0x00, 0x03, 0x31, 0x20, 0x03, 0x33, 0x40, 0x03, 0x35, 0x60,
    0x03, 0x37, 0x80, 0x03, 0x39, 0xA0, 0x03, 0x3B, 0xC0, 0x03, 0x3D, 0xE0, 0x03, 0x3F, 0x00, 0x04,
    0x41, 0x20, 0x04, 0x43, 0x40, 0x04, 0x45, 0x60, 0x04, 0x47, 0x80, 0x04, 0x49, 0xA0, 0x04, 0x4B,
    0xC0, 0x04, 0x4D, 0xE0, 0x04, 0x4F, 0x00, 0x05, 0x51, 0x20, 0x05, 0x53, 0x40, 0x05, 0x55, 0x60,
    0x05, 0x57, 0x80, 0x05, 0x59, 0xA0, 0x05, 0x5B, 0xC0, 0x05, 0x5D, 0xE0, 0x05, 0x5F, 0x00, 0x06,
    0x61, 0x20, 0x06, 0x63, 0x40, 0x06, 0x65, 0x60, 0x06, 0x67, 0x80, 0x06, 0x69, 0xA0, 0x06, 0x6B,
    0xC0, 0x06, 0x6D, 0xE0, 0x06, 0x6F, 0x00, 0x07, 0x71, 0x20, 0x07, 0x73, 0x40, 0x07, 0x75, 0x60,
    0x07, 0x77, 0x80, 0x07, 0x79, 0xA0, 0x07, 0x7B, 0xC0, 0x07, 0x7D, 0xE0, 0x07, 0x7F, 0x00, 0x08,
    0x81, 0x20, 0x08, 0x83, 0x40, 0x08, 0x85, 0x60, 0x08, 0x87, 0x80, 0x08, 0x89, 0xF0, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define FIRMWARE_DATE_TIME (((2017 - 1980) << 25) /* Year since 1980 */ | \
                            (5 << 21)             /* Month (1-12) */    | \
                            (14 << 16)            /* Day (1-31) */      | \
                            (23 << 11)            /* Hour (0-23) */     | \
                            (15 << 5)             /* Minute (0-59) */   | \
                            (0 >> 1))             /* Seconds (Divided by 2) */

enum attributes_e {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE = 0x20,
    ATTR_LONG_NAME = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID,
};

unsigned char dirEntry[] = {
	// A long filename entry for firmware.bin
	0x41,                                                             // Sequence number (LAST_LONG_ENTRY (0x40) | N)
	'f', 0x00, 'i', 0x00, 'r', 0x00, 'm', 0x00, 'w', 0x00,            // Five name characters in UTF-16
	ATTR_LONG_NAME,                                                   // Attribute
	0x00,                                                             // Type
#ifdef CRYPTO
	0x14,                                                             // Checksum of DOS filename
	'a', 0x00, 'r', 0x00, 'e', 0x00, '.', 0x00, 's', 0x00, 'i', 0x00, // Six name characters in UTF-16
	0x00, 0x00,                                                       // First cluster
	'g', 0x00, 0x00, 0x00,                                            // Two name characters in UTF-16
#else
	0x57,                                                             // Checksum of DOS filename
	'a', 0x00, 'r', 0x00, 'e', 0x00, '.', 0x00, 'b', 0x00, 'i', 0x00, // Six name characters in UTF-16
	0x00, 0x00,                                                       // First cluster
	'n', 0x00, 0x00, 0x00,                                            // Two name characters in UTF-16
#endif
	// 8.3 entry
	'F', 'I', 'R', 'M', 'W', 'A', 'R', 'E', // Filename
#ifdef CRYPTO
	'S', 'I', 'G',                          // Extension
#else
	'B', 'I', 'N',                          // Extension
#endif
	ATTR_ARCHIVE,                           // Attribute byte
	0x00,                                   // Reserved for Windows NT
	0x00,                                   // Creation millisecond
	QBVAL(FIRMWARE_DATE_TIME),              // Creation date and time
	WBVAL(FIRMWARE_DATE_TIME >> 16),        // Last access date
	0x00, 0x00,                             // Reserved for FAT32
	QBVAL(FIRMWARE_DATE_TIME),              // Creation date and time
	WBVAL(FIRMWARE_BIN_CLUSTER),            // Starting cluster
	QBVAL(UPLOAD_LENGTH)                    // File size in bytes
};

void *massStorageOpen(unsigned long drive)
{
	return ((void *)&massStorageDrive);
}

void massStorageClose(void *drive)
{
#ifdef DEBUGUART
    UARTprintf("massStorageClose\n");
#endif
    USBDCDTerm(0); // Terminate the USB connection
    CallUserProgram();
}

unsigned long massStorageRead(void *drive, unsigned char *data, unsigned long blockNumber, unsigned long numberOfBlocks)
{
#if defined(DEBUGUART) && 0
	UARTprintf("Reading %d block(s) starting at %d\n", numberOfBlocks, blockNumber);
#endif
	for (int i = 0; i < BLOCK_SIZE; i++) {
		data[i] = 0;
	}
	if (blockNumber == 0) {
		for (int i = 0; i < sizeof(bootSector); i++) {
			data[i] = bootSector[i];
		}
		// The boot sector signature AA55h at the end
		data[510] = 0x55;
		data[511] = 0xAA;
	}
	else if (blockNumber == 1 || blockNumber == 2) {
		for (int i = 0; i < sizeof(fatTable); i++) {
			data[i] = fatTable[i];
		}
	}
	else if (blockNumber == 3) {
		for (int i = 0; i < sizeof(dirEntry); i++) {
			data[i] = dirEntry[i];
		}
	}
	else if (blockNumber >= FIRMWARE_START_SECTOR && blockNumber < FIRMWARE_START_SECTOR + UPLOAD_LENGTH / BLOCK_SIZE) {
#ifdef NOREAD
		unsigned char dummy[16] = "READ DISABLED  \n";
		for (int i = 0; i < BLOCK_SIZE; i++) {
			data[i] = dummy[i % 16];
		}
#else
		for (int i = 0; i < BLOCK_SIZE; i++) {
			data[i] = ((unsigned char *)(UPLOAD_START + (blockNumber - FIRMWARE_START_SECTOR) * BLOCK_SIZE))[i];
		}
#endif
	}
	return BLOCK_SIZE;
}

// Inspired by: https://github.com/opentx/opentx/blob/eb7c73668f55026c57b880027acc77f1bd2ee00a/radio/src/targets/taranis/flash_driver.cpp
// Please report back if this header does not match your binary file
static bool isFirmwareStart(const uint8_t *buffer) {
    const uint32_t *block = (const uint32_t*)buffer;
    if ((block[0] & 0xFFFC0000) != 0x20000000)
        return false;
    if ((block[1] & 0xFFFF000F) != 0x9)
        return false;
    if ((block[2] & 0xFFFF000F) != 0x1)
        return false;
    if ((block[3] & 0xFFFF000F) != 0x3)
        return false;
    return true;
}

unsigned long massStorageWrite(void *drive, unsigned char *data, unsigned long blockNumber, unsigned long numberOfBlocks)
{
#if defined(DEBUGUART) && 0
	UARTprintf("Writing %d block(s) starting at %d\n", numberOfBlocks, blockNumber);
	UARTprintf("Firmware start cluster: %d\n", firmware_start_cluster);
	for (int j = 0; j < BLOCK_SIZE * numberOfBlocks; j += 16) {
		for (int i = 0; i < 16; i++) {
			UARTprintf("%02x ",data[j+i]);
		}
		UARTprintf("\n");
	}
#endif
	if (blockNumber == 0) {
		for (int i = 0; i < sizeof(bootSector); i++) {
			bootSector[i] = data[i];
		}
	}
	else if (blockNumber == 1 || blockNumber == 2) {
		for (int i = 0; i < sizeof(fatTable); i++) {
			fatTable[i] = data[i];
		}
	}
	else if (blockNumber == 3) {
		for (int i = 0; i < sizeof(dirEntry); i++) {
			dirEntry[i] = data[i];
		}
	}
	// The bootloader is 16 kB i.e. 32 blocks of 512 bytes
	else if (blockNumber >= FIRMWARE_START_SECTOR) {
		if (isFirmwareStart(data)) { // TODO: Reset flag after when the firmware has been read
			// the host tried to write actual data to the data region, we assume this is the new firmware
			newFirmwareStartSet = true;
			firmware_start_cluster = (blockNumber - DATA_REGION_SECTOR) / SECTORS_PER_CLUSTER + 2;
#ifdef DEBUGUART
            UARTprintf("New firmware start\n");
#endif
		}
		// New firmware is being uploaded
		if (newFirmwareStartSet && blockNumber < FIRMWARE_START_SECTOR + UPLOAD_LENGTH / BLOCK_SIZE) {
			unsigned long address = (blockNumber - FIRMWARE_START_SECTOR) * BLOCK_SIZE + UPLOAD_START;
			// Erase
			if (blockNumber == FIRMWARE_START_SECTOR) {
				for (int counter = 0; counter < UPLOAD_LENGTH / FLASH_ERASE_SIZE; counter++) {
					FlashErase(UPLOAD_START + counter * FLASH_ERASE_SIZE);
				}
			}
#ifdef DEBUGUART
            UARTprintf("Writing to flash at: %u\n", blockNumber);
#endif
			FlashProgram((unsigned long *)data, address, BLOCK_SIZE * numberOfBlocks);
			return BLOCK_SIZE * numberOfBlocks;
		}
	}
	return BLOCK_SIZE * numberOfBlocks;
}

unsigned long massStorageNumBlocks(void *drive)
{
	// Filesystem size is 512 kB
	return 1024; // (2^9*1024)/BLOCK_SIZE
}
