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

#define VERSION "0.0.1"

#define VENDOR_ID  0x0c45
#define PRODUCT_ID 0x7401
 
#define INTERFACE1 0x00
#define INTERFACE2 0x01


 /* Offset of temperature in read buffer; varies by product */
static size_t tempOffset;

static int bsalir=1;
static int seconds=5;
static int formato=0;
static int debug=0;
static int mrtg=0;

/* Even within the same VENDOR_ID / PRODUCT_ID, there are hardware variations
* which we can detect by reading the USB product ID string. This determines
* where the temperature offset is stored in the USB read buffer. */
const static struct product_hw {
	size_t      offset;
	const char *id_string;
} product_ids[] = {
	{ 4, "TEMPer1F_V1.3" },
	{ 2, 0 } /* default offset is 2 */
};

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
		printf("Could not set configuration 1\n");
		return NULL;
	}


	// Microdia tiene 2 interfaces
	if (usb_claim_interface(lvr_winusb, INTERFACE1) < 0) {
		printf("Could not claim interface\n");
		return NULL;
	}

	if (usb_claim_interface(lvr_winusb, INTERFACE2) < 0) {
		printf("Could not claim interface\n");
		return NULL;
	}

	return lvr_winusb;
}

static void read_product_string(usb_dev_handle *handle, struct usb_device *dev)
{
	char prodIdStr[256];
	const struct product_hw *p;
	int strLen;

	memset(prodIdStr, 0, sizeof(prodIdStr));

	strLen = usb_get_string_simple(handle, dev->descriptor.iProduct, prodIdStr,
		sizeof(prodIdStr)-1);
	if (debug) {
		if (strLen < 0)
			puts("Couldn't read iProduct string");
		else
			printf("iProduct: %s\n", prodIdStr);
	}

	for (p = product_ids; p->id_string; ++p) {
		if (!strncmp(p->id_string, prodIdStr, sizeof(prodIdStr)))
			break;
	}
	tempOffset = p->offset;
}

usb_dev_handle *find_lvr_winusb() {
	struct usb_bus *bus;
	struct usb_device *dev;

	for (bus = usb_busses; bus; bus = bus->next) {
		for (dev = bus->devices; dev; dev = dev->next) {

			printf("Device with Vendor Id: %x and Product Id: %x found.\n", dev->descriptor.idVendor, dev->descriptor.idProduct);

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
					read_product_string(handle, dev);
					return handle;
			}
		}
	}
	return NULL;
}

int _tmain(int argc, _TCHAR* argv[])
{
	printf("pcsensor version %s\n",VERSION);

	usb_dev_handle *lvr_winusb = NULL;

	debug = 1;
	formato=1;

	 if ((lvr_winusb = setup_libusb_access()) == NULL) {
         exit(EXIT_FAILURE);
     } 

	(void) signal(SIGINT, ex_program);


	usb_init();
	return 0;
}

