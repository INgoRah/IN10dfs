/*
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

    LICENSE (As of version 2.5p4 2-Oct-2006)
    owlib: GPL v2
    owfs, owhttpd, owftpd, owserver: GPL v2
    owshell(owdir owread owwrite owpresent): GPL v2
    owcapi (libowcapi): GPL v2
    owperl: GPL v2
    owtcl: LGPL v2
    owphp: GPL v2
    owpython: GPL v2
    owsim.tcl: GPL v2
    where GPL v2 is the "Gnu General License version 2"
    and "LGPL v2" is the "Lesser Gnu General License version 2"


    Written 2003 Paul H Alfille
        Fuse code based on "fusexmp" {GPL} by Miklos Szeredi, mszeredi@inf.bme.hu
        Serial code based on "xt" {GPL} by David Querbach, www.realtime.bc.ca
        in turn based on "miniterm" by Sven Goldt, goldt@math.tu.berlin.de
    GPL license
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    Other portions based on Dallas Semiconductor Public Domain Kit,
    ---------------------------------------------------------------------------
    Implementation:
    25-05-2003 iButtonLink device
*/

/* Cannot stand alone -- part of ow.h but separated for clarity */

#ifndef OW_FUNCTION_H			/* tedious wrapper */
#define OW_FUNCTION_H

#define BYTE_MASK(x)        ( (unsigned char) ((x) & 0xFF) )
#define BYTE_INVERSE(x)     BYTE_MASK((x) ^ 0xFF)
#define LOW_HIGH_ADDRESS(x)         BYTE_MASK(x),BYTE_MASK((x)>>8)

int UT_getbit(const uint8_t * buf, int loc);
void UT_setbit(uint8_t * buf, int loc, int bit);

int UT_getbit_U(unsigned int U, int loc);
void UT_setbit_U(unsigned int * U, int loc, int bit);

/* Prototypes */
#define  READ_FUNCTION( fname )  static int fname(struct one_wire_query * owq)
#define  WRITE_FUNCTION( fname )  static int fname(struct one_wire_query * owq)
#define  VISIBLE_FUNCTION( fname )  static enum e_visibility fname(const struct parsedname * pn);

int TestConnection(const struct parsedname *pn);

/* Pasename processing -- URL/path comprehension */
int filetype_cmp(const void *name, const void *ex);
int FS_ParsedName(const char *fn, struct parsedname *pn);
int FS_ParsedNamePlus(const char *path, const char *file, struct parsedname *pn);

int FS_ParsedNamePlusExt(const char *path, const char *file, int extension, enum ag_index alphanumeric, struct parsedname *pn);
int FS_ParsedNamePlusText(const char *path, const char *file, const char *extension, struct parsedname *pn);

void FS_ParsedName_destroy(struct parsedname *pn);
int FS_read_postparse(struct one_wire_query *owq);

size_t FileLength(const struct parsedname *pn);
size_t FullFileLength(const struct parsedname *pn);
int CheckPresence(struct parsedname *pn);
int ReCheckPresence(struct parsedname *pn);

void FS_devicename(char *buffer, const size_t length, const uint8_t *sn, const struct parsedname *pn);
void FS_devicefind(const char *code, struct parsedname *pn);
struct device * FS_devicefindhex(uint8_t f, struct parsedname *pn);

const char *FS_DirName(const struct parsedname *pn);

/* Utility functions */
uint8_t CRC8(const uint8_t * bytes, const size_t length);
uint8_t CRC8seeded(const uint8_t * bytes, const size_t length, const unsigned int seed);
uint8_t CRC8compute(const uint8_t * bytes, const size_t length, const unsigned int seed);
int CRC16(const uint8_t * bytes, const size_t length);
uint16_t CRC16compute(const uint8_t * bytes, const size_t length, const unsigned int seed);
int CRC16seeded(const uint8_t * bytes, const size_t length, const unsigned int seed);
uint8_t char2num(const char *s);
uint8_t string2num(const char *s);
char num2char(const uint8_t n);
void num2string(char *s, const uint8_t n);
void string2bytes(const char *str, uint8_t * b, const int bytes);
void bytes2string(char *str, const uint8_t * b, const int bytes);

void FS_LoadDirectoryOnly(struct parsedname *pn_directory, const struct parsedname *pn_original);

/* High-level callback functions */
int FS_dir(void (*dirfunc) (void *, const struct parsedname *), void *v, struct parsedname *pn);

int FS_write(const char *path, const char *buf, const size_t size, const off_t offset);
int FS_write_postparse(struct one_wire_query *owq);
int FS_write_local(struct one_wire_query *owq);

int FS_read(const char *path, char *buf, const size_t size, const off_t offset);
int FS_read_postparse(struct one_wire_query *owq);
int FS_read_fake(struct one_wire_query *owq);
int FS_read_tester(struct one_wire_query *owq);
int FS_r_aggregate_all(struct one_wire_query *owq);
int FS_read_local( struct one_wire_query *owq);

int FS_fstat(const char *path, struct stat *stbuf);
int FS_fstat_postparse(struct stat *stbuf, const struct parsedname *pn);

#endif							/* OW_FUNCTION_H */
