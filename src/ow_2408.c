/*
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: paul.alfille@gmail.com
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

// regex

/* General Device File format:
    This device file corresponds to a specific 1wire/iButton chip type
    ( or a closely related family of chips )

    The connection to the larger program is through the "device" data structure,
      which must be declared in the acompanying header file.

    The device structure holds the
      family code,
      name,
      device type (chip, interface or pseudo)
      number of properties,
      list of property structures, called "filetype".

    Each filetype structure holds the
      name,
      estimated length (in bytes),
      aggregate structure pointer,
      data format,
      read function,
      write funtion,
      generic data pointer

    The aggregate structure, is present for properties that several members
    (e.g. pages of memory or entries in a temperature log. It holds:
      number of elements
      whether the members are lettered or numbered
      whether the elements are stored together and split, or separately and joined
*/

/* LCD drivers, two designs
   Maxim / AAG uses 7 PIO pins
   based on Public domain code from Application Note 3286

   Hobby-Boards by Eric Vickery
   Paul,

Go right ahead and use it for whatever you want. I just provide it as an
example for people who are using the LCD Driver.

It originally came from an application that I was working on (and may
again) but that particular code is in the public domain now.

Let me know if you have any other questions.

Eric
*/

#include "ow_2408.h"

/* ------- Prototypes ----------- */

/* DS2408 switch */
READ_FUNCTION(FS_r_pio);
WRITE_FUNCTION(FS_w_pio);
READ_FUNCTION(FS_r_latch);
WRITE_FUNCTION(FS_w_latch);
READ_FUNCTION(FS_r_s_alarm);
WRITE_FUNCTION(FS_w_s_alarm);
#if 0
READ_FUNCTION(FS_sense);
READ_FUNCTION(FS_power);
WRITE_FUNCTION(FS_out_of_testmode);
READ_FUNCTION(FS_r_por);
WRITE_FUNCTION(FS_w_por);
#endif

/* ------- Structures ----------- */

static struct aggregate A2408 = { 2, ag_numbers, ag_aggregate, };
static struct aggregate A2408l = { 8, ag_numbers, ag_separate, };

static struct filetype DS2408[] = {
	F_STANDARD,
	//{"power", PROPERTY_LENGTH_YESNO, NON_AGGREGATE, ft_yesno, fc_volatile, FS_power, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA, },
	//{"out_of_testmode", PROPERTY_LENGTH_YESNO, NON_AGGREGATE, ft_yesno, fc_volatile, NO_READ_FUNCTION,  FS_out_of_testmode, VISIBLE, NO_FILETYPE_DATA, },
	{"PIO", PROPERTY_LENGTH_BITFIELD, &A2408, ft_bitfield, fc_stable, FS_r_pio, FS_w_pio, VISIBLE, NO_FILETYPE_DATA, },
	//{"sensed", PROPERTY_LENGTH_BITFIELD, &A2408, ft_bitfield, fc_volatile, FS_sense, NO_WRITE_FUNCTION, VISIBLE, NO_FILETYPE_DATA, },
	{"latch", PROPERTY_LENGTH_BITFIELD, &A2408l, ft_bitfield, fc_volatile, FS_r_latch, FS_w_latch, VISIBLE, NO_FILETYPE_DATA, },
	{"set_alarm", PROPERTY_LENGTH_UNSIGNED, NON_AGGREGATE, ft_unsigned, fc_stable, FS_r_s_alarm, FS_w_s_alarm, VISIBLE, NO_FILETYPE_DATA, },
	//{"por", PROPERTY_LENGTH_YESNO, NON_AGGREGATE, ft_yesno, fc_stable, FS_r_por, FS_w_por, VISIBLE, NO_FILETYPE_DATA, },
};

DeviceEntryExtended(29, DS2408, DEV_alarm | DEV_resume | DEV_ovdr, NO_GENERIC_READ, NO_GENERIC_WRITE);

#define _1W_READ_PIO_REGISTERS  0xF0
#define _1W_CHANNEL_ACCESS_READ 0xF5
#define _1W_CHANNEL_ACCESS_WRITE 0x5A
#define _1W_WRITE_CONDITIONAL_SEARCH_REGISTER 0xCC
#define _1W_RESET_ACTIVITY_LATCHES 0xC3

