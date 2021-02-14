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
#include "ow_connection.h"

/* Tests whether this bus (pn->selected_connection) has too many consecutive reset errors */
/* If so, the bus is closed and "reconnected" */
/* Reconnection usually just means reopening (detect) with the same initial name like ttyS0 */
/* USB is a special case, in gets reenumerated, so we look for similar DS2401 chip */
int TestConnection(const struct parsedname *pn)
{
	int ret = 0;
	struct connection_in *in ;

	if (pn == NO_PARSEDNAME)
		return gbGOOD ;
	in = pn->selected_connection;
	if (in == NO_CONNECTION)
		return gbGOOD ;

	LEVEL_DEFAULT("bus master reconnected");
	ret = DS2482_detect(in->pown) ;	// call initial opener
	if (ret != 0) {
		LEVEL_DEFAULT("Failed to reconnect %s bus master!", in->adapter_name);
	} else
		LEVEL_DEFAULT("%s bus master reconnected", in->adapter_name);

	return ret;
}
