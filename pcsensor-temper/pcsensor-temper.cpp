// pcsensor-temper.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lusb0_usb.h"
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include "windows.h" 

#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

#define VERSION "0.0.1"

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401

#define INTERFACE 0x01

const static char uTemperatura[] = { 0x01, 0x80, 0x33, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni1[] = { 0x01, 0x82, 0x77, 0x01, 0x00, 0x00, 0x00, 0x00 };
const static char uIni2[] = { 0x01, 0x86, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00 };

const static int reqIntLen=8;

static int measureDelay=2000;/* delay in ms */
const static int timeout=5000; /* timeout in ms */

/* Offset of temperature in read buffer; varies by product */
static size_t tempOffset;

static int bsalir=0;
static int formato=1;
static int debug=1;
static int mrtg=0;

void ex_program(int sig) {
	bsalir=1;

	(void) signal(SIGINT, SIG_DFL);
}

usb_dev_handle *find_lvr_winusb();

usb_dev_handle* setup_libusb_access() {
	usb_dev_handle *lvr_winusb;

	if(debug) {
		usb_set_debug(255);
	} else {
		usb_set_debug(0);
	}

	usb_init();
	usb_find_busses();
	usb_find_devices();


	if(!(lvr_winusb = find_lvr_winusb())) {
		printf("Couldn't find the USB device, Exiting\n");
		return NULL;
	}

	if (usb_set_configuration(lvr_winusb, 0x01) < 0) {
		printf("Could not set configuration 0\n");
		return NULL;
	}

	if (usb_claim_interface(lvr_winusb, INTERFACE) < 0) {
		printf("Could not claim interface\n");
		return NULL;
	}

	return lvr_winusb;
}

usb_dev_handle *find_lvr_winusb() {
	struct usb_bus *bus;
	struct usb_device *dev;

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {

			if (dev->descriptor.idVendor == VENDOR_ID && 
				dev->descriptor.idProduct == PRODUCT_ID ) {
					usb_dev_handle *handle;
					if(debug) {
						printf("lvr_winusb with Vendor Id: %x and Product Id: %x found.\n", VENDOR_ID, PRODUCT_ID);
					}

					if (!(handle = usb_open(dev))) {
						printf("Could not open USB device\n");
						return NULL;
					}
					return handle;
			}
		}
	}
	return NULL;
}

void ini_control_transfer(usb_dev_handle *dev) {
	int r,i;
	char question[] = { 0x01,0x01 };
	r = usb_control_msg(dev, 0x21, 0x09, 0x0201, 0x00, (char *) question, 2, timeout);
	if( r < 0 )
	{
		perror("USB init control write");
	}
	if(debug) {
		for (i=0;i<reqIntLen; i++) printf("%02x ",question[i] & 0xFF);
		printf("\n");
	}
}

void control_transfer(usb_dev_handle *dev, const char *pquestion) {
	int r,i;
	char question[reqIntLen];
	memcpy(question, pquestion, sizeof question);
	r = usb_control_msg(dev, 0x21, 0x09, 0x0200, 0x01, (char *) question, reqIntLen, timeout);
	if( r < 0 )
	{
		perror("USB control write");
	}
	if(debug) {
		for (i=0;i<reqIntLen; i++) printf("%02x ",question[i] & 0xFF);
		printf("\n");
	}
}

void interrupt_read(usb_dev_handle *dev) {
	int r,i;
	char answer[reqIntLen];
	bzero(answer, reqIntLen);
	r = usb_interrupt_read(dev, 0x82, (char *) answer, reqIntLen, timeout);
	if( r != reqIntLen )
	{
		perror("USB interrupt read");
	}
	if(debug) {
		for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i] & 0xFF);
		printf("\n");
	}
}

void interrupt_read_temperatura(usb_dev_handle *dev, float *tempC) {

	int r,i, temperature;
	unsigned char answer[reqIntLen];
	bzero(answer, reqIntLen);

	r = usb_interrupt_read(dev, 0x82, (char *) answer, reqIntLen, timeout);
	if( r != reqIntLen )
	{
		perror("USB interrupt read");
	}

	if(debug) {
		for (i=0;i<reqIntLen; i++) printf("%02x ",answer[i]  & 0xFF);

		printf("\n");
	}

	/* Temperature in C is a 16-bit signed fixed-point number, big-endian */
	temperature = (answer[3] & 0xFF) + (answer[2] << 8);
	*tempC = temperature * (125.0f / 32000.0f);
}



int _tmain(int argc, _TCHAR* argv[])
{
	printf("pcsensor version %s\n", VERSION);

	usb_dev_handle *lvr_winusb = NULL;
	float tempc;

	struct tm *local;
	time_t t;

	if ((lvr_winusb = setup_libusb_access()) == NULL) {
		exit(EXIT_FAILURE);
	} 

	(void) signal(SIGINT, ex_program);

	ini_control_transfer(lvr_winusb);

	control_transfer(lvr_winusb, uTemperatura );
	interrupt_read(lvr_winusb);

	control_transfer(lvr_winusb, uIni1 );
	interrupt_read(lvr_winusb);

	control_transfer(lvr_winusb, uIni2 );
	interrupt_read(lvr_winusb);
	interrupt_read(lvr_winusb);

	do {
		control_transfer(lvr_winusb, uTemperatura );
		interrupt_read_temperatura(lvr_winusb, &tempc);

		t = time(NULL);
		local = localtime(&t);

		printf("%04d/%02d/%02d %02d:%02d:%02d ", 
			local->tm_year + 1900, 
			local->tm_mon + 1, 
			local->tm_mday,
			local->tm_hour,
			local->tm_min,
			local->tm_sec);

		printf("Temperature %.2fC\n", tempc);

		if (!bsalir)
			Sleep(measureDelay);

	} while (!bsalir);

	usb_release_interface(lvr_winusb, INTERFACE);

	usb_close(lvr_winusb); 

	return 0;
}

