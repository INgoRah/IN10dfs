/*
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

    Function naming scheme:
    OW -- Generic call to interaface
    LI -- LINK commands
    L1 -- 2480B commands
    FS -- filesystem commands
    UT -- utility functions

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
*/

#ifndef OW_CONNECTION_H			/* tedious wrapper */
#define OW_CONNECTION_H

/* reset types */

/* bus serach */
#include "ow_search.h"

#define I2C_FIFO_SIZE 1

/* -------------------------------------------- */
/* Inbound connections (bus masters) ---------- */
#include "ow_port_in.h"
#include "ow_connection_in.h"

/* This bug-fix/workaround function seem to be fixed now... At least on
 * the platforms I have tested it on... printf() in owserver/src/c/owserver.c
 * returned very strange result on c->busmode before... but not anymore */
#define BusIsServer(arg) 0

// mode bit flags for level
#define MODE_NORMAL                    0x00
#define MODE_STRONG5                   0x01
#define MODE_PROGRAM                   0x02
#define MODE_BREAK                     0x04

// 1Wire Bus Speed Setting Constants
#define ONEWIREBUSSPEED_REGULAR        0x00
#define ONEWIREBUSSPEED_FLEXIBLE       0x01	/* Only used for USB adapter */
#define ONEWIREBUSSPEED_OVERDRIVE      0x02

void FreeInAll(void);
void RemoveIn( struct connection_in * conn ) ;

void FreeOutAll(void);
void DelIn(struct connection_in *in);

struct connection_in *LinkIn(struct connection_in *in, struct port_in * head);

void Add_InFlight( GOOD_OR_BAD (*nomatch)(struct port_in * trial,struct port_in * existing), struct port_in * new_pin );
void Del_InFlight( GOOD_OR_BAD (*nomatch)(struct port_in * trial,struct port_in * existing), struct port_in * new_pin );

struct connection_in *find_connection_in(int nr);
int SetKnownBus( int bus_number, struct parsedname * pn) ;

int TestConnection(const struct parsedname *pn);

#endif							/* OW_CONNECTION_H */
