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
#include "ow_standard.h"
#include "ow_dirblob.h"
#include "ow_memblob.h"
#include "ow_cache.h"
#include "ow_search.h"
#include "ow_connection.h"

#define INDEX_BAD -1
#define INDEX_DEFAULT 0

/* ------- Prototypes ------------ */
static int CheckPresence_low(struct parsedname *pn);
static int CheckThisConnection(int bus_nr, struct parsedname *pn) ;

/* Check if device exists -- >=0 yes, -1 no */
int CheckPresence(struct parsedname *pn)
{
	int bus_nr;

	if (NotRealDir(pn)) {
		return INDEX_DEFAULT;
	}

	/* If set, already found bus. */
	/* Use UnsetKnownBus to clear and allow a new search */
	if (KnownBus(pn)) {
		return pn->known_bus->index;
	}
	if (GOOD(Cache_Get_Device(&bus_nr, pn))) {
		LEVEL_DEBUG("Found device on bus %d",bus_nr);
		SetKnownBus(bus_nr, pn);
		return bus_nr;
	}

	LEVEL_DETAIL("Checking presence of %s", SAFESTRING(pn->path));

	bus_nr = CheckPresence_low(pn);	// check only allocated inbound connections
	if (bus_nr != -1) {
		SetKnownBus(bus_nr, pn);
		Cache_Add_Device( bus_nr, pn->sn ) ;
		return bus_nr;
	}
	UnsetKnownBus(pn);

	return INDEX_BAD;
}

/* See if a cached location is accurate -- called with "Known Bus" set */
int ReCheckPresence(struct parsedname *pn)
{
	int bus_nr;

	if (NotRealDir(pn))
		return INDEX_DEFAULT;

	if (KnownBus(pn)) {
		if (CheckThisConnection(0,pn) != -1)
			return 0;
	}

	if ( GOOD( Cache_Get_Device(&bus_nr, pn)) ) {
		LEVEL_DEBUG("Found device on bus %d",bus_nr);
		if (CheckThisConnection(bus_nr,pn) != -1) {
			SetKnownBus(bus_nr, pn);
			return bus_nr ;
		}
	}

	UnsetKnownBus(pn);
	Cache_Del_Device(pn) ;

	return CheckPresence(pn);
}

static int CheckPresence_low(struct parsedname *pn)
{
#if 0
	struct checkpresence_struct cps = { Inbound_Control.head_port, NULL, pn, INDEX_BAD };

	if ( cps.pin != NULL ) {
		CheckPresence_callback_port( (void *) (&cps) ) ;
	}

	return cps.bus_nr;
#else
	return 0;
#endif
}

int FS_present(struct one_wire_query *owq)
{
	struct parsedname *pn = PN(owq);

	LEVEL_CALL("check");
	if (NotRealDir(pn)) {
		OWQ_Y(owq) = 1;
	} else if ( IsUncachedDir(pn) ) {
#if 0 // TODO
		struct transaction_log t[] = {
			TRXN_NVERIFY,
			TRXN_END,
		};
		OWQ_Y(owq) = BAD(BUS_transaction(t, pn)) ? 0 : 1;
	} else if ( pn->selected_connection->iroutines.flags & ADAP_FLAG_sham ) {
		OWQ_Y(owq) = 0 ;
	} else {
		struct transaction_log t[] = {
			TRXN_NVERIFY,
			TRXN_END,
		};
		OWQ_Y(owq) = BAD(BUS_transaction(t, pn)) ? 0 : 1;
#else
		OWQ_Y(owq) = 1;
#endif
	}
	return 0;
}


// Look on a given connection for the device
static int CheckThisConnection(int bus_nr, struct parsedname *pn)
{
	struct parsedname s_pn_copy;
	struct parsedname * pn_copy = &s_pn_copy ;
	struct connection_in * in = find_connection_in(bus_nr) ;
	int connection_result = -1;

	if (in == NO_CONNECTION)
		return -1;

	memcpy(pn_copy, pn, sizeof(struct parsedname));	// shallow copy
	pn_copy->selected_connection = in;

	if (TestConnection(pn_copy) != 0)
		// Connection currently disconnected
		return -1;
	else
		connection_result =  in->index ;
	if (connection_result == -1) {
		LEVEL_DEBUG("Presence of "SNformat" NOT found", SNvar(pn_copy->sn)) ;
	} else {
		LEVEL_DEBUG("Presence of "SNformat" FOUND", SNvar(pn_copy->sn)) ;
		Cache_Add_Device(in->index,pn_copy->sn) ; // add or update cache */
	}

	return connection_result ;
}
