#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include "i2c-dev.h"

#include "ow.h"
#include "ow_search.h"
#include "ow_functions.h"

#define MAX_BUS 2
#define MAX_DEVS 15

struct global Globals = {
	.daemon_status = e_daemon_fg,
	.error_level = e_err_default,
	.error_print = e_err_print_mixed,
	.fatal_debug = 1,
	.fatal_debug_file = NULL,
	.uncached = 0,
	.timeout_volatile = 15,
	.timeout_stable = 300,
	.timeout_directory = 60,
	.timeout_presence = 120,
};

int fd;
uint8_t devs[MAX_BUS][MAX_DEVS][8];

/* Length of a property element */
/* based on property type in most cases, except ascii and binary, which are explicitly sized */
size_t FileLength(const struct parsedname *pn)
{
	if (pn->type == ePN_structure) {
		return PROPERTY_LENGTH_STRUCTURE;	/* longest seem to be /1wire/structure/0F/memory.ALL (28 bytes) so far... */
	}
	/* directory ? */
	if (IsDir(pn)) {
		return PROPERTY_LENGTH_DIRECTORY;
	}

	switch (pn->selected_filetype->format) {
	case ft_yesno:
		return PROPERTY_LENGTH_YESNO;
	case ft_integer:
		return PROPERTY_LENGTH_INTEGER;
	case ft_unsigned:
		return PROPERTY_LENGTH_UNSIGNED;
	case ft_float:
		return PROPERTY_LENGTH_FLOAT;
	case ft_pressure:
		return PROPERTY_LENGTH_PRESSURE;
	case ft_temperature:
		return PROPERTY_LENGTH_TEMP;
	case ft_tempgap:
		return PROPERTY_LENGTH_TEMPGAP;
	case ft_date:
		return PROPERTY_LENGTH_DATE;
	case ft_bitfield:
		return (pn->extension == EXTENSION_BYTE) ? PROPERTY_LENGTH_UNSIGNED : PROPERTY_LENGTH_YESNO;
	case ft_vascii:			// not used anymore here...
	case ft_alias:
	case ft_ascii:
	case ft_binary:
	default:
		return pn->selected_filetype->suglen;
	}
}

/* Length of file based on filetype and extension */
size_t FullFileLength(const struct parsedname * pn)
{
	size_t entry_length = FileLength(pn);
	if (pn->type == ePN_structure) {
		return entry_length;
	} else if (pn->extension != EXTENSION_ALL) {
		return entry_length;
	} else {
		size_t elements = pn->selected_filetype->ag->elements;
		if (pn->selected_filetype->format == ft_binary) {
			return entry_length * elements;
		} else {				// add room for commas
			return (entry_length + 1) * elements - 1;
		}
	}
}

static int getattr_callback(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	if (strncmp(path, "/bus", 4) == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp(path, "/status") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp(path, "/status/mode") == 0) {
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size = 5;
		return 0;
	}
	if (strcmp(path, "/search") == 0) {
		stbuf->st_mode = S_IFREG | 0666;
		stbuf->st_nlink = 1;
		stbuf->st_size = 5;
		return 0;
	}
	if (strcmp(path, "/status/cmd") == 0) {
		stbuf->st_mode = S_IFREG | 0222;
		stbuf->st_nlink = 1;
		stbuf->st_size = 5;
		return 0;
	}

	printf ("getattr fail\n");
	return -ENOENT;
}

#define FILLER(handle,name) filler(handle,name,DT_DIR,0)

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi) {
	int i, j, bus = 0;

	(void) offset;
	(void) fi;
	char s[32];

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	if (strcmp(path, "/bus.0") == 0) {
		i = 0;
		while(devs[bus][i][0] != 0) {
			printf ("id %d\n", i);
			sprintf(s, "%02X.%02X%02X%02X%02X%02X%02X",
				devs[bus][i][0], devs[bus][i][1],
				devs[bus][i][2], devs[bus][i][3],
				devs[bus][i][4], devs[bus][i][5],
				devs[bus][i][6]);
			filler(buf, s, NULL, 0);
			i++;
		}
		return 0;
	}
	if (strcmp(path, "/bus.1") == 0) {
		i = 1;
		sprintf(s, "%02X.%02X%02X%02X%02X%02X%02X%02X",
			devs[bus][i][0], devs[bus][i][1],
			devs[bus][i][2], devs[bus][i][3],
			devs[bus][i][4], devs[bus][i][5],
			devs[bus][i][6], devs[bus][i][7]);
		filler(buf, s, NULL, 0);
		return 0;
	}
	if (strcmp(path, "/status") == 0) {
		filler(buf, "mode", NULL, 0);
		filler(buf, "cmd", NULL, 0);
		return 0;
	}

	filler(buf, "search", NULL, 0);
	filler(buf, "bus.0", NULL, 0);
	filler(buf, "bus.1", NULL, 0);

  return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	return 0;
}

