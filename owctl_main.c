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
#include "ow_cache.h"
#include "ow_functions.h"
#include "sys/mount.h"
#include "ow_connection.h"

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
	struct port_in * pin;

	printf ("IN10d %s\n", __TIME__);
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
	/* Set up owlib */
	Cache_Open();
	/* device arg / i2c args */
	pin = NewPort(NULL) ;
	if (pin == NULL || pin->first == NO_CONNECTION)
		return -1;

	/* libstart */
	/* Build device and filetype arrays (including externals) */
	DeviceSort();

	if (DS2482_detect(pin) == -1) {
		LEVEL_DEFAULT("Cannot detect an i2c DS2482-x00");
		return -1;
	}
	LEVEL_DEFAULT("detected an i2c DS2482-x00");

#if FUSE_VERSION > 25
	ret = fuse_main(argc, argv, &owfs_oper, NULL);
#else							/* FUSE_VERSION <= 25 */
	ret = fuse_main(argc, argv, &owfs_oper);
#endif							/* FUSE_VERSION <= 25 */

	Cache_Close();
	return ret;
}
