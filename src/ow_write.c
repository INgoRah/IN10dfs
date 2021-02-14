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
#include <search.h>
#include "ow_dirblob.h"
#include "ow_memblob.h"
#include "ow_cache.h"
#include "ow_connection.h"

/* ------- Prototypes ----------- */
static int FS_w_given_bus(struct one_wire_query *owq);
static int FS_w_interface(struct one_wire_query *owq);
static int FS_w_local(struct one_wire_query *owq);
static int FS_write_owq(struct one_wire_query *owq);
static int FS_write_all( struct one_wire_query *owq_all ) ;
static int FS_write_all_bits( struct one_wire_query *owq_all );
static int FS_write_a_bit(struct one_wire_query *owq_bit);
static int FS_write_in_parts( struct one_wire_query *owq_all );
static int FS_write_a_part( struct one_wire_query *owq_part );
static int FS_write_as_bits( struct one_wire_query *owq_byte ) ;
static int FS_write_real(int depth, struct one_wire_query *owq) ;
static int FS_write_post_stats(struct one_wire_query *owq) ;
static int FS_write_post_input(struct one_wire_query *owq) ;

/* ---------------------------------------------- */
/* Filesystem callback functions                  */
/* ---------------------------------------------- */

/* Note on return values: */
/* Top level FS_write will return size if ok, else a negative number */
/* Each lower level function called will return 0 if ok, else non-zero */

/* Note on size and offset: */
/* Buffer length (and requested data) is size bytes */
/* writing should start after offset bytes in original data */
/* only binary, and ascii data support offset in single data points */
/* only binary supports offset in array data */
/* size and offset are vetted against specification data size and calls */
/*   outside of this module will not have buffer overflows */
/* I.e. the rest of owlib can trust size and buffer to be legal */

/* Format of input,
        Depends on "filetype"
        type     function    format                         Handled as
        integer  strol      decimal integer                 integer array
        unsigned strou      decimal integer                 unsigned array
        bitfield strou      decimal integer                 unsigned array
        yesno    strcmp     "0" "1" "yes" "no" "on" "off"   unsigned array
        float    strod      decimal floating point          double array
        date     strptime   "Jan 01, 1901", etc             date array
        ascii    strcpy     string without "," or null      comma-separated-strings
        binary   memcpy     fixed length binary string      binary "string"
*/


/* return size if ok, else negative */
int FS_write(const char *path, const char *buf, const size_t size, const off_t offset)
{
	int write_return;
	OWQ_allocate_struct_and_pointer(owq);

	LEVEL_CALL("path=%s size=%d offset=%d", SAFESTRING(path), (int) size, (int) offset);

	// parsable path?
	if ( OWQ_create(path, owq) != 0 ) { // for write
		return -ENOENT;
	}
	OWQ_assign_write_buffer(buf, size, offset, owq) ;
	write_return = FS_write_postparse(owq);
	OWQ_destroy(owq);
	return write_return;		/* here's where the size is used! */
}

/* return size if ok, else negative */
int FS_write_postparse(struct one_wire_query *owq)
{
	int write_or_error;
	struct parsedname *pn = PN(owq);

	if (IsDir(pn)) {
		LEVEL_DEBUG("Attempt to write to a directory.");
		return -EISDIR;			// not a file
	}

	write_or_error = FS_write_post_stats( owq ) ;

	return write_or_error;
}

/* return 0 if ok, else negative */
/* Handles 3-peat */
static int FS_write_post_stats(struct one_wire_query *owq)
{
	// Parse the data to be written
	int input_or_error = OWQ_parse_input(owq);

	if (input_or_error < 0) {
		LEVEL_DEBUG("Error interpreting input value.") ;
		return input_or_error ;
	}
	return FS_write_post_input( owq ) ;
}

/* return 0 if ok, else negative */
/* Handles 3-peat */
static int FS_write_post_input(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);

	// Write differently depending on the type of directory
	switch (pn->type) {
		case ePN_structure:
		case ePN_statistics:
		case ePN_system:
			// No writable features here
			LEVEL_DEBUG("Cannot write in this type of directory.") ;
			return -ENOTSUP;
		case ePN_real:				// ePN_real
			if (pn->selected_connection == NULL) {
				LEVEL_DEBUG("Attempt to write but no 1-wire bus master.");
				return -ENODEV;			// no buses
			} else {
				// Normal path for most writes to actual devices
				return FS_write_real(0,owq) ; // start with 0 depth
			}
		case ePN_interface:
			return FS_w_interface(owq) ;
		case ePN_root:
		default:
			return -ENOTSUP ;
	}
}