/* large enough for arrays of 2048 elements of ~49 bytes each */
#define MAX_OWSERVER_PROTOCOL_PAYLOAD_SIZE  100050

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
	int res;

	if (strcmp(path, "/status/mode") == 0) {
		i2c_smbus_write_byte_data(fd, 0xE1, 0x69);
		res = i2c_smbus_read_byte(fd);
		sprintf (buf, "0x%02X\n", res);
		return 5;
	}

	int return_size ;
	struct one_wire_query struct_owq;
	struct one_wire_query *owq = &struct_owq;

	memset(&struct_owq, 0, sizeof(struct one_wire_query));

	if (path == NO_PATH)
		path = "/";

	/* Can we parse the input string */
	if (BAD(OWQ_create(path, owq)))
		return -ENOENT;

	if ( IsDir( PN(owq) ) ) { /* A directory of some kind */
		return_size = -EISDIR ;
	} else if ( offset >= (off_t) FullFileLength( PN(owq) ) ) {
		// fuse requests a useless read at end of file -- just return ok.
		return_size = 0 ;
	} else {
		if ( size > MAX_OWSERVER_PROTOCOL_PAYLOAD_SIZE ) {
			LEVEL_DEBUG( "Requested read length %ld will be trimmed to owfs max %ld",(long int) size, (long int) MAX_OWSERVER_PROTOCOL_PAYLOAD_SIZE ) ;
			size = MAX_OWSERVER_PROTOCOL_PAYLOAD_SIZE ;
		}
		OWQ_assign_read_buffer(buf, size, offset, owq) ;
		return_size = FS_read_postparse(owq) ;
	}
	OWQ_destroy(owq);

	return return_size ;

	return -ENOENT;
}

#if 0
int owSearch()
{
	int res;
	int bus = 0, i, to;
	uint8_t data[10];

	memset(&devs[bus][0][0], 0, MAX_DEVS * 8);
	printf ("searching\n");
	i2c_smbus_write_byte(fd, 0x5A);
	sleep(1);
#ifdef WAIT_POLL
	res = i2c_smbus_read_byte(fd);
	printf ("res=%d\n", res);
	to = 20;
	do {
		usleep(1000);
		res = i2c_smbus_read_byte(fd);
		if (to-- == 0)
			break;
	} while (res & 0x01);
	printf ("res=%d\n", res);
	/*if (res != 0)
		return 0;*/
#endif
#ifdef WAIT_POLL
	i2c_smbus_write_byte_data(fd, 0xE1, 0xE1);
	res = i2c_smbus_read_byte(fd);
	printf ("res=%d\n", res);
	to = 20;
	do {
		usleep(1000);
		res = i2c_smbus_read_byte(fd);
		if (to-- == 0)
			break;
		printf (".");
		fflush(stdout);
	} while (res & 0x01);
	printf ("res=%d\n", res);
	if (res & 0x81 != 0)
		return 0;
#endif
	printf ("reading\n");
	//i2c_smbus_write_byte_data(fd, 0xE1, 0xA9);
	i2c_smbus_write_byte(fd, 0x96);
	usleep(100);
	memset(data, 0, sizeof(data));
	/*res = i2c_smbus_read_block_data(fd, 0x96, data);
	printf ("res=%d\n", res);*/
	for (i = 0; i < MAX_DEVS; i++) {
		for (int j = 0; j < 7; j++) {
			devs[bus][i][j] = i2c_smbus_read_byte(fd);
		}
		if (devs[bus][i][0] == 0)
			break;
		printf("%02X.%02X%02X%02X%02X%02X%02X\n",
			devs[bus][i][0], devs[bus][i][1],
			devs[bus][i][2], devs[bus][i][3],
			devs[bus][i][4], devs[bus][i][5],
			devs[bus][i][6]);
	}
	i = 0;

	i2c_smbus_write_byte(fd, 0x5B);
	return 0;
}
#endif

static int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *flags)
{
	if (strcmp(path, "/status/mode") == 0) {
		int mode;

		if (!sscanf(buf, "0x%X\n", &mode))
			sscanf(buf, "%d\n", &mode);
		printf ("Mode=%X\n", mode);

		return size;
	}
	if (strcmp(path, "/status/cmd") == 0) {
		int mode;

		if (!sscanf(buf, "0x%X\n", &mode))
			sscanf(buf, "%d\n", &mode);
		printf ("cmd=%X\n", mode);
		i2c_smbus_write_byte(fd, mode);

		return size;
	}
	if (strcmp(path, "/search") == 0) {
		//owSearch();
		return size;
	}

	return size;
}

static int stat_cb (const char *path, struct statvfs *stat)
{
	memset(stat, 0, sizeof(struct statvfs));
	return 0;
}

static int truncate_cb (const char *path, long long int size/*,	 struct fuse_file_info *fi*/)
{
	(void) size;
	/*if(strcmp(path, "/") != 0)
		return -ENOENT;
	*/
	return size;
}

static struct fuse_operations fuse_example_operations = {
	.getattr = getattr_callback,
	.truncate	= truncate_cb,
	.open = open_callback,
	.read = read_callback,
	.write = write_callback,
	.readdir = readdir_callback,
};

#include "ow_search.h"
extern int DS2482_channel_select(int fd, int chan);
extern enum search_status DS2482_next_both(struct device_search *ds, int fd);
extern GOOD_OR_BAD DS2482_send(int fd, const uint8_t wr);
extern int DS2482_reset(int fd);

int main(int argc, char *argv[])
{
	int ret, adr;

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
	} else {
		struct device_search ds;

		printf("Found an i2c device at address %.2X\n", adr);
		DS2482_channel_select(fd, 0);
		ds.LastDevice = 0;
		ds.search = _1W_SEARCH_ROM;
		do {
			DS2482_reset(fd);
			DS2482_send(fd, _1W_SEARCH_ROM);
			DS2482_next_both(&ds, fd);
			LEVEL_DEFAULT("SN found: " SNformat, SNvar(ds.sn));
		} while(ds.LastDevice == 0);
	}

	adr = 0x2f;
	if (ioctl(fd, I2C_SLAVE, adr) < 0) {
		printf("Cound not set trial i2c address to %.2X\n", adr);
		close(fd);
		return -1;
	} else {
		printf("Found an i2c device at address %.2X\n", adr);
	}
	//owSearch();
	ret = fuse_main(argc, argv, &fuse_example_operations, NULL);

	close(fd);
	return ret;
}
