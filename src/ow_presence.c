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

#define INDEX_BAD -1
#define INDEX_DEFAULT 0

extern int DS2482_detect(int *fd);

/* ------- Prototypes ------------ */
static int CheckPresence_low(struct parsedname *pn);
static int CheckThisConnection(int bus_nr, struct parsedname *pn) ;
static GOOD_OR_BAD PresenceFromDirblob( struct parsedname * pn ) ;

/* ------- Functions ------------ */
int SetKnownBus( int bus_number, struct parsedname * pn)
{
	pn->state |= ePS_bus;

	return 0 ;
}

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
		return 0;
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
	pn->fd = -1;
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

/* Check if device exists -- -1 no, >=0 yes (bus number) */
/* lower level, cycle through the devices */
struct checkpresence_struct {
	struct port_in * pin;
	struct connection_in * cin;
	struct parsedname *pn;
	int bus_nr;
};

static void * CheckPresence_callback_conn(void * v)
{
#if 0
	struct checkpresence_struct * cps = (struct checkpresence_struct *) v ;
	struct checkpresence_struct cps_next ;
	pthread_t thread;
	int threadbad = 0 ;

	cps_next.cin = cps->cin->next ;
	if ( cps_next.cin == NULL) {
		threadbad = 1 ;
	} else {
		//cps_next.pin = cps->pin ;
		cps_next.pn = cps->pn;
		cps_next.bus_nr = -1;
		threadbad = pthread_create(&thread, NULL, CheckPresence_callback_conn, (void *) (&cps_next)) ;
	}

	//cps->bus_nr = CheckThisConnection( cps->cin->index, cps->pn ) ;
	cps->bus_nr = 0;
	if (threadbad == 0) {		/* was a thread created? */
		if (pthread_join(thread, NULL)==0) {
			if (cps_next.bus_nr != -1) {
				cps->bus_nr = cps_next.bus_nr ;
			}
		}
	}
#endif
	return NULL;
}

static void * CheckPresence_callback_port(void * v)
{
#if 0
	struct checkpresence_struct * cps = (struct checkpresence_struct *) v ;
	struct checkpresence_struct cps_next ;
	pthread_t thread;
	int threadbad = 0 ;

	cps_next.pin = cps->pin->next ;
	if ( cps_next.pin == NULL ) {
		threadbad = 1 ;
	} else {
		cps_next.pn = cps->pn ;
		cps_next.bus_nr = INDEX_BAD ;
		threadbad = pthread_create(&thread, DEFAULT_THREAD_ATTR, CheckPresence_callback_port, (void *) (&cps_next)) ;
	}

	cps->cin = cps->pin->first ;
	if (cps->cin != NULL) {
		CheckPresence_callback_conn(v) ;
	}

	if (threadbad == 0) {		/* was a thread created? */
		if (pthread_join(thread, NULL)==0) {
			if ( INDEX_VALID(cps_next.bus_nr) ) {
				cps->bus_nr = cps_next.bus_nr ;
			}
		}
	}
#endif
	return NULL;
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

	if (NotRealDir(pn)) {
		OWQ_Y(owq) = 1;
	} else if ( IsUncachedDir(pn) ) {
#if 0 // TODO
		struct transaction_log t[] = {
			TRXN_NVERIFY,
			TRXN_END,
		};
		OWQ_Y(owq) = BAD(BUS_transaction(t, pn)) ? 0 : 1;
	} else if ( pn->selected_connection->iroutines.flags & ADAP_FLAG_presence_from_dirblob ) {
		OWQ_Y(owq) = GOOD( PresenceFromDirblob(pn) ) ;
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
	if (DS2482_detect(&pn->fd) != 0)
		return INDEX_BAD;

	return 0;
}

static GOOD_OR_BAD PresenceFromDirblob( struct parsedname * pn )
{
	struct dirblob db;	// cached dirblob
	if ( GOOD( Cache_Get_Dir( &db , pn ) ) ) {
		// Use the dirblob from the cache
		GOOD_OR_BAD ret = ( DirblobSearch(pn->sn, &db ) >= 0 ) ? gbGOOD : gbBAD ;
		DirblobClear( &db ) ;
		return ret ;
	} else {
		// look through actual directory
		struct device_search ds ;
		enum search_status nextboth = BUS_first( &ds, pn ) ;

		while ( nextboth == search_good ) {
			if ( memcmp( ds.sn, pn->sn, SERIAL_NUMBER_SIZE ) == 0 ) {
				// found it. Early exit.
				BUS_next_cleanup( &ds );
				return gbGOOD ;
			}
			// Not found. Clean up done by BUS_next in this case
			nextboth = BUS_next( &ds, pn ) ;
		}
		return gbBAD ;
	}
}
