/*
    OW -- One-Wire filesystem
    version 0.4 7/2/2003

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

#ifndef OW_CONNECTION_IN_H			/* tedious wrapper */
#define OW_CONNECTION_IN_H

/* struct connection_in (for each bus master) as part of ow_connection.h */

// For forward references
struct connection_in;
struct port_in ;

/* -------------------------------------------- */
/* BUS-MASTER-specific routines ---------------- */


enum adapter_type {
	adapter_DS2482_100,
	adapter_DS2482_800,
};

enum e_reconnect {
	reconnect_bad = -1,
	reconnect_ok = 0,
	reconnect_error = 2,
};

enum e_anydevices {
	anydevices_no = 0 ,
	anydevices_yes ,
	anydevices_unknown ,
};

enum e_bus_stat {
	e_bus_reconnects,
	e_bus_reconnect_errors,
	e_bus_locks,
	e_bus_unlocks,
	e_bus_errors,
	e_bus_resets,
	e_bus_reset_errors,
	e_bus_short_errors,
	e_bus_program_errors,
	e_bus_pullup_errors,
	e_bus_timeouts,
	e_bus_read_errors,
	e_bus_write_errors,
	e_bus_detect_errors,
	e_bus_open_errors,
	e_bus_close_errors,
	e_bus_search_errors1,
	e_bus_search_errors2,
	e_bus_search_errors3,
	e_bus_status_errors,
	e_bus_select_errors,
	e_bus_try_overdrive,
	e_bus_failed_overdrive,
	e_bus_stat_last_marker
};


// DS2482 (i2c) hub -- 800 has 8 channels
struct master_i2c {
	int channels;
	int index;
	int i2c_address;
	enum ds248x_type { ds2482_unknown, ds2482_100, ds2482_800, ds2483, } type ;
	int i2c_index ;
	uint8_t configreg;
	uint8_t configchip;
	/* only one per chip, the bus entries for the other 7 channels point to the first one */
	int current;
	struct connection_in *head;
};

struct connection_in {
	struct connection_in *next;
	struct port_in * pown ; // pointer to port_in that owns us.
	int index; // general index number across all ports
	int channel ; // index (0-based) in this port's channels

	enum e_reconnect reconnect_state;

	unsigned int bus_stat[e_bus_stat_last_marker];

	struct timeval bus_time;

	//struct interface_routines iroutines;
	enum adapter_type Adapter;
	char *adapter_name;
	enum e_anydevices AnyDevices;
	int overdrive;
	int flex ;
	int changed_bus_settings;
	//int ds2404_found;
	int ProgramAvailable;
	size_t last_root_devs;
	//struct ds2409_hubs branch;		// ds2409 branch currently selected
			// or the special eBranch_bad and eBranch_cleared

	unsigned char remembered_sn[SERIAL_NUMBER_SIZE] ;       /* last address */

	struct master_i2c master;
};

#define NO_CONNECTION NULL

/* Defines for flow control */
#define flow_first	( (Globals.serial_hardflow) ? flow_hard : flow_none )
#define flow_second	( (Globals.serial_hardflow) ? flow_none : flow_first )

extern struct inbound_control {
	int active ; // how many "bus" entries are currently in linked list
	int next_index ; // increasing sequence number
	struct port_in * head_port ; // head of a linked list of "bus" entries
	//my_rwlock_t lock; // RW lock of linked list
} Inbound_Control ; // Single global struct -- see ow_connect.c

#endif							/* OW_CONNECTION_IN_H */
