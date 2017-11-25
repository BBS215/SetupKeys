#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "main.h"
#include "hid.h"

////////// SETTINGS ///////////////
#define DEVICE_VID			0x043B
#define DEVICE_PID			0x0325
#define DEVICE_USAGE_PAGE	0x1 
#define DEVICE_USAGE		0x80
//////////////////////////////////

#define DEBUG_PRINT			1
#define CONFIGURE_KEYS_REPORT_ID	(BYTE)0x3
#define READ_KEYS_REPORT_ID			(BYTE)0x4
#define CONFIGURE_KEYS_DELAY_MS		100

void usleep(__int64 usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}

PHID_DEVICE Find_My_Keyboard(PHID_DEVICE deviceList, ULONG numDevices)
{
	ULONG i;
	PHID_DEVICE my_device = NULL;
	if (!deviceList) return NULL;
	if (!numDevices) return NULL;
	for (i = 0; i < numDevices; i++)
	{
		if (INVALID_HANDLE_VALUE != deviceList[i].HidDevice)
		{
			if ((deviceList[i].Attributes.VendorID == DEVICE_VID) &&
				(deviceList[i].Attributes.ProductID == DEVICE_PID) &&
				(deviceList[i].Caps.UsagePage == DEVICE_USAGE_PAGE) &&
				(deviceList[i].Caps.Usage == DEVICE_USAGE))
			{
				my_device = &deviceList[i];
				break;
			}
		}
	}
	return my_device;
}

int Write_to_device(uint8_t *p_buffer, uint8_t buf_size)
{
	HID_DEVICE			writeDevice;
	DWORD				bytesWritten = 0;
	BOOL				status = FALSE;
	ULONG				SetUsageStatus = 0;
	uint8_t				write_buffer[8];
	int					ret = 0;
	PHID_DEVICE			deviceList = NULL;
	ULONG				numDevices = 0;
	PHID_DEVICE			pDevice = NULL;

	if (!p_buffer) return -1;
	if (!buf_size) return -2;
	if (buf_size > 8) return -3;
	ZeroMemory(&writeDevice, sizeof(writeDevice));
	ZeroMemory(&write_buffer, 8);
	memcpy(write_buffer, p_buffer, buf_size);

	FindKnownHidDevices(&deviceList, &numDevices);
	pDevice = Find_My_Keyboard(deviceList, numDevices);
	if (!pDevice) {
		if (deviceList) {
			free(deviceList);
			deviceList = NULL;
		}
		return -4;
	}

	status = OpenHidDevice(pDevice->DevicePath,
		FALSE,
		TRUE,
		FALSE,
		FALSE,
		&writeDevice);
	if (deviceList) {
		free(deviceList);
		deviceList = NULL;
		pDevice = NULL;
	}
	if (!status) return -5;
	
	SetUsageStatus = dll_HidP_SetUsageValue(HidP_Output,
		writeDevice.OutputData->UsagePage,
		0, // All Collections
		writeDevice.OutputData->ValueData.Usage,
		writeDevice.OutputData->ValueData.Value,
		writeDevice.Ppd,
		writeDevice.OutputReportBuffer,
		writeDevice.Caps.OutputReportByteLength);

	if (SetUsageStatus != HIDP_STATUS_SUCCESS)
	{
		ret = -6;
	} else {
		status = WriteFile(writeDevice.HidDevice,
			write_buffer,
			writeDevice.Caps.OutputReportByteLength,
			&bytesWritten,
			NULL);
		if (!status) ret = -7;
		if ((status) && (bytesWritten != writeDevice.Caps.OutputReportByteLength)) ret = -8;
	}
	CloseHidDevice(&writeDevice);
	return ret;
}

int Read_from_device(
	_In_ BYTE           reportID, 
	_Out_ uint8_t		*p_buffer, 
	_In_ uint8_t		buf_size)
{
	HID_DEVICE  syncDevice;
	BOOL status;
	PHID_DEVICE deviceList = NULL;
	ULONG       numDevices = 0;
	PHID_DEVICE	pDevice = NULL;
	int ret = 0;
	
	if (!p_buffer) return -1;
	if (!buf_size) return -2;
	
	RtlZeroMemory(&syncDevice, sizeof(syncDevice));

	FindKnownHidDevices(&deviceList, &numDevices);
	pDevice = Find_My_Keyboard(deviceList, numDevices);
	if (!pDevice) {
		if (deviceList) {
			free(deviceList);
			deviceList = NULL;
		}
		return -3;
	}

	status = OpenHidDevice(pDevice->DevicePath,
		TRUE,
		FALSE,
		FALSE,
		FALSE,
		&syncDevice);
	if (deviceList) {
		free(deviceList);
		deviceList = NULL;
		pDevice = NULL;
	}
	if (!status) return -4;
	
	syncDevice.InputReportBuffer[0] = reportID;
	status = dll_HidD_GetInputReport(syncDevice.HidDevice,
		syncDevice.InputReportBuffer,
		syncDevice.Caps.InputReportByteLength);
	if (!status)
	{
		//printf("HidD_GetInputReport() failed. Error: 0x%X\n", GetLastError());
		ret = -5;
	} else {
		ZeroMemory(p_buffer, buf_size);
		ret = (syncDevice.Caps.InputReportByteLength<buf_size)?syncDevice.Caps.InputReportByteLength:buf_size;
		memcpy(p_buffer, syncDevice.InputReportBuffer, ret);
	}
	CloseHidDevice(&syncDevice);
	return ret;
}

int Read_Key_Settings(uint8_t key_num, uint8_t *p_usage_page, uint8_t *p_modifiers, uint16_t *p_scancode)
{
	uint8_t buf[6];
	int retry_count = 15; // 15 sec
	int ret;

	buf[0] = READ_KEYS_REPORT_ID; 
	buf[1] = key_num;
	ret = Write_to_device(buf, 2);
	while ((ret) && (retry_count)) {
		usleep(1 * 1000 * 1000); // 1 sec
		buf[0] = READ_KEYS_REPORT_ID;
		buf[1] = key_num;
		ret = Write_to_device(buf, 2);
		retry_count--;
	};
	if (ret < 0) return -2;
	ZeroMemory(buf, 6);
	ret = Read_from_device(READ_KEYS_REPORT_ID, buf, 6);
	if (ret < 6) return -3;
	if (buf[0] != READ_KEYS_REPORT_ID) return -4;
	if (buf[1] != key_num) return -5;
	if (p_usage_page) *p_usage_page = buf[2];
	if (p_modifiers) *p_modifiers = buf[3];
	if (p_scancode) *p_scancode = (uint16_t)((uint16_t)buf[4] | ((uint16_t)(buf[5]) << 8));
	return 0;
}

int Write_Key_Settings(uint8_t key_num, uint8_t usage_page, uint8_t modifiers, uint16_t scancode)
{
	uint8_t buf[6];
	int retry_count = 15; // 15 sec
	int ret;
	buf[0] = CONFIGURE_KEYS_REPORT_ID;
	buf[1] = key_num;
	buf[2] = usage_page;
	buf[3] = modifiers;
	buf[4] = (uint8_t)(scancode & 0xFF);
	buf[5] = (uint8_t)((scancode >> 8) & 0xFF);
	ret = Write_to_device(buf, 6);
	if (ret < 0) return -2;
	usleep(CONFIGURE_KEYS_DELAY_MS * 1000);
	buf[0] = READ_KEYS_REPORT_ID;
	buf[1] = key_num;
	ret = Write_to_device(buf, 2);
	while ((ret) && (retry_count)) {
		//printf("Retry... ");
		usleep(1 * 1000 * 1000); // 1 sec
		buf[0] = READ_KEYS_REPORT_ID;
		buf[1] = key_num;
		ret = Write_to_device(buf, 2);
		retry_count--;
	};
	if (ret < 0) return -3;
	ZeroMemory(buf, 6);
	ret = Read_from_device(READ_KEYS_REPORT_ID, buf, 6);
	if (ret < 6) return -4;
	if (buf[0] != READ_KEYS_REPORT_ID) return -5;
	if (buf[1] != key_num) return -6;
	if (buf[2] != usage_page) return -7;
	if (buf[3] != modifiers) return -8;
	if ((uint16_t)((uint16_t)buf[4] | ((uint16_t)(buf[5]) << 8)) != scancode) return -9;
	return 0;
}

void print_usage(LPSTR * argv)
{
	printf("\nRead / write the settings of the HID keyboard keys.\n\n");
	printf("Usage: %s read/r/write/w key_num [usage_page] [modifiers] [scancode]\n\n", argv[0]);
	printf("Read key settings : %s read  key_num\n", argv[0]);
	printf("Write key settings: %s write key_num usage_page modifiers scancode\n", argv[0]);
}

