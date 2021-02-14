#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "i2c-dev.h"
#include "ow.h"
#include "ow_search.h"
#include "ow_functions.h"
#include "ow_connection.h"

/**
 * The DS2482 registers - there are 3 registers that are addressed by a read
 * pointer. The read pointer is set by the last command executed.
 *
 * To read the data, issue a register read for any address
 */
#define DS2482_CMD_RESET               0xF0	/* No param */
#define DS2482_CMD_SET_READ_PTR        0xE1	/* Param: DS2482_PTR_CODE_xxx */
#define DS2482_CMD_CHANNEL_SELECT      0xC3	/* Param: Channel byte - DS2482-800 only */
#define DS2482_CMD_WRITE_CONFIG        0xD2	/* Param: Config byte */
#define DS2482_CMD_1WIRE_RESET         0xB4	/* Param: None */
#define DS2482_CMD_1WIRE_SINGLE_BIT    0x87	/* Param: Bit byte (bit7) */
#define DS2482_CMD_1WIRE_WRITE_BYTE    0xA5	/* Param: Data byte */
#define DS2482_CMD_1WIRE_READ_BYTE     0x96	/* Param: None */
/* Note to read the byte, Set the ReadPtr to Data then read (any addr) */
#define DS2482_CMD_1WIRE_TRIPLET       0x78	/* Param: Dir byte (bit7) */

/* Values for DS2482_CMD_SET_READ_PTR */
#define DS2482_STATUS_REGISTER         		 0xF0
#define DS2482_READ_DATA_REGISTER            0xE1
#define DS2482_DEVICE_CONFIGURATION_REGISTER 0xC3
#define DS2482_CHANNEL_SELECTION_REGISTER    0xD2	/* DS2482-800 only */
#define DS2482_PORT_CONFIGURATION_REGISTER   0xB4   /* DS2483 only */

/**
 * Configure Register bit definitions
 * The top 4 bits always read 0.
 * To write, the top nibble must be the 1's compl. of the low nibble.
 */
#define DS2482_REG_CFG_1WS     0x08
#define DS2482_REG_CFG_SPU     0x04
#define DS2482_REG_CFG_PDN     0x02 /* DS2483 only, power down */
#define DS2482_REG_CFG_PPM     0x02 /* non-DS2483, presence pulse masking */
#define DS2482_REG_CFG_APU     0x01

/**
 * Status Register bit definitions (read only)
 */
#define DS2482_REG_STS_DIR     0x80
#define DS2482_REG_STS_TSB     0x40
#define DS2482_REG_STS_SBR     0x20
#define DS2482_REG_STS_RST     0x10
#define DS2482_REG_STS_LL      0x08
#define DS2482_REG_STS_SD      0x04
#define DS2482_REG_STS_PPD     0x02
#define DS2482_REG_STS_1WB     0x01

/* Time limits for communication
    unsigned long int min_usec, unsigned long int max_usec */
#define DS2482_Chip_reset_usec   1, 2
#define DS2482_1wire_reset_usec   1125, 1250
#define DS2482_1wire_write_usec   530, 585
#define DS2482_1wire_triplet_usec   198, 219

static int CreateChannels(struct connection_in *head);
int DS2482_channel_select(struct connection_in * in);

/* All the rest of the program sees is the DS2482_detect and the entry in iroutines */
/* Open a DS2482 */
/* Top level detect routine */

int DS2482_detect(struct port_in *pin)
{
	int adr = 0x18;
	int fd;

	fd = open("/dev/i2c-0", O_RDWR);
	if (fd < 0) {
		printf("I2c device open error (%s)\n", strerror(errno));
		return -1;
	}
	if (ioctl(fd, I2C_SLAVE, adr) < 0) {
		printf("Cound not set trial i2c address to %.2X\n", adr);
		close(fd);
		return -1;
	}

	LEVEL_CALL("Found an i2c device address %.2X (fd=%d)", adr, fd);
	/* Provisional setup as a DS2482-100 ( 1 channel ) */
	struct connection_in * in = pin->first ;

	in->pown->file_descriptor = fd;
	/*pin->state = cs_deflowered;
	pin->type = ct_i2c; */
#if 0
	// for now first only 800 is supported, should check:
	if ( BAD(DS2482_channel_select(head)) ) {	/* Couldn't switch */
		...
		head->master.i2c.type = ds2482_100 ;
#endif
	/* Must be a DS2482-800 */
	in->master.channels = 8;
	in->master.type = ds2482_800 ;
	in->Adapter = adapter_DS2482_800;

	return CreateChannels(in);
}

/* Is this a DS2483? Try to set to new register */
int DS2482_channel_select(struct connection_in * in)
{
	int chan = in->master.index;
	int fd = in->pown->file_descriptor;
	int read_back;

	/*
		Write and verify codes for the CHANNEL_SELECT command (DS2482-800 only).
		To set the channel, write the value at the index of the channel.
		Read and compare against the corresponding value to verify the change.
	*/
	static const uint8_t W_chan[8] = { 0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87 };
	static const uint8_t R_chan[8] = { 0xB8, 0xB1, 0xAA, 0xA3, 0x9C, 0x95, 0x8E, 0x87 };

	if (fd < 0) {
		LEVEL_DEBUG("Invalid FD");
		return gbBAD;
	}
	/* Select command */
	if (i2c_smbus_write_byte_data(fd, DS2482_CMD_CHANNEL_SELECT, W_chan[chan]) < 0) {
		LEVEL_DEBUG("Channel select set error");
		return gbBAD;
	}

	/* Read back and confirm */
	read_back = i2c_smbus_read_byte(fd);
	if (read_back < 0) {
		LEVEL_DEBUG("Channel select get error");
		return gbBAD; // flag for DS2482-100 vs -800 detection
	}
	if (((uint8_t)read_back) != R_chan[chan]) {
		LEVEL_DEBUG("Channel selected doesn't match");
		return gbBAD; // flag for DS2482-100 vs -800 detection
	}

	LEVEL_DEBUG ("good:  %X", read_back);
	return gbGOOD;
}

/* read status register */
/* should already be set to read from there */
/* will read at min time, avg time, max time, and another 50% */
/* returns 0 good, 1 bad */
/* tests to make sure bus not busy */
static GOOD_OR_BAD DS2482_readstatus(uint8_t * c, int fd, unsigned long int min_usec, unsigned long int max_usec)
{
	unsigned long int delta_usec = (max_usec - min_usec + 1) / 2;
	int i = 0;
	usleep(min_usec);		// at least get minimum out of the way
	do {
		int ret = i2c_smbus_read_byte(fd);
		if (ret < 0) {
			LEVEL_DEBUG("problem min=%lu max=%lu i=%d ret=%d", min_usec, max_usec, i, ret);
			return gbBAD;
		}
		if ((ret & DS2482_REG_STS_1WB) == 0x00) {
			c[0] = (uint8_t) ret;
			//LEVEL_DEBUG("ok");
			return gbGOOD;
		}
		if (i++ == 3) {
			LEVEL_DEBUG("still busy min=%lu max=%lu i=%d ret=%d", min_usec, max_usec, i, ret);
			return gbBAD;
		}
		usleep(delta_usec);	// increment up to three times
	} while (1);
}

/* DS2482 Reset -- A little different from DS2480B */
// return 1 shorted, 0 ok, <0 error
int DS2482_reset(const struct parsedname *pn)
{
	uint8_t status_byte;
	int AnyDevices;
	struct connection_in *in = pn->selected_connection;
	int fd = in->pown->file_descriptor;

	/* Make sure we're using the correct channel */
	if (DS2482_channel_select(in) != 0)
		return BUS_RESET_ERROR;

	/* write the RESET code */
	if (i2c_smbus_write_byte(fd, DS2482_CMD_1WIRE_RESET)) {
		return BUS_RESET_ERROR;
	}

	/* wait */
	// rstl+rsth+.25 usec

	/* read status */
	if (BAD( DS2482_readstatus(&status_byte, fd, DS2482_1wire_reset_usec) ) ) {
		return BUS_RESET_ERROR;			// 8 * Tslot
	}

	AnyDevices = (status_byte & DS2482_REG_STS_PPD) ? 1 : 0;
	LEVEL_DEBUG("DS2482 Any devices found on reset? %s",AnyDevices==1?"Yes":"No");

	return (status_byte & DS2482_REG_STS_SD) ? BUS_RESET_SHORT : BUS_RESET_OK;
}

/* Single byte -- assumes channel selection already done */
GOOD_OR_BAD DS2482_send(int fd, const uint8_t wr)
{
	uint8_t c;

	/* Write data byte */
	if (i2c_smbus_write_byte_data(fd, DS2482_CMD_1WIRE_WRITE_BYTE, wr) < 0)
		return gbBAD;

	/* read status for done */
	RETURN_BAD_IF_BAD(DS2482_readstatus(&c, fd, DS2482_1wire_write_usec)) ;

	return gbGOOD;
}


/* Single byte -- assumes channel selection already done */
static GOOD_OR_BAD DS2482_send_and_get(int fd, const uint8_t wr, uint8_t * rd)
{
	int read_back;
	uint8_t c;

	/* Write data byte */
	if (i2c_smbus_write_byte_data(fd, DS2482_CMD_1WIRE_WRITE_BYTE, wr) < 0) {
		return gbBAD;
	}

	/* read status for done */
	RETURN_BAD_IF_BAD(DS2482_readstatus(&c, fd, DS2482_1wire_write_usec)) ;

	/* Select the data register */
	if (i2c_smbus_write_byte_data(fd, DS2482_CMD_SET_READ_PTR, DS2482_READ_DATA_REGISTER) < 0) {
		return gbBAD;
	}

	/* Read the data byte */
	read_back = i2c_smbus_read_byte(fd);

	if (read_back < 0) {
		return gbBAD;
	}
	rd[0] = (uint8_t) read_back;

	return gbGOOD;
}

static GOOD_OR_BAD DS2482_triple(uint8_t * bits, int direction, int fd)
{
	/* 3 bits in bits */
	uint8_t c;

	//LEVEL_DEBUG("-> TRIPLET attempt direction %d", direction);
	/* Write TRIPLE command */
	if (i2c_smbus_write_byte_data(fd, DS2482_CMD_1WIRE_TRIPLET, direction ? 0xFF : 0) < 0) {
		return gbBAD;
	}

	/* read status */
	RETURN_BAD_IF_BAD(DS2482_readstatus(&c, fd, DS2482_1wire_triplet_usec)) ;

	bits[0] = (c & DS2482_REG_STS_SBR) != 0;
	bits[1] = (c & DS2482_REG_STS_TSB) != 0;
	bits[2] = (c & DS2482_REG_STS_DIR) != 0;
	//LEVEL_DEBUG("<- TRIPLET %d %d %d", bits[0], bits[1], bits[2]);
	return gbGOOD;
}

/* uses the "Triple" primative for faster search */
enum search_status DS2482_next_both(struct device_search *ds, const struct parsedname *pn)
{
	int search_direction = 0;	/* initialization just to forestall incorrect compiler warning */
	int bit_number;
	int last_zero = -1;
	uint8_t bits[3];
	struct connection_in *in = pn->selected_connection;
	int fd = in->pown->file_descriptor;

	if (in == NULL || fd < 0) {
		LEVEL_DEBUG("error");
		return search_error;
	}
	// initialize for search
	// if the last call was not the last one
	if (ds->LastDevice) {
		return search_done;
	}
	/* Make sure we're using the correct channel */
	if (DS2482_channel_select(in) != 0)
		return search_error;

	DS2482_reset(pn);
	/* Make sure we're using the correct channel */
	/* Appropriate search command */
	DS2482_send(fd, ds->search);

	// loop to do the search
	for (bit_number = 0; bit_number < 64; ++bit_number) {
		//LEVEL_DEBUG("bit number %d", bit_number);
		/* Set the direction bit */
		if (bit_number < ds->LastDiscrepancy) {
			search_direction = UT_getbit(ds->sn, bit_number);
		} else {
			search_direction = (bit_number == ds->LastDiscrepancy) ? 1 : 0;
		}
		/* Appropriate search command */
		if (BAD(DS2482_triple(bits, search_direction, fd)))  {
			LEVEL_DEBUG("error");
			return search_error;
		}
		if (bits[0] || bits[1] || bits[2]) {
			if (bits[0] && bits[1]) {	/* 1,1 */
				/* No devices respond */
				ds->LastDevice = 1;
				return search_done;
			}
		} else {				/* 0,0,0 */
			last_zero = bit_number;
		}
		UT_setbit(ds->sn, bit_number, bits[2]);
	}							// loop until through serial number bits

	if (CRC8(ds->sn, SERIAL_NUMBER_SIZE) || (bit_number < 64) || (ds->sn[0] == 0)) {
		/* Unsuccessful search or error -- possibly a device suddenly added */
		return search_error;
	}
	// if the search was successful then
	ds->LastDiscrepancy = last_zero;
	ds->LastDevice = (last_zero < 0);
	LEVEL_DEBUG("SN found: " SNformat, SNvar(ds->sn));
	return search_good;
}

/* create more channels,
   inserts in connection_in chain
   "in" points to  first (head) channel
   called only for DS12482-800
   NOTE: coded assuming num = 1 or 8 only
 */
static int CreateChannels(struct connection_in *head)
{
	int i;
	char *name[] = { "DS2482-800(0)", "DS2482-800(1)", "DS2482-800(2)",
		"DS2482-800(3)", "DS2482-800(4)", "DS2482-800(5)", "DS2482-800(6)",
		"DS2482-800(7)",
	};
	head->master.index = 0;
	head->adapter_name = name[0];
	for (i = 1; i < 8; ++i) {
		struct connection_in * added = AddtoPort(head->pown);
		if (added == NO_CONNECTION) {
			return -1;
		}
		added->master.index = i;
		added->adapter_name = name[i];
	}

	return 0;
}

