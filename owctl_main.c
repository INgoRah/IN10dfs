#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

int fd;

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

static int getattr_callback(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid(); 
	if (strcmp(path, "/") == 0) {
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
  (void) offset;
  (void) fi;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  if (strcmp(path, "/status") == 0) {
	filler(buf, "mode", NULL, 0);
	filler(buf, "cmd", NULL, 0);
  } else {
	filler(buf, "status", NULL, 0);
  }

  return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
  if (strcmp(path, "/status/mode") == 0) {
      memcpy(buf, "0xAA\n", 5);
      return 5;
  }

  return -ENOENT;
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
	i2c_smbus_write_byte_data(fd, 0x69, 0x6);

	ret = fuse_main(argc, argv, &fuse_example_operations, NULL);
	
	close(fd);
	return ret;
}