#define _ADDRESS_PIO_LOGIC_STATE 0x0088
#define _ADDRESS_ALARM_REGISTERS 0x008B
#define _ADDRESS_CONTROL_STATUS_REGISTER 0x008D

/* ------- Functions ------------ */

/* DS2408 */
static GOOD_OR_BAD OW_w_control(const uint8_t data, const struct parsedname *pn);
static GOOD_OR_BAD OW_c_latch(const struct parsedname *pn);
static GOOD_OR_BAD OW_w_pio(const uint8_t data, const struct parsedname *pn);
static GOOD_OR_BAD OW_r_reg(uint8_t * data, const struct parsedname *pn);
static GOOD_OR_BAD OW_w_s_alarm(const uint8_t * data, const struct parsedname *pn);
//static GOOD_OR_BAD OW_w_pios(const uint8_t *data, const size_t size, const uint8_t verify_mask, const struct parsedname *pn);
//static GOOD_OR_BAD OW_out_of_test_mode( const struct parsedname * pn ) ;

#if 0
/* 2408 switch */
/* 2408 switch -- is Vcc powered?*/
static ZERO_OR_ERROR FS_power(struct one_wire_query *owq)
{
	uint8_t data[6];
	RETURN_ERROR_IF_BAD( OW_r_reg(data, PN(owq)) );
	OWQ_Y(owq) = UT_getbit(&data[5], 7);
	return 0;
}

static ZERO_OR_ERROR FS_out_of_testmode(struct one_wire_query *owq)
{
	if ( OWQ_Y(owq) ) {
		RETURN_ERROR_IF_BAD( OW_out_of_test_mode(PN(owq) ) );
	}
	return 0;
}
#endif

/* 2408 switch PIO set*/
/* From register 0x89 */
static ZERO_OR_ERROR FS_r_pio(struct one_wire_query *owq)
{
	uint8_t data[6];
	RETURN_ERROR_IF_BAD( OW_r_reg(data, PN(owq)) ) ;
	OWQ_U(owq) = BYTE_INVERSE(data[1]);	/* reverse bits */
	return 0;
}

/* 2408 switch PIO change*/
static ZERO_OR_ERROR FS_w_pio(struct one_wire_query *owq)
{
	uint8_t data = BYTE_INVERSE(OWQ_U(owq)) & 0xFF ;   /* reverse bits */

	return GB_to_Z_OR_E(OW_w_pio(data, PN(owq))) ;
}

/* 2408 read activity latch */
/* From register 0x8A */
static ZERO_OR_ERROR FS_r_latch(struct one_wire_query *owq)
{
	uint8_t data[6];
	RETURN_ERROR_IF_BAD( OW_r_reg(data, PN(owq)) );
	OWQ_U(owq) = data[2];
	return 0;
}

/* 2408 write activity latch */
/* Actually resets them all */
static ZERO_OR_ERROR FS_w_latch(struct one_wire_query *owq)
{
	return GB_to_Z_OR_E(OW_c_latch(PN(owq))) ;
}

/* 2408 alarm settings*/
/* From registers 0x8B-0x8D */
static ZERO_OR_ERROR FS_r_s_alarm(struct one_wire_query *owq)
{
	uint8_t d[6];
	int i, p;
	unsigned int U;
	RETURN_ERROR_IF_BAD( OW_r_reg(d, PN(owq)) );
	/* register 0x8D */
	U = (d[5] & 0x03) * 100000000;
	/* registers 0x8B and 0x8C */
	for (i = 0, p = 1; i < 8; ++i, p *= 10) {
		U += (UT_getbit(&d[4], i) | (UT_getbit(&d[3], i) << 1)) * p;
	}
	OWQ_U(owq) = U;
	return 0;
}