/* write to a real 1-wire device */
/* If error, try twice more */
static int FS_write_real(int depth, struct one_wire_query *owq)
{
	int write_or_error;
	struct parsedname *pn = PN(owq);
	struct filetype * ft = pn->selected_filetype ;
	int initial_bus = pn->selected_connection->index ; // current selected bus
	int rechecked_bus ;

	if ( depth > 1 ) {
		LEVEL_DEBUG("Too many bus changes for write");
		return -ENODEV ;
	}

	/* First try */
	/* in and bus_nr already set */
	write_or_error = FS_w_given_bus(owq);
	if ( write_or_error ==0 ) {
		return 0 ;
	}

	/* Second Try */
	if (SpecifiedBus(pn)) {
		// The bus number casn't be changed -- it was specified in the path
		write_or_error = FS_w_given_bus(owq);
		if ( write_or_error == 0 ) {
			return 0 ;
		}

		// The bus number casn't be changed -- it was specified in the path
		return FS_w_given_bus(owq);
	}

	/* Recheck location */
	/* if not a specified bus, relook for chip location */
	rechecked_bus = ReCheckPresence(pn) ;
	if ( rechecked_bus < 0 ) {
		// can't find the location
		return -ENOENT ;
	}

	if ( initial_bus == rechecked_bus ) {
		// try again
		write_or_error = FS_w_given_bus(owq);
		if ( write_or_error == 0 ) {
			return 0 ;
		}
		// third try
		return FS_w_given_bus(owq);
	}

	// Changed location retry everything
	LEVEL_DEBUG("Bus location changed from %d to %d\n",initial_bus,rechecked_bus);
	return FS_write_real(depth+1,owq);
}

/* Write to interface dir */
static int FS_w_interface(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);
#if 0
	if ( pn->selected_connection == NO_CONNECTION ) {
		LEVEL_DEBUG("Attempt to write to no bus for /settings");
		return -ENODEV ;
	} else if ( SpecifiedLocalBus(pn) ) {
		return FS_w_local(owq);
	} else {
		return ServerWrite(owq);
	}
#endif

	return 0;
}

/* Write now that connection is set */
static int FS_w_given_bus(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);

	if ( BAD(TestConnection(pn)) ) {
		return -ECONNABORTED;
	} else if (OWQ_pn(owq).type == ePN_real) {
		return FS_w_local(owq);
	} else if ( IsInterfaceDir(pn) ) {
		int write_or_error;
		write_or_error = FS_w_local(owq);
		return write_or_error ;
	}

	return FS_w_local(owq);
}

/* return 0 if ok */
static int FS_w_local(struct one_wire_query *owq)
{
	// Device already locked
	struct parsedname *pn = PN(owq);
	struct filetype * ft = pn->selected_filetype ;

	/* Writable? */
	if ( ft->write == NO_WRITE_FUNCTION ) {
		return -ENOTSUP;
	}

	/* Non-array? */
	if ( ft->ag == NON_AGGREGATE ) {
		LEVEL_DEBUG("Write a non-array element %s",pn->path);
		return FS_write_owq(owq);
	}

	/* array */
	switch ( ft->ag->combined ) {
		case ag_sparse:
			// avoid cache
			return (ft->write) (owq);
		case ag_aggregate:
			switch (pn->extension) {
				case EXTENSION_BYTE:
					LEVEL_DEBUG("Write an aggregate .BYTE %s",pn->path);
					return FS_write_owq(owq);
				case EXTENSION_ALL:
					LEVEL_DEBUG("Write an aggregate .ALL %s",pn->path);
					return FS_write_all(owq);
				default:
					LEVEL_DEBUG("Write an aggregate element %s",pn->path);
					return FS_write_a_part(owq) ;
			}
		case ag_mixed:
			switch (pn->extension) {
				case EXTENSION_BYTE:
					LEVEL_DEBUG("Write a mixed .BYTE %s",pn->path);
					OWQ_Cache_Del_parts(owq);
					return FS_write_owq(owq);
				case EXTENSION_ALL:
					LEVEL_DEBUG("Write a mixed .ALL %s",pn->path);
					OWQ_Cache_Del_parts(owq);
					return FS_write_all(owq);
				default:
					LEVEL_DEBUG("Write a mixed element %s",pn->path);
					OWQ_Cache_Del_ALL(owq);
					OWQ_Cache_Del_BYTE(owq);
					return FS_write_owq(owq);
			}
		case ag_separate:
			switch (pn->extension) {
				case EXTENSION_BYTE:
					LEVEL_DEBUG("Write a separate .BYTE %s",pn->path);
					return FS_write_as_bits(owq);
				case EXTENSION_ALL:
					LEVEL_DEBUG("Write a separate .ALL %s",pn->path);
					return FS_write_in_parts(owq);
				default:
					LEVEL_DEBUG("Write a separate element %s",pn->path);
					return FS_write_owq(owq);
			}
		default:
			return -ENOENT ;
	}
}

