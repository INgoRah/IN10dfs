#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define MAX_BUS 2
#define MAX_DEVS 15

int fd;
uint8_t devs[MAX_BUS][MAX_DEVS][8];

static inline __s32 i2c_smbus_access(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;
	return ioctl(file, I2C_SMBUS, &args);
}

static inline __s32 i2c_smbus_write_byte(int file, __u8 value)
{
	return i2c_smbus_access(file, I2C_SMBUS_WRITE, value, I2C_SMBUS_BYTE, NULL);
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command, __u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

static inline __s32 i2c_smbus_read_byte(int file)
{
	union i2c_smbus_data data;
	if (i2c_smbus_access(file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))
		return -1;
	else
		return 0x0FF & data.byte;
}

/* Returns the number of read bytes */
static inline __s32 i2c_smbus_read_block_data(int file, __u8 command, __u8 * values)
{
	union i2c_smbus_data data;
	int i;
	if (i2c_smbus_access(file, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data))

	if (i2c_smbus_access(file, I2C_SMBUS_READ, command, I2C_SMBUS_BLOCK_DATA, &data))
		return -1;
	else {
		for (i = 1; i <= data.block[0]; i++)
			values[i - 1] = data.block[i];
		return data.block[0];
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

	return -ENOENT;
}

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
		owSearch();
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

int main(int argc, char *argv[])
{
	int ret;
	fd = open("/dev/i2c-0", O_RDWR);
	int adr = 0x2f;
	if (fd < 0) {
		printf("I2c device open error (%s)\n", strerror(errno));
		return -1;
	}
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
