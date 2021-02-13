/*
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

    Function naming scheme:
    OW -- Generic call to interaface
    LI -- LINK commands
    L1 -- 2480B commands
    FS -- filesystem commands
    UT -- utility functions

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
    Copyright (C) 2000 Dallas Semiconductor Corporation, All Rights Reserved.
        Permission is hereby granted, free of charge, to any person obtaining a
        copy of this software and associated documentation files (the "Software"),
        to deal in the Software without restriction, including without limitation
        the rights to use, copy, modify, merge, publish, distribute, sublicense,
        and/or sell copies of the Software, and to permit persons to whom the
        Software is furnished to do so, subject to the following conditions:
        The above copyright notice and this permission notice shall be included
        in all copies or substantial portions of the Software.
    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY,  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL DALLAS SEMICONDUCTOR BE LIABLE FOR ANY CLAIM, DAMAGES
    OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
        Except as contained in this notice, the name of Dallas Semiconductor
        shall not be used except as stated in the Dallas Semiconductor
        Branding Policy.
    ---------------------------------------------------------------------------
    Implementation:
    25-05-2003 iButtonLink device
*/

#ifndef OW_H					/* tedious wrapper */
#define OW_H

/* For everything */
#define _GNU_SOURCE 1

#include <stdio.h> // for getline
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>				/* for bit twiddling */
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h> // for offsetof(), size_t
#include <string.h>

/* Debugging and error messages separated out for readability */
#include "ow_debug.h"
#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

/* Some errnos are not defined for MacOSX and gcc3.3 or openbsd */
#ifndef EBADMSG
#define EBADMSG ENOMSG
#endif							/* EBADMSG */

#ifndef EPROTO
#define EPROTO EIO
#endif							/* EPROTO */

#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif							/* ENOTSUP */

#define owcalloc(nmemb,size)  calloc(nmemb,size)
#define owmalloc(size)        malloc(size)
#define owfree(ptr)           free(ptr)
#define owstrdup(s)           strdup(s)

#define SAFEFREE(p)    do { if ( (p)!= NULL ) { owfree(p) ; p=NULL; } } while (0)
//#define SAFETDESTROY(p,f) do { if ( (p)!=NULL ) { tdestroy(p,f) ; p=NULL; } } while (0)

/* Bytes in a 1-wire address */
#define SERIAL_NUMBER_SIZE           8

/* Define our understanding of function returns ... */
#include "ow_localreturns.h"

/* Maximum length of a file or directory name, and extension */
#define OW_NAME_MAX      (32)
#define OW_EXT_MAX       (6)
#define OW_FULLNAME_MAX  (OW_NAME_MAX+OW_EXT_MAX)
#define OW_DEFAULT_LENGTH (128)
#include "ow_filetype.h"

/* -------------------------------- */
/* Devices -- types of 1-wire chips */
/*                                  */
#include "ow_device.h"

/* device display format */
enum deviceformat { fdi, fi, fdidc, fdic, fidc, fic };

/* Parsedname -- path converted into components */
#include "ow_parsedname.h"

/* "Object-type" structure for the anctual owfs query --
  holds name, flags, values, and path */
#include "ow_onewirequery.h"


/* Globals information (for local control) */
/* Separated out into ow_global.h for readability */
#include "ow_global.h"

#include "ow_functions.h"

/* Return and error codes */
/* Set return code into an integer */
#if 0
#define RETURN_CODE_SET_SCALAR(i,rc)    return_code_set_scalar( rc, &(i), __FILE__, __LINE__, __func__ )
/* Unconditional return with return code */
#define RETURN_CODE_RETURN(rc)	do { int i ; RETURN_CODE_SET_SCALAR(i,rc) ; return -i; } while(0) ;
#endif

#define UCLIBCLOCK			do { } while(0)
#define UCLIBCUNLOCK		do { } while(0)

#define BUS_RESET_OK    0
#define BUS_RESET_SHORT 1
#define BUS_RESET_ERROR -EINVAL

/* 1-wire ROM commands */
#define _1W_READ_ROM               0x33
#define _1W_OLD_READ_ROM           0x0F
#define _1W_MATCH_ROM              0x55
#define _1W_SKIP_ROM               0xCC
#define _1W_SEARCH_ROM             0xF0
#define _1W_CONDITIONAL_SEARCH_ROM 0xEC
#define _1W_RESUME                 0xA5
#define _1W_OVERDRIVE_SKIP_ROM     0x3C
#define _1W_OVERDRIVE_MATCH_ROM    0x69
#define _1W_SWAP                   0xAA	// used by the DS27xx
#define _1W_LOCATOR                0x00 // used by the Link Locator

#endif							/* OW_H */