static int FS_write_owq(struct one_wire_query *owq)
{
	int write_error = (OWQ_pn(owq).selected_filetype->write) (owq);
	OWQ_Cache_Del(owq) ; // Delete anyways
	LEVEL_DEBUG("Write %s Extension %d Gives result %d",PN(owq)->path,PN(owq)->extension,write_error);
	return write_error;
}

/* Write just one field of an aggregate property -- but a property that is handled as one big object */
// Handles .n
static int FS_write_a_part( struct one_wire_query *owq_part )
{
	struct parsedname *pn = PN(owq_part);
	size_t extension = pn->extension;
	struct filetype * ft = pn->selected_filetype ;
	int z_or_e ;
	struct one_wire_query * owq_all ;

	// bitfield
	if ( ft->format == ft_bitfield ) {
		return FS_write_a_bit( owq_part ) ;
	}

	// non-bitfield
	owq_all = OWQ_create_aggregate( owq_part ) ;
	if ( owq_all == NO_ONE_WIRE_QUERY ) {
		return -ENOENT ;
	}

	// First fill the whole array with current values
	if ( FS_read_local( owq_all ) < 0 ) {
		OWQ_destroy( owq_all ) ;
		return -ENOENT ;
	}

	// Copy ascii/binary field
	switch (ft->format) {
	case ft_binary:
	case ft_ascii:
	case ft_vascii:
	case ft_alias:
		{
			size_t extension_index;
			size_t elements = ft->ag->elements;
			char *buffer_pointer = OWQ_buffer(owq_all);
			char *entry_pointer;
			char *target_pointer;

			// All prior elements
			for (extension_index = 0; extension_index < extension; ++extension) {
				// move past their buffer position
				buffer_pointer += OWQ_array_length(owq_all, extension_index);
			}

			entry_pointer = buffer_pointer; // this element's buffer start

			target_pointer = buffer_pointer + OWQ_length(owq_part); // new start next element
			buffer_pointer = buffer_pointer + OWQ_array_length(owq_all, extension); // current start next element

			// move rest of elements to new locations
			for (extension_index = extension + 1; extension_index < elements; ++extension_index ) {
				size_t this_length = OWQ_array_length(owq_all, extension_index);
				memmove(target_pointer, buffer_pointer, this_length);
				target_pointer += this_length;
				buffer_pointer += this_length;
			}

			// now move current element's buffer to location
			memmove(entry_pointer, OWQ_buffer(owq_part), OWQ_length(owq_part));
			OWQ_array_length(owq_all,extension) = OWQ_length(owq_part) ;
		}
		break;
	default:
		// Copy value field
		memcpy(&OWQ_array(owq_all)[pn->extension], &OWQ_val(owq_part), sizeof(union value_object));
		break;
	}

	// Write whole thing out
	z_or_e = FS_write_owq(owq_all);

	OWQ_destroy(owq_all);

	return z_or_e ;
}

// Write a whole aggregate array (treated as a single large value )
// handles ALL
static int FS_write_all( struct one_wire_query * owq_all )
{
	// bitfield, convert to .BYTE format and write ( and delete cache ) as BYTE.
	if ( OWQ_pn(owq_all).selected_filetype->format == ft_bitfield ) {
		return FS_write_all_bits( owq_all ) ;
	}

	return FS_write_owq( owq_all ) ;
}

/* Takes ALL to individual, no need for the cache */
// Handles: ALL
static int FS_write_in_parts( struct one_wire_query *owq_all )
{
	struct one_wire_query * owq_part = OWQ_create_separate( 0, owq_all ) ;
	struct parsedname *pn = PN(owq_all);
	size_t elements = pn->selected_filetype->ag->elements;
	size_t extension ;
	char *buffer_pointer;
	int z_or_e = 0 ;

	// Create a "single" OWQ copy to iterate with
	if ( owq_part == NO_ONE_WIRE_QUERY ) {
		return -ENOENT ;
	}

	// create a buffer for certain types
	// point to 0th element's buffer first
	buffer_pointer = OWQ_buffer(owq_all);
	size_t fileSize = FileLength(PN(owq_part));
	OWQ_offset(owq_part) = 0;

	// loop through all eloements
	for (extension = 0; extension < elements; ++extension) {
		int single_write;

		switch (pn->selected_filetype->format) {
		case ft_ascii:
		case ft_vascii:
		case ft_alias:
		case ft_binary:
			OWQ_length(owq_part) = OWQ_size(owq_part) = OWQ_array_length(owq_all,extension) ;
			OWQ_buffer(owq_part) = buffer_pointer;
			buffer_pointer += OWQ_size(owq_part);
			break;
		default:
			OWQ_size(owq_part) = fileSize;
			memcpy(&OWQ_val(owq_part), &OWQ_array(owq_all)[extension], sizeof(union value_object));
			break;
		}

		OWQ_pn(owq_part).extension = extension;
		single_write = FS_write_owq(owq_part);

		if (single_write != 0) {
			z_or_e = single_write ;
		}
	}

	return z_or_e;
}

/* Write BYTE to bits */
// handles: BYTE
static int FS_write_as_bits( struct one_wire_query *owq_byte )
{
	struct one_wire_query * owq_bit = OWQ_create_separate( 0, owq_byte ) ;
	size_t elements = OWQ_pn(owq_byte).selected_filetype->ag->elements;
	size_t extension ;
	int z_or_e = 0 ;

	if ( owq_bit == NO_ONE_WIRE_QUERY ) {
		return -ENOENT ;
	}

	for ( extension = 0 ; extension < elements ; ++extension ) {
		int z ;
		OWQ_pn(owq_bit).extension = extension ;
		OWQ_Y(owq_bit) = UT_getbit_U( OWQ_U(owq_byte), extension ) ;
		z = FS_write_owq( owq_bit ) ;
		if ( z != 0 ) {
			z_or_e = z ;
		}
	}
	OWQ_destroy( owq_bit ) ;

	return z_or_e ;
}

struct one_wire_query * ALLtoBYTE(struct one_wire_query *owq_all)
{
	struct one_wire_query * owq_byte = OWQ_create_separate( EXTENSION_BYTE, owq_all );
	size_t elements = PN(owq_all)->selected_filetype->ag->elements ;
	size_t extension ;

	if ( owq_byte == NO_ONE_WIRE_QUERY ) {
		return NO_ONE_WIRE_QUERY ;
	}

	for ( extension = 0 ; extension < elements ; ++extension ) {
		UT_setbit_U( &OWQ_U(owq_byte), extension, OWQ_array_Y(owq_all,extension) ) ;
	}
	return owq_byte ;
}

/* Write ALL to BYTE */
// Handles: ALL
static int FS_write_all_bits( struct one_wire_query *owq_all )
{
	struct one_wire_query * owq_byte = ALLtoBYTE( owq_all ) ;
	int z_or_e = -ENOENT ;

	if ( owq_byte != NO_ONE_WIRE_QUERY ) {
		z_or_e = FS_write_owq( owq_byte ) ;
		OWQ_destroy( owq_byte ) ;
	}
	return z_or_e ;
}

/* Write a bit in a BYTE */
// Handles: .n
static int FS_write_a_bit(struct one_wire_query *owq_bit)
{
	struct one_wire_query * owq_byte = OWQ_create_separate( EXTENSION_BYTE, owq_bit ) ;
	int z_or_e = -ENOENT ;

	if ( owq_byte != NO_ONE_WIRE_QUERY ) {
		if ( FS_read_local( owq_byte ) >= 0 ) {
			UT_setbit_U( &OWQ_U( owq_byte ), OWQ_pn(owq_bit).extension, OWQ_Y(owq_bit) ) ;
			z_or_e = FS_write_owq( owq_byte ) ;
		}
		OWQ_destroy( owq_byte ) ;
	}
	return z_or_e ;
}

// Used for sibling write -- bus already locked, and it's local
int FS_write_local(struct one_wire_query *owq)
{
	return FS_w_local(owq);
}
