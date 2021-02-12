/*
   OWFS and OWHTTPD
   one-wire file system and
   one-wire web server

    By Paul H Alfille
    {c} 2003 GPL
    paul.alfille@gmail.com
*/

/* OWFS - specific header */

#ifndef OWFS_H
#define OWFS_H

#include <fuse.h>

/* Include FUSE -- http://fuse.sf.net */
/* Lot's of version-specific code */

extern struct fuse_operations owfs_oper;
int FS_getdir(const char *path, fuse_dirh_t h, fuse_dirfil_t filler);
int FS_truncate(const char *path, const off_t size);
int FS_open(const char *path, int flags);
int FS_release(const char *path, int flags);
int FS_chmod(const char *path, mode_t mode);
int FS_chown(const char *path, uid_t uid, gid_t gid);
int CB_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *flags);
int CB_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *flags);

struct Fuse_option {
	int allocated_slots;
	char **argv;
	int argc;
};

int Fuse_setup(struct Fuse_option *fo);
void Fuse_cleanup(struct Fuse_option *fo);
int Fuse_parse(char *opts, struct Fuse_option *fo);
int Fuse_add(char *opt, struct Fuse_option *fo);
char *Fuse_arg(char *opt_arg, char *entryname);

#endif							/* OWFS_H */
