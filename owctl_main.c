#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "i2c-dev.h"

#include "ow.h"
#include "owfs.h"
#include "ow_search.h"
#include "ow_functions.h"
#include "sys/mount.h"

#define MAX_BUS 2
#define MAX_DEVS 15

struct global Globals = {
	.daemon_status = e_daemon_fg,
	.error_level = e_err_debug,
	.error_print = e_err_print_console /*e_err_print_mixed*/,
	.fatal_debug = 1,
	.fatal_debug_file = NULL,
	.uncached = 0,
	.timeout_volatile = 15,
	.timeout_stable = 300,
	.timeout_directory = 60,
	.timeout_presence = 120,
};

int fd;

int main(int argc, char *argv[])
{
	int ret;

	printf ("IN10d %s\n", __TIME__);
	printf ("%d\n", Globals.error_level);
	LEVEL_DEFAULT("Lvl Default");
	LEVEL_CALL("Lvl Call");
	if (Globals.error_level>=e_err_call)  {
    	err_msg(e_err_type_level,e_err_call,   __FILE__,__LINE__,__func__,"test"); }
#if 0
	int adr;
	fd = open("/dev/i2c-0", O_RDWR);
	if (fd < 0) {
		printf("I2c device open error (%s)\n", strerror(errno));
		return -1;
	}
	adr = 0x18;
	if (ioctl(fd, I2C_SLAVE, adr) < 0) {
		printf("Cound not set trial i2c address to %.2X\n", adr);
		close(fd);
		return -1;
	}
	adr = 0x2f;
	if (ioctl(fd, I2C_SLAVE, adr) < 0) {
		printf("Cound not set trial i2c address to %.2X\n", adr);
		close(fd);
		return -1;
	} else {
		printf("Found an i2c device at address %.2X\n", adr);
	}
	close(fd);
#endif
	/* libstart */
	/* Build device and filetype arrays (including externals) */
	DeviceSort();
	/*

struct inbound_control Inbound_Control = {
	.active = 0,
	.next_index = 0,
	.head_port = NULL,
};

	struct port_in pin;
	pin.file_descriptor = -1;
	Inbound_Control.head_port = pin ;

	struct port_in *pin = Inbound_Control.head_port;

	if ( BAD( DS2482_detect(pin) )) {
	*/
	if (DS2482_detect(&fd) == -1) {
		LEVEL_DEFAULT("Cannot detect an i2c DS2482-x00");
		return -1;
	}
	printf ("fd=%d\n", fd);

#if FUSE_VERSION > 25
	ret = fuse_main(argc, argv, &owfs_oper, NULL);
#else							/* FUSE_VERSION <= 25 */
	ret = fuse_main(argc, argv, &owfs_oper);
#endif							/* FUSE_VERSION <= 25 */

	return ret;
}