/* 2408 alarm settings*/
/* First digit source and logic data[2] */
/* next 8 channels */
/* data[1] polarity */
/* data[0] selection  */
static ZERO_OR_ERROR FS_w_s_alarm(struct one_wire_query *owq)
{
	uint8_t data[3] = { 0, 0, 0, }; // coverity likes this initialized
	int i;
	unsigned int p;
	unsigned int U = OWQ_U(owq);
	for (i = 0, p = 1; i < 8; ++i, p *= 10) {
		UT_setbit(&data[1], i, ((int) (U / p) % 10) & 0x01);
		UT_setbit(&data[0], i, (((int) (U / p) % 10) & 0x02) >> 1);
	}
	data[2] = ((U / 100000000) % 10) & 0x03;
	return GB_to_Z_OR_E(OW_w_s_alarm(data, PN(owq))) ;
}

#if 0
static ZERO_OR_ERROR FS_r_por(struct one_wire_query *owq)
{
	uint8_t data[6];
	RETURN_ERROR_IF_BAD( OW_r_reg(data, PN(owq)) );
	OWQ_Y(owq) = UT_getbit(&data[5], 3);
	return 0;
}

static ZERO_OR_ERROR FS_w_por(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);
	uint8_t data[6];
	RETURN_ERROR_IF_BAD( OW_r_reg(data, pn) );
	UT_setbit(&data[5], 3, OWQ_Y(owq));
	return GB_to_Z_OR_E( OW_w_control(data[5], pn) ) ;
}
#endif

/* Read 6 bytes --
   0x88 PIO logic State
   0x89 PIO output Latch state
   0x8A PIO Activity Latch
   0x8B Conditional Ch Mask
   0x8C Londitional Ch Polarity
   0x8D Control/Status
   plus 2 more bytes to get to the end of the page and qualify for a CRC16 checksum
*/
static GOOD_OR_BAD OW_r_reg(uint8_t * data, const struct parsedname *pn)
{
	uint8_t p[3 + 8 + 2] = { _1W_READ_PIO_REGISTERS,
		LOW_HIGH_ADDRESS(_ADDRESS_PIO_LOGIC_STATE),
	};
	/*
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WR_CRC16(p, 3, 8),
		TRXN_END,
	};

	RETURN_BAD_IF_BAD(BUS_transaction(t, pn)) ;
	*/
	memcpy(data, &p[3], 6);
	return gbGOOD;
}

static GOOD_OR_BAD OW_w_pio(const uint8_t data, const struct parsedname *pn)
{
	uint8_t write_string[] = { _1W_CHANNEL_ACCESS_WRITE, data, (uint8_t) ~ data, };
	uint8_t read_back[2];
	/*
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WRITE3(write_string),
		TRXN_READ2(read_back),
		TRXN_END,
	};

	if ( BAD(BUS_transaction(t, pn)) ) {
		// may be in test mode, which causes Channel Access Write to fail
		// fix now, but need another attempt to see if will work
		OW_out_of_test_mode(pn) ;
		return gbBAD ;
	}
	*/
	if (read_back[0] != 0xAA) {
		return gbBAD;
	}

	/* Ignore byte 5 read_back[1] the PIO status byte */
	return gbGOOD;
}

#if 0
/* Send several bytes to the channel, and verify that they where sent properly
 * verify_mask can be used if we do not have explicit control over all PIOs, i.e. if we don't know if they
 * are pulled up or not (device with buttons on lower 3 bits, may not be pulled up if no buttons are included)
 */
static GOOD_OR_BAD OW_w_pios(const uint8_t *data, const size_t size, const uint8_t verify_mask, const struct parsedname *pn)
{
	uint8_t cmd[] = { _1W_CHANNEL_ACCESS_WRITE, };
	size_t formatted_size = 4 * size;
	uint8_t formatted_data[formatted_size];
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WRITE1(cmd),
		TRXN_MODIFY(formatted_data, formatted_data, formatted_size),
		TRXN_END,
	};
	size_t i;

	// setup the array
	// each byte takes 4 bytes after formatting
	for (i = 0; i < size; ++i) {
		int formatted_data_index = 4 * i;
		formatted_data[formatted_data_index + 0] = data[i];
		formatted_data[formatted_data_index + 1] = (uint8_t) ~ data[i];
		formatted_data[formatted_data_index + 2] = 0xFF;
		formatted_data[formatted_data_index + 3] = 0xFF;
	}

	if ( BAD(BUS_transaction(t, pn)) ) {
		// may be in test mode, which causes Channel Access Write to fail
		// fix now, but need another attempt to see if will work
		OW_out_of_test_mode(pn) ;
		return gbBAD ;
	}

	for (i = 0; i < size; ++i) {
		int formatted_data_index = 4 * i;
		uint8_t rdata = ((uint8_t)~data[i]);  // get rid of warning: comparison of promoted ~unsigned with unsigned
		if (formatted_data[formatted_data_index + 0] != data[i]) {
			return gbBAD;
		}
		if (formatted_data[formatted_data_index + 1] != rdata) {
			return gbBAD;
		}
		if (formatted_data[formatted_data_index + 2] != 0xAA) {
			return gbBAD;
		}
		if ((formatted_data[formatted_data_index + 3] & verify_mask) != (data[i] & verify_mask)) {
			return gbBAD;
		}
	}

	return gbGOOD;
}
#endif

