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
#include "ow_search.h"
#include "ow_dirblob.h"
#include "ow_memblob.h"
#include "ow_cache.h"
//#include "ow_connection.h"
//#include "ow_codes.h"

extern enum search_status DS2482_next_both(struct device_search *ds, const struct parsedname *pn);

static void BUS_first_both(struct device_search *ds);
static enum search_status BUS_next_3try(struct device_search *ds, const struct parsedname *pn) ;

//--------------------------------------------------------------------------
/** The 'owFirst' doesn't find the first device on the 1-Wire Net.
 instead, it sets up for BUS_next interator.

 serialnumber -- 8byte (64 bit) serial number found

 Returns:   0-device found 1-no dev or error
*/
enum search_status BUS_first(struct device_search *ds, const struct parsedname *pn)
{
	LEVEL_DEBUG("Start of directory path=%s device=" SNformat, SAFESTRING(pn->path), SNvar(pn->sn));
	// reset the search state
	BUS_first_both(ds);
	ds->search = _1W_SEARCH_ROM;
	return BUS_next(ds, pn);
}

enum search_status BUS_first_alarm(struct device_search *ds, const struct parsedname *pn)
{
	LEVEL_DEBUG("Start of directory path=%s device=" SNformat, SAFESTRING(pn->path), SNvar(pn->sn));
	// reset the search state
	BUS_first_both(ds);
	ds->search = _1W_CONDITIONAL_SEARCH_ROM;
	return BUS_next(ds, pn);
}

static void BUS_first_both(struct device_search *ds)
{
	// reset the search state
	memset(ds->sn, 0, 8);		// clear the serial number
	ds->LastDiscrepancy = -1;
	ds->LastDevice = 0;
	ds->index = -1;				// true place in dirblob

	/* Initialize dir-at-once structure */
	//DirblobInit(&(ds->gulp));
}

//--------------------------------------------------------------------------
/** The BUS_next function does a general search.  This function
 continues from the previous search state (held in struct device_search). The search state
 can be reset by using the BUS_first function.

 Returns:  0=No problems, 1=Problems

 Sets LastDevice=1 if no more
*/
enum search_status BUS_next(struct device_search *ds, const struct parsedname *pn)
{
	switch ( BUS_next_3try(ds, pn) ) {
		case search_good:
			// found a device in a directory search, add to "presence" cache
			LEVEL_DEBUG("Device found: " SNformat, SNvar(ds->sn));
			Cache_Add_Device(0,ds->sn) ;
			return search_good ;
		case search_done:
			BUS_next_cleanup(ds);
			return search_done;
		case search_error:
		default:
			BUS_next_cleanup(ds);
			return search_error;
	}
}

void BUS_next_cleanup( struct device_search *ds )
{
	//DirblobClear(&(ds->gulp));
}


/* try the directory search 3 times.
 * Since ds->LastDescrepancy is altered only on success a repeat is legal
 * */
static enum search_status BUS_next_3try(struct device_search *ds, const struct parsedname *pn)
{
	switch (BUS_next_both(ds, pn) ) {
		case search_good:
			return search_good ;
		case search_done:
			return search_done;
		case search_error:
			break ;
	}

	switch (BUS_next_both(ds, pn) ) {
		case search_good:
			return search_good ;
		case search_done:
			return search_done;
		case search_error:
			break ;
	}

	switch (BUS_next_both(ds, pn) ) {
		case search_good:
			return search_good ;
		case search_done:
			return search_done;
		case search_error:
			break ;
	}

	return search_error;
}

enum search_status BUS_next_both(struct device_search *ds, const struct parsedname *pn)
{
	enum search_status next_both;

	next_both = DS2482_next_both(ds, pn);

	return next_both ;
}
