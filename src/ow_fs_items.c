/*
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: paul.alfille@gmail.com
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

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

#include "ow.h"
#include "ow_standard.h"

/* ------- Prototypes ------------ */

/* ------- Functions ow_fs_address ------------ */

ZERO_OR_ERROR FS_address(struct one_wire_query *owq)
{
	char ad[SERIAL_NUMBER_SIZE*2];
	struct parsedname *pn = PN(owq);
	bytes2string(ad, pn->sn, SERIAL_NUMBER_SIZE);
	return OWQ_format_output_offset_and_size(ad, SERIAL_NUMBER_SIZE*2, owq);
}

ZERO_OR_ERROR FS_r_address(struct one_wire_query *owq)
{
	int sn_index, ad_index;
	char ad[SERIAL_NUMBER_SIZE*2];
	struct parsedname *pn = PN(owq);
	for (sn_index = SERIAL_NUMBER_SIZE-1, ad_index = 0; sn_index >= 0; --sn_index, ad_index += 2) {
		num2string(&ad[ad_index], pn->sn[sn_index]);
	}
	return OWQ_format_output_offset_and_size(ad, SERIAL_NUMBER_SIZE*2, owq);
}

/* ------- Functions ow_fs_id ------------ */

ZERO_OR_ERROR FS_ID(struct one_wire_query *owq)
{
	char id[12];
	struct parsedname *pn = PN(owq);
	bytes2string(id, &(pn->sn[1]), 6);
	return OWQ_format_output_offset_and_size(id, 12, owq);
}

ZERO_OR_ERROR FS_r_ID(struct one_wire_query *owq)
{
	int sn_index, id_index;
	char id[12];
	struct parsedname *pn = PN(owq);
	for (sn_index = 6, id_index = 0; sn_index > 0; --sn_index, id_index += 2) {
		num2string(&id[id_index], pn->sn[sn_index]);
	}
	return OWQ_format_output_offset_and_size(id, 12, owq);
}

/* ------- Functions ow_fs_crc ------------ */

ZERO_OR_ERROR FS_crc8(struct one_wire_query *owq)
{
	char crc[2];
	struct parsedname *pn = PN(owq);
	num2string(crc, pn->sn[7]);
	return OWQ_format_output_offset_and_size(crc, 2, owq);
}

/* ------- Functions ow_fs_code ------------ */

ZERO_OR_ERROR FS_code(struct one_wire_query *owq)
{
	char code[2];
	struct parsedname *pn = PN(owq);
	num2string(code, pn->sn[0]);
	return OWQ_format_output_offset_and_size(code, 2, owq);
}

/* ------- Functions ow_fs_alias ------------ */

ZERO_OR_ERROR FS_r_alias(struct one_wire_query *owq)
{
#if 0
	uint8_t * sn = OWQ_pn(owq).sn ;
	char * alias_name = Cache_Get_Alias( sn ) ;

	if ( alias_name != NULL ) {
		ZERO_OR_ERROR zoe = OWQ_format_output_offset_and_size_z(alias_name, owq);
		LEVEL_DEBUG("Found alias %s for "SNformat,alias_name,SNvar(sn));
		owfree( alias_name ) ;
		return zoe;
	}

	LEVEL_DEBUG("Didn't find alias %s for "SNformat,alias_name,SNvar(sn));
#endif
	return OWQ_format_output_offset_and_size_z("", owq);
}

ZERO_OR_ERROR FS_w_alias(struct one_wire_query *owq)
{
#if 0
	size_t size = OWQ_size(owq) ;
	char * alias_name = owmalloc( size+1 ) ;
	GOOD_OR_BAD gob ;

	if ( alias_name == NULL ) {
		return -ENOMEM ;
	}

	// make a slightly larger buffer and add a null terminator
	memset( alias_name, 0, size+1 ) ;
	memcpy( alias_name, OWQ_buffer(owq), size ) ;

	gob = Test_and_Add_Alias( alias_name, OWQ_pn(owq).sn ) ;

	owfree( alias_name ) ;
	return GOOD(gob) ? 0 : -EINVAL ;
#else
	return 0;
#endif
}

/* ------- Functions ow_fs_type ------------ */

ZERO_OR_ERROR FS_type(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);
	return OWQ_format_output_offset_and_size_z(pn->selected_device->readable_name, owq);
}
