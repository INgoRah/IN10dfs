#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <linux/i2c-dev.h>

static const char *filepath = "/file";
static const char *filename = "file";
static const char *filecontent = "I'm the content of the only file available there\n";

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
		printf ("status dir request\n");
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	if (strcmp(path, filepath) == 0) {
		stbuf->st_mode = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(filecontent);
		return 0;
	}
	if (strcmp(path, "/status/mode") == 0) {
		stbuf->st_mode = S_IFREG | 0777;
		printf ("getattr\n");
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
  } else {
	filler(buf, filename, NULL, 0);
	filler(buf, "status", NULL, 0);
  }

  return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	printf("open %s\n", path);
	
	return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset,
    struct fuse_file_info *fi)
{
	printf("read %s\n", path);

  if (strcmp(path, "/status/mode") == 0) {
      memcpy(buf, "0xAA\n", 5);
      return 5;
  }
  if (strcmp(path, filepath) == 0) {
    size_t len = strlen(filecontent);
    if (offset >= len) {
      return 0;
    }

    if (offset + size > len) {
      memcpy(buf, filecontent + offset, len - offset);
      return len - offset;
    }

    memcpy(buf, filecontent + offset, size);
    return size;
  }

  return -ENOENT;
}

static int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *flags)
{
	printf("write path=%s size=%d offset=%d", path, (int) size, (int) offset);
	printf(" buf=%s\n", buf);
	
	if (strcmp(path, "/status/mode") == 0) {
		int mode;
		
		if (!sscanf(buf, "0x%X\n", &mode))
			sscanf(buf, "%d\n", &mode);
		printf ("Mode=%X\n", mode);
		int fd = open("/dev/i2c-0", O_RDWR);
		close(fd);
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
	printf("truncate path=%s size=%d\n", path, (int) size);
	
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
  return fuse_main(argc, argv, &fuse_example_operations, NULL);
}