/* Reset activity latch */
static GOOD_OR_BAD OW_c_latch(const struct parsedname *pn)
{
	uint8_t reset_string[] = { _1W_RESET_ACTIVITY_LATCHES, };
/*
	uint8_t read_back[1];
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WRITE1(reset_string),
		TRXN_READ1(read_back),
		TRXN_END,
	};

	RETURN_BAD_IF_BAD(BUS_transaction(t, pn)) ;
	if (read_back[0] != 0xAA) {
		return gbBAD;
	}
*/
	return gbGOOD;
}

/* Write control/status */
static GOOD_OR_BAD OW_w_control(const uint8_t data, const struct parsedname *pn)
{
	uint8_t write_string[1 + 2 + 1] = { _1W_WRITE_CONDITIONAL_SEARCH_REGISTER,
		LOW_HIGH_ADDRESS(_ADDRESS_CONTROL_STATUS_REGISTER), data,
	};
	uint8_t check_string[1 + 2 + 3 + 2] = { _1W_READ_PIO_REGISTERS,
		LOW_HIGH_ADDRESS(_ADDRESS_CONTROL_STATUS_REGISTER),
	};
#if 0
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WRITE(write_string, 4),
		/* Read registers */
		TRXN_START,
		TRXN_WR_CRC16(check_string, 3, 3),
		TRXN_END,
	};

	RETURN_BAD_IF_BAD(BUS_transaction(t, pn));
#endif
	return ((data & 0x0F) != (check_string[3] & 0x0F)) ? gbBAD : gbGOOD ;
}

/* write alarm settings */
static GOOD_OR_BAD OW_w_s_alarm(const uint8_t * data, const struct parsedname *pn)
{
	uint8_t old_register[6];
	uint8_t new_register[6];
	uint8_t control_value[1];
	uint8_t alarm_access[] = { _1W_WRITE_CONDITIONAL_SEARCH_REGISTER,
		LOW_HIGH_ADDRESS(_ADDRESS_ALARM_REGISTERS),
	};
#if 0
	struct transaction_log t[] = {
		TRXN_START,
		TRXN_WRITE3(alarm_access),
		TRXN_WRITE2(data),
		TRXN_WRITE1(control_value),
		TRXN_END,
	};

	// get the existing register contents
	RETURN_BAD_IF_BAD( OW_r_reg(old_register, pn) ) ;

	control_value[0] = (data[2] & 0x03) | (old_register[5] & 0x0C);

	RETURN_BAD_IF_BAD(BUS_transaction(t, pn)) ;

	/* Re-Read registers */
	RETURN_BAD_IF_BAD(OW_r_reg(new_register, pn)) ;
#endif
	return (data[0] != new_register[3]) || (data[1] != new_register[4])
		|| (control_value[0] != (new_register[5] & 0x0F)) ? gbBAD : gbGOOD;
}

#if 0
// very strange command to get out of test mode.
// Uses a different 1-wire command
static GOOD_OR_BAD OW_out_of_test_mode( const struct parsedname * pn )
{
	uint8_t out_of_test[] = { 0x96, SNvar(pn->sn), 0x3C, } ;
	struct transaction_log t[] = {
		TRXN_RESET,
		TRXN_WRITE(out_of_test, 1 + SERIAL_NUMBER_SIZE + 1 ),
		TRXN_END,
	};
	return BUS_transaction( t, pn ) ;
}
#endif