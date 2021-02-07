/*
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: paul.alfille@gmail.com
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

#include "ow.h"

/* After parsing, but before sending to various devices. Will repeat 3 times if needed */
int FS_read_postparse(struct one_wire_query *owq)
{
}