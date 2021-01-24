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

/* Utility functions */
uint8_t CRC8(const uint8_t * bytes, const size_t length);
uint8_t CRC8seeded(const uint8_t * bytes, const size_t length, const unsigned int seed);
uint8_t CRC8compute(const uint8_t * bytes, const size_t length, const unsigned int seed);
int CRC16(const uint8_t * bytes, const size_t length);
uint16_t CRC16compute(const uint8_t * bytes, const size_t length, const unsigned int seed);
int CRC16seeded(const uint8_t * bytes, const size_t length, const unsigned int seed);

int UT_getbit(const uint8_t * buf, int loc);
void UT_setbit(uint8_t * buf, int loc, int bit);

int UT_getbit_U(unsigned int U, int loc);
void UT_setbit_U(unsigned int * U, int loc, int bit);

#endif							/* OW_FUNCTION_H */
