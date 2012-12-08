/* 
* Copyright 2012 Andrzej Surowiec
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
* File:			ramdisk.c
* Author:		Andrzej Surowiec <emeryth@gmail.com>
* Version:		1.0
* Decription:	Mass storage filesystem driver
*
*/

#include "ramdisk.h"
#include <string.h>

#include "inc/hw_flash.h"
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"
#include "driverlib/flash.h"

#include "utils/uartstdio.h"

#define BLOCK_SIZE 512
#define FIRMWARE_START_SECTOR (27+firmware_start_cluster*4)
#define SECTORS_PER_CLUSTER 4

int massStorageDrive=0;
int newFirmwareStartSet=0;
unsigned long firmware_start_cluster=0x03;



#define BOOT_LEN 192
unsigned char bootSector[] = {
	0xeb, 0x3c, 0x90, 0x6d, 0x6b, 0x64, 0x6f, 0x73, 0x66, 0x73, 0x00, 0x00,
	0x02, 0x04, 0x01, 0x00, 0x02, 0x00, 0x02, 0x00, 0x04, 0xf8, 0x01, 0x00,
	0x20, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x29, 0x69, 0x17, 0xad, 0x53, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x32, 0x20,
	0x20, 0x20, 0x0e, 0x1f, 0xbe, 0x5b, 0x7c, 0xac, 0x22, 0xc0, 0x74, 0x0b,
	0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0x5e, 0xeb, 0xf0, 0x32,
	0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb, 0xfe, 0x54, 0x68, 0x69, 0x73, 0x20,
	0x69, 0x73, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f,
	0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2e, 0x20,
	0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x65,
	0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c,
	0x65, 0x20, 0x66, 0x6c, 0x6f, 0x70, 0x70, 0x79, 0x20, 0x61, 0x6e, 0x64,
	0x0d, 0x0a, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20,
	0x6b, 0x65, 0x79, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61,
	0x67, 0x61, 0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d, 0x0a, 0x00
};


#define FAT_LEN 256
unsigned char fat[] = {
	0xf8, 0xff, 0xff, 0x00, 0x40, 0x00, 0x05, 0x60, 0x00, 0x07, 0x80, 0x00,
	0x09, 0xa0, 0x00, 0x0b, 0xc0, 0x00, 0x0d, 0xe0, 0x00, 0x0f, 0x00, 0x01,
	0x11, 0x20, 0x01, 0x13, 0x40, 0x01, 0x15, 0x60, 0x01, 0x17, 0x80, 0x01,
	0x19, 0xa0, 0x01, 0x1b, 0xc0, 0x01, 0x1d, 0xe0, 0x01, 0x1f, 0x00, 0x02,
	0x21, 0x20, 0x02, 0x23, 0x40, 0x02, 0x25, 0x60, 0x02, 0x27, 0x80, 0x02,
	0x29, 0xa0, 0x02, 0x2b, 0xc0, 0x02, 0x2d, 0xe0, 0x02, 0x2f, 0x00, 0x03,
	0x31, 0x20, 0x03, 0x33, 0x40, 0x03, 0x35, 0x60, 0x03, 0x37, 0x80, 0x03,
	0x39, 0xa0, 0x03, 0x3b, 0xc0, 0x03, 0x3d, 0xe0, 0x03, 0x3f, 0x00, 0x04,
	0x41, 0x20, 0x04, 0x43, 0x40, 0x04, 0x45, 0x60, 0x04, 0x47, 0x80, 0x04,
	0x49, 0xa0, 0x04, 0x4b, 0xc0, 0x04, 0x4d, 0xe0, 0x04, 0x4f, 0x00, 0x05,
	0x51, 0x20, 0x05, 0x53, 0x40, 0x05, 0x55, 0x60, 0x05, 0x57, 0x80, 0x05,
	0x59, 0xa0, 0x05, 0x5b, 0xc0, 0x05, 0x5d, 0xe0, 0x05, 0x5f, 0x00, 0x06,
	0x61, 0x20, 0x06, 0x63, 0x40, 0x06, 0x65, 0x60, 0x06, 0x67, 0x80, 0x06,
	0x69, 0xa0, 0x06, 0x6b, 0xc0, 0x06, 0x6d, 0xe0, 0x06, 0x6f, 0x00, 0x07,
	0x71, 0x20, 0x07, 0x73, 0x40, 0x07, 0x75, 0x60, 0x07, 0x77, 0x80, 0x07,
	0x79, 0xa0, 0x07, 0x7b, 0xc0, 0x07, 0x7d, 0xe0, 0x07, 0x7f, 0x00, 0x08,
	0x81, 0x20, 0x08, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};


#define DIRECTORY_LEN 64
unsigned char directory[] = {
	0x41, 0x66, 0x00, 0x69, 0x00, 0x72, 0x00, 0x6d, 0x00, 0x77, 0x00, 0x0f,
	0x00, 0x57, 0x61, 0x00, 0x72, 0x00, 0x65, 0x00, 0x2e, 0x00, 0x62, 0x00,
	0x69, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x46, 0x49, 0x52, 0x4d,
	0x57, 0x41, 0x52, 0x45, 0x42, 0x49, 0x4e, 0x20, 0x00, 0x00, 0xce, 0x01,
	0x86, 0x41, 0x86, 0x41, 0x00, 0x00, 0xce, 0x01, 0x86, 0x41, 0x03, 0x00,
	0x00, 0xc0, 0x03, 0x00
};




void *massStorageOpen(unsigned long drive)
{	
	return ((void *)&massStorageDrive);
}


void massStorageClose(void * drive)
{
	
}

unsigned long massStorageRead(void * drive, unsigned char *data,unsigned long blockNumber,unsigned long numberOfBlocks)
{
	
#ifdef DEBUG
	UARTprintf("Read block: %d, no. of blocks: %d\n",blockNumber,numberOfBlocks);
#endif	

	memset(data,0,BLOCK_SIZE);
	
	if (blockNumber==0){
		memcpy(data,bootSector,BOOT_LEN);
		data[510]=0x55;
		data[511]=0xaa;
	}
	else if (blockNumber==1||blockNumber==2){
		memcpy(data,fat,FAT_LEN);
		
	}
	else if (blockNumber==3){
		memcpy(data,directory,DIRECTORY_LEN);
	}
	else if (blockNumber>=FIRMWARE_START_SECTOR&&blockNumber<FIRMWARE_START_SECTOR+USER_PROGRAM_LENGTH/BLOCK_SIZE){
		memcpy(data,(unsigned char*)(USER_PROGRAM_START+(blockNumber-FIRMWARE_START_SECTOR)*BLOCK_SIZE),BLOCK_SIZE);
	}
	
	return BLOCK_SIZE;
}

unsigned long counter=0;

unsigned long massStorageWrite(void * drive,unsigned char *data,unsigned long blockNumber,unsigned long numberOfBlocks)
{
	
	//FlashProgram((unsigned long*)data,USER_PROGRAM_START+counter,BLOCK_SIZE*numberOfBlocks);
	//counter+=BLOCK_SIZE*numberOfBlocks;
	
#ifdef DEBUG
	UARTprintf("Write block: %d, no. of blocks: %d\n",blockNumber,numberOfBlocks);
	UARTprintf("Firmware start cluster: %d\n",firmware_start_cluster);
	
	int i,j;
	
	for (j=0;j<BLOCK_SIZE*numberOfBlocks;j+=16){
		for (i=0;i<16;i++)
		UARTprintf("%x ",data[j+i]);
		UARTprintf("\n");
	}
	
#endif	 
	
	if (blockNumber==0){
		memcpy(bootSector,data,BOOT_LEN);
	}
	else if (blockNumber==1||blockNumber==2){
		memcpy(fat,data,FAT_LEN);
		firmware_start_cluster=directory[58];//sometimes the system will move the firmware to a different cluster
		
	}
	else if (blockNumber==3){
		memcpy(directory,data,DIRECTORY_LEN);
		
		
	}
	else if (blockNumber>=FIRMWARE_START_SECTOR){
		if (!newFirmwareStartSet){
			//the host tried to write actual data to some far away block
			newFirmwareStartSet=1;
			firmware_start_cluster=(blockNumber-27)/4;
		}
		//new firmware is being uploaded
		if (blockNumber<FIRMWARE_START_SECTOR+USER_PROGRAM_LENGTH/BLOCK_SIZE){
			unsigned long address=(blockNumber-FIRMWARE_START_SECTOR)*BLOCK_SIZE+USER_PROGRAM_START;
			
			if (blockNumber==FIRMWARE_START_SECTOR){
				for (counter=0;counter<239;counter++)
				FlashErase(USER_PROGRAM_START+counter*1024);
			}
			
			FlashProgram((unsigned long*)data,address,BLOCK_SIZE*numberOfBlocks);
			
			return BLOCK_SIZE*numberOfBlocks;
		}
		
		
	}
	
	//new firmware is being uploaded
	/* else if (blockNumber>=FIRMWARE_START_SECTOR&&blockNumber<FIRMWARE_START_SECTOR+USER_PROGRAM_LENGTH/BLOCK_SIZE){
		unsigned long address=(blockNumber-FIRMWARE_START_SECTOR)*BLOCK_SIZE+USER_PROGRAM_START;
				
		if (blockNumber==FIRMWARE_START_SECTOR){
			for (counter=0;counter<239;counter++)
		FlashErase(USER_PROGRAM_START+counter*1024);
		}
		
		FlashProgram((unsigned long*)data,address,BLOCK_SIZE*numberOfBlocks);
		
		return BLOCK_SIZE*numberOfBlocks;
	} */
	
	
	return BLOCK_SIZE*numberOfBlocks;
}


unsigned long massStorageNumBlocks(void * drive)
{
	return 1024; //filesystem size is 512kB
}