LONG __cdecl main(LONG argc, LPSTR * argv)
{
	int						ret = 0;
	int						argc_cnt = 1;
	unsigned long			command = 0;
	unsigned long			param1 = 0;
	unsigned long			param2 = 0;
	unsigned long			param3 = 0;
	unsigned long			param4 = 0;



	if (argc > argc_cnt) {
		if ((strcmp(argv[argc_cnt], "read") == 0) || (strcmp(argv[argc_cnt], "r") == 0)) command = 1;
		else if ((strcmp(argv[argc_cnt], "write") == 0) || (strcmp(argv[argc_cnt], "w") == 0)) command = 2;
	}
	argc_cnt++;
	if (argc > argc_cnt) {
		if (argv[argc_cnt][1] == 'x') param1 = strtoul(argv[argc_cnt], NULL, 16);
		else param1 = strtoul(argv[argc_cnt], NULL, 10);
	}
	argc_cnt++;
	if (argc > argc_cnt) {
		if (argv[argc_cnt][1] == 'x') param2 = strtoul(argv[argc_cnt], NULL, 16);
		else param2 = strtoul(argv[argc_cnt], NULL, 10);
	}
	argc_cnt++;
	if (argc > argc_cnt) {
		if (argv[argc_cnt][1] == 'x') param3 = strtoul(argv[argc_cnt], NULL, 16);
		else param3 = strtoul(argv[argc_cnt], NULL, 10);
	}
	argc_cnt++;
	if (argc > argc_cnt) {
		if (argv[argc_cnt][1] == 'x') param4 = strtoul(argv[argc_cnt], NULL, 16);
		else param4 = strtoul(argv[argc_cnt], NULL, 10);
	}
	//argc_cnt++;

	if ((command != 1) && (command != 2)) {
		print_usage(argv);
		exit(0);
	}
	if ((command == 1) && (argc < 3)) {
		print_usage(argv);
		exit(0);
	}
	if ((command == 2) && (argc < 6)) {
		print_usage(argv);
		exit(0);
	}
	
	if (LoadHIDLib()) {
		printf("Error: coud not load HID.DLL!\n");
		goto exit;
	}

	{
		PHID_DEVICE				deviceList = NULL;
		ULONG					numDevices = 0;
		PHID_DEVICE				p_device = NULL;
		FindKnownHidDevices(&deviceList, &numDevices);
		p_device = Find_My_Keyboard(deviceList, numDevices);
		if (!p_device) {
			printf("Error: coud not find device!\n");
			goto exit;
		}

		if (DEBUG_PRINT) printf("Device found: VID: 0x%04X  PID: 0x%04X  UsagePage: 0x%X  Usage: 0x%X\n",
			p_device->Attributes.VendorID,
			p_device->Attributes.ProductID,
			p_device->Caps.UsagePage,
			p_device->Caps.Usage);

		if (deviceList) {
			free(deviceList);
			deviceList = NULL;
		}
	}

	if (command == 1) { // READ
		uint8_t key_num = (uint8_t)param1;
		uint8_t usage_page = 0;
		uint8_t modifiers = 0;
		uint16_t scancode = 0;
		printf("Reading settings of key %d... ", key_num);
		ret = Read_Key_Settings(key_num, &usage_page, &modifiers, &scancode);
		if (ret) {
			printf("Error %d\n", ret);
		} else {
			printf("OK!\n");
			printf("Key_num: %d\n", key_num);
			printf("Usage page: 0x%X\n", usage_page);
			printf("Modifiers: 0x%X\n", modifiers);
			printf("Scancode: 0x%X\n", scancode);

		}
	} else
	if (command == 2) { // WRITE
		uint8_t key_num = (uint8_t)param1;
		uint8_t usage_page = (uint8_t)param2;
		uint8_t modifiers = (uint8_t)param3;
		uint16_t scancode = (uint16_t)param4;
		printf("Key_num: %d\n", key_num);
		printf("Usage page: 0x%X\n", usage_page);
		printf("Modifiers: 0x%X\n", modifiers);
		printf("Scancode: 0x%X\n", scancode);
		printf("Writing settings of key %d... ", key_num);
		ret = Write_Key_Settings(key_num, usage_page, modifiers, scancode);
		if (ret) printf("Error %d\n", ret);
		else printf("OK!\n");
	}
	
exit:
	UnloadHIDLib();
	return ret;
}
