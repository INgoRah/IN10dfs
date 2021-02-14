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
#include "ow.h"
#include "ow_connection.h"
#include <pthread.h>
//#include "ow_devices.h"
#include <regex.h>

void ow_regcomp( regex_t * preg, const char * regex, int cflags ) ;

struct ow_regmatch {
	int number ;
	char ** pre ;
	char ** match ;
	char ** post ;
} ;

int ow_regexec( const regex_t * rex, const char * string, struct ow_regmatch * orm ) ;
void ow_regexec_free( struct ow_regmatch * orm ) ;

#define CONNIN_WLOCK
#define CONNIN_WUNLOCK
#define CONNIN_RLOCK
#define CONNIN_RUNLOCK

#define CONTROLFLAGSLOCK
#define CONTROLFLAGSUNLOCK

#define INDEX_NOT_VALID(bus_nr)  ((bus_nr) == -1)

static int BranchAdd(struct parsedname *pn);

enum parse_pass {
	parse_pass_pre_remote,
	parse_pass_post_remote,
};

struct parsedname_pointers {
	char pathcpy[PATH_MAX+1];
	char *pathnow;
	char *pathnext;
	char *pathlast;
};

static enum parse_enum set_type( enum ePN_type epntype, struct parsedname * pn ) ;
static enum parse_enum Parse_Unspecified(char *pathnow, enum parse_pass remote_status, struct parsedname *pn);
static enum parse_enum Parse_Branch(char *pathnow, enum parse_pass remote_status, struct parsedname *pn);
static enum parse_enum Parse_Real(char *pathnow, enum parse_pass remote_status, struct parsedname *pn);
static enum parse_enum Parse_RealDevice(char *filename, enum parse_pass remote_status, struct parsedname *pn);
static enum parse_enum Parse_Property(char *filename, struct parsedname *pn);

static enum parse_enum Parse_RealDeviceSN(enum parse_pass remote_status, struct parsedname *pn);
static enum parse_enum Parse_Bus( int bus_number, struct parsedname *pn);

static int FS_ParsedName_anywhere(const char *path, enum parse_pass remote_status, struct parsedname *pn);
static int FS_ParsedName_setup(struct parsedname_pointers *pp, const char *path, struct parsedname *pn);

#define BRANCH_INCR (9)

static regex_t rx_bus;
static regex_t rx_set;
static regex_t rx_str;
static regex_t rx_tex;
static regex_t rx_jso;
static regex_t rx_unc;
static regex_t rx_ala;
static regex_t rx_p_bus;
static regex_t rx_extension;
static regex_t rx_all;
static regex_t rx_byte;
static regex_t rx_number;
static regex_t rx_letter;

static void regex_fini(void)
{
	regfree(&rx_bus);
	regfree(&rx_set);
	regfree(&rx_str);
	regfree(&rx_tex);
	regfree(&rx_jso);
	regfree(&rx_unc);
	regfree(&rx_ala);
	regfree(&rx_p_bus);
	regfree(&rx_extension);
	regfree(&rx_all);
	regfree(&rx_byte);
	regfree(&rx_number);
	regfree(&rx_letter);
}

static pthread_once_t regex_init_once = PTHREAD_ONCE_INIT;

static void regex_init(void)
{
	ow_regcomp(&rx_bus, "^bus\\.([[:digit:]]+)/?", REG_ICASE);
	ow_regcomp(&rx_set, "^settings/?", REG_ICASE | REG_NOSUB);
	//ow_regcomp(&rx_sta, "^statistics/?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_str, "^structure/?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_tex, "^text/?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_jso, "^json/?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_unc, "^uncached/?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_ala, "^alarm\?", REG_ICASE | REG_NOSUB);
	ow_regcomp(&rx_p_bus, "^/bus\\.[[:digit:]]+/?", REG_ICASE);
	ow_regcomp(&rx_extension, "\\.", 0);
	ow_regcomp(&rx_all, "\\.all$", REG_ICASE);
	ow_regcomp(&rx_byte, "\\.byte$", REG_ICASE);
	ow_regcomp(&rx_number, "\\.[[:digit:]]+$", 0);
	ow_regcomp(&rx_letter, "\\.[[:alpha:]]$", REG_ICASE);

	atexit(regex_fini);
}

/* ---------------------------------------------- */
/* Filename (path) parsing functions              */
/* ---------------------------------------------- */
void FS_ParsedName_destroy(struct parsedname *pn)
{
	if (!pn) {
		return;
	}
	//LEVEL_DEBUG("%s", SAFESTRING(pn->path));
	CONNIN_RUNLOCK ;
	//Detail_Free( pn ) ;
	SAFEFREE(pn->sparse_name);
}

/*
 * Path is either NULL (in which case a minimal structure is created that doesn't need Destroy -- used for Bus Master setups)
 * or Path is a full "filename" type string of form: 10.1243Ab000 or uncached/bus.0/statistics etc.
 *
 * The Path passed in isn't altered, but 2 copies are made -- one with the full path, the other (to_server) has the first bus.n removed.
 * An initial / is added to the path, and the full length has to be less than MAX_PATH (2048)
 *
 * For efficiency, the two path copies are allocated in the same memory allocation call, and so can be removed together.
 * */

/* Parse a path to check it's validity and attach to the propery data structures */
int FS_ParsedName(const char *path, struct parsedname *pn)
{
	return FS_ParsedName_anywhere(path, parse_pass_pre_remote, pn);
}

/* Parse a path from a remote source back -- so don't check presence */
int FS_ParsedName_BackFromRemote(const char *path, struct parsedname *pn)
{
	return FS_ParsedName_anywhere(path, parse_pass_post_remote, pn);
}

/* Parse off starting "mode" directory (uncached, alarm...) */
static int FS_ParsedName_anywhere(const char *path, enum parse_pass remote_status, struct parsedname *pn)
{
	struct parsedname_pointers s_pp;
	struct parsedname_pointers *pp = &s_pp;
	int parse_error_status = 0;
	enum parse_enum pe = parse_first;
	int ret;

	// To make the debug output useful it's cleared here.
	// Even on normal glibc, errno isn't cleared on good system calls
	errno = 0;

	//LEVEL_CALL("path=[%s]", SAFESTRING(path));

	ret = FS_ParsedName_setup(pp, path, pn);
	if (ret != 0)
		return ret;

	if (path == NO_PATH)
		return 0;

	while (1) {
		// Check for extreme conditions (done, error)
		switch (pe) {

		case parse_done:		// the only exit!
			//LEVEL_DEBUG("PARSENAME parse_done") ;
			//printf("PARSENAME end parse_error_status=%d\n",parse_error_status) ;
			if (parse_error_status) {
				FS_ParsedName_destroy(pn);
				return parse_error_status ;
			}

			if ( pp->pathnext != NULL ) {
				// extra text -- make this an error
				parse_error_status = 77; // extra text in path
				pe = parse_done;
				continue;
			}

			//printf("%s: Parse before corrections: %.4X -- state = %d\n",pn->path,pn->state,pn->type) ;
			// Play with remote levels
			switch ( pn->type ) {
				case ePN_interface:
					// /interface is interesting -- it's actually a property of the calling server
					pn->state |= ePS_buslocal ;
					break ;

				case ePN_root:
					// root buses are considered "real"
					pn->type = ePN_real;	// default state
					break ;
				default:
					// everything else gets promoted so directories aren't added on
					break ;
			}
			//printf("%s: Parse %s after  corrections: %.4X -- state = %d\n\n",(back_from_remote)?"BACK":"FORE",pn->path,pn->state,pn->type) ;
			// set up detail debugging
			//Detail_Test( pn ) ; // turns on debug mode only during this device's query
			return 0;

		case parse_error:
			parse_error_status = 27; // bad path syntax
			pe = parse_done;
			continue;

		default:
			break;
		}

		// break out next name in path
		if ( pp->pathnext == NULL ) {
			// make sure pp->pathnext isn't NULL. (SIGSEGV in uClibc)
			pp->pathnow = NULL ;
		} else {
			pp->pathnow = strsep(&(pp->pathnext), "/") ;
		}
		//LEVEL_DEBUG("PARSENAME pathnow=[%s] rest=[%s]",pp->pathnow,pp->pathnext) ;

		// test next path segment for existence
		if (pp->pathnow == NULL || pp->pathnow[0] == '\0') {
			// done parsing
			pe = parse_done;
		}

		// rest of state machine on parsename
		switch (pe) {

		case parse_done:
			// nothing left to process -- will be handled in next loop pass
			break ;

		case parse_first:
			//LEVEL_DEBUG("PARSENAME parse_first") ;
			pe = Parse_Unspecified(pp->pathnow, remote_status, pn);
			break;

		case parse_real:
			//LEVEL_DEBUG("PARSENAME parse_real") ;
			pe = Parse_Real(pp->pathnow, remote_status, pn);
			break;

		case parse_branch:
			//LEVEL_DEBUG("PARSENAME parse_branch") ;
			pe = Parse_Branch(pp->pathnow, remote_status, pn);
			break;

		case parse_prop:
			//LEVEL_DEBUG("PARSENAME parse_prop") ;
			pn->dirlength = pp->pathnow - pp->pathcpy + 1 ;
			//LEVEL_DEBUG("Dirlength=%d <%*s>",pn->dirlength,pn->dirlength,pn->path) ;
			//printf("dirlength = %d which makes the path <%s> <%.*s>\n",pn->dirlength,pn->path,pn->dirlength,pn->path);
			pp->pathlast = pp->pathnow;	/* Save for concatination if subdirectory later wanted */
			pe = Parse_Property(pp->pathnow, pn);
			break;

		case parse_subprop:
			//LEVEL_DEBUG("PARSENAME parse_subprop") ;
			pp->pathnow[-1] = '/';
			pe = Parse_Property(pp->pathlast, pn);
			break;

		default:
			pe = parse_error;	// unknown state
			break;

		}
		//printf("PARSENAME pe=%d\n",pe) ;
	}
}

extern int fd;

/* Initial memory allocation and pn setup */
static int FS_ParsedName_setup(struct parsedname_pointers *pp, const char *path, struct parsedname *pn)
{
	if (pn == NO_PARSEDNAME)
		return 78; // unexpected null pointer

	memset(pn, 0, sizeof(struct parsedname));
	pn->known_bus = NULL;		/* all buses */
	pn->sparse_name = NULL ;
	pn->return_code = 0;

	/* Set the persistent state info (temp scale, ...) -- will be overwritten by client settings in the server */
	CONTROLFLAGSLOCK;
	pn->control_flags = /*LocalControlFlags | */ SHOULD_RETURN_BUS_LIST;	// initial flag as the bus-returning level, will change if a bus is specified
	CONTROLFLAGSUNLOCK;

	// initialization
	pp->pathnow = NO_PATH;
	pp->pathlast = NO_PATH;
	pp->pathnext = NO_PATH;

	/* Default attributes */
	pn->state = ePS_normal;
	pn->type = ePN_root;

	/* uncached attribute */
	if ( Globals.uncached ) {
		// local settings (--uncached) can set
		pn->state |= ePS_uncached;
	}
	// Also can be set by path ("/uncached")
	// Also in owserver, can be set by client flags
	// sibling inherits parent
#if 0
	/* unaliased attribute */
	if ( Globals.unaliased ) {
		// local settings (--unalaised) can set
		pn->state |= ePS_unaliased;
	}
#endif
	// Also can be set by path ("/unaliased")
	// Also in owserver, can be set by client flags
	// sibling inherits parent

	/* No device lock yet assigned */
	pn->lock = NULL ;

	/* minimal structure for initial bus "detect" use -- really has connection and LocalControlFlags only */
	pn->dirlength = -1 ;
	if (path == NO_PATH)
		return 0; // success

	if (strlen(path) > PATH_MAX)
		return -26; // path too long

	/* Have to save pn->path at once */
	strcpy(pn->path, "/"); // initial slash
	strcpy(pn->path+1, path[0]=='/'?path+1:path);
	strcpy(pn->path_to_server, pn->path);

	/* make a copy for destructive parsing  without initial '/'*/
	strcpy(pp->pathcpy,&pn->path[1]);
	/* pointer to rest of path after current token peeled off */
	pp->pathnext = pp->pathcpy;
	pn->dirlength = strlen(pn->path) ;

	/* connection_in list and start */
	pn->selected_connection = NULL ; // Default bus assignment

	return 0 ; // success
}

/* Used for virtual directories like settings and statistics
 * If local, applies to all local (this program) and not a
 * specific local bus.
 * If remote, pass it on for the remote to handle
 * */
static enum parse_enum set_type( enum ePN_type epntype, struct parsedname * pn )
{
	if (SpecifiedLocalBus(pn)) {
		return parse_error;
	}

	pn->type = epntype;
	return parse_nonreal;
}


// Early parsing -- only bus entries, uncached and text may have preceeded
static enum parse_enum Parse_Unspecified(char *pathnow, enum parse_pass remote_status, struct parsedname *pn)
{
	pthread_once(&regex_init_once, regex_init);

	struct ow_regmatch orm ;
	orm.number = 1 ; // for bus

	if ( ow_regexec( &rx_bus, pathnow, &orm ) == 0) {
		int bus_number = (int) atoi(orm.match[1]) ;
		ow_regexec_free( &orm ) ;
		return Parse_Bus( bus_number, pn);

	} else if (ow_regexec( &rx_set, pathnow, NULL ) == 0) {
		return set_type( ePN_settings, pn ) ;

	} else if (ow_regexec( &rx_str, pathnow, NULL ) == 0) {
		return set_type( ePN_structure, pn ) ;

	} else if (ow_regexec( &rx_tex, pathnow, NULL ) == 0) {
		pn->state |= ePS_text;
		return parse_first;

	} else if (ow_regexec( &rx_jso, pathnow, NULL ) == 0) {
		pn->state |= ePS_json;
		return parse_first;

	} else if (ow_regexec( &rx_unc, pathnow, NULL ) == 0) {
		pn->state |= ePS_uncached;
		return parse_first;

	}

	pn->type = ePN_real;
	return Parse_Branch(pathnow, remote_status, pn);
}

static enum parse_enum Parse_Branch(char *pathnow, enum parse_pass remote_status, struct parsedname *pn)
{
	pthread_once(&regex_init_once, regex_init);

	if (ow_regexec( &rx_ala, pathnow, NULL ) == 0) {
		pn->state |= ePS_alarm;
		pn->type = ePN_real;
		return parse_real;
	}
	return Parse_Real(pathnow, remote_status, pn);
}

static enum parse_enum Parse_Real(char *pathnow, enum parse_pass remote_status, struct parsedname *pn)
{
	pthread_once(&regex_init_once, regex_init);

	if (ow_regexec( &rx_tex, pathnow, NULL ) == 0) {
		pn->state |= ePS_text;
		return parse_real;

	} else if (ow_regexec( &rx_jso, pathnow, NULL ) == 0) {
		pn->state |= ePS_json;
		return parse_real;

	} else if (ow_regexec( &rx_unc, pathnow, NULL ) == 0) {
		pn->state |= ePS_uncached;
		return parse_real;

	} else {
		return Parse_RealDevice(pathnow, remote_status, pn);
	}
}

/* We've reached a /bus.n entry */
static enum parse_enum Parse_Bus(int bus_number, struct parsedname *pn)
{
	pthread_once(&regex_init_once, regex_init);

	struct ow_regmatch orm ;
	orm.number = 0 ;

	/* Processing for bus.X directories -- eventually will make this more generic */
	if (bus_number  == -1) {
		return parse_error;
	}

	/* Should make a presence check on remote buses here, but
	 * it's not a major problem if people use bad paths since
	 * they will just end up with empty directory listings. */
	if (SpecifiedLocalBus(pn)) {	/* already specified a "bus." */
		/* too many levels of bus for a non-remote adapter */
		return parse_error;
	}
	/* Since we are going to use a specific in-device now, set
	 * pn->selected_connection to point at that device at once. */
	if (SetKnownBus(bus_number, pn))
		return parse_error ; // bus doesn't exist

	pn->state |= ePS_buslocal;
	/* don't return bus-list for local paths. */
	pn->control_flags &= (~SHOULD_RETURN_BUS_LIST);

	/* Create the path without the "bus.x" part in pn->path_to_server */
	if ( ow_regexec( &rx_p_bus, pn->path, &orm ) == 0 ) {
		strcpy( pn->path_to_server, orm.pre[0] ) ;
		strcat( pn->path_to_server, "/" ) ;
		strcat( pn->path_to_server, orm.post[0] ) ;
		ow_regexec_free( &orm ) ;
	}
	return parse_first;
}

/* Parse Name (only device name) part of string */
/* Return -ENOENT if not a valid name
   return 0 if good
   *next points to next segment, or NULL if not filetype
 */
static enum parse_enum Parse_RealDevice(char *filename, enum parse_pass remote_status, struct parsedname *pn)
{
	switch (Parse_SerialNumber(filename,pn->sn)) {
		case sn_valid:
			return Parse_RealDeviceSN( remote_status, pn ) ;
		case sn_invalid:
		case sn_not_sn:
		default:
			return parse_error ;
	}
}

/* Device is known with serial number */
static enum parse_enum Parse_RealDeviceSN(enum parse_pass remote_status, struct parsedname *pn)
{
	int bus_nr;

	/* Search for known 1-wire device -- keyed to device name (family code in HEX) */
	pn->selected_device = FS_devicefindhex(pn->sn[0], pn);

	// returning from owserver -- don't need to check presence (it's implied)
	if (remote_status == parse_pass_post_remote)
		return parse_prop;

	/* Check the presence, and cache the proper bus number for better performance */
	bus_nr = CheckPresence(pn);

	if (bus_nr == -1)
		return parse_error;	/* CheckPresence failed */

	return parse_prop;
}

static enum parse_enum Parse_Property(char *filename, struct parsedname *pn)
{
	pthread_once(&regex_init_once, regex_init);

	struct device * pdev = pn->selected_device ;
	struct filetype * ft ;

	int extension_given ;

	struct ow_regmatch orm ;
	orm.number = 0 ;

	//printf("FilePart: %s %s\n", filename, pn->path);

	// Special case for remote device. Use distant data
	if ( pdev == &RemoteDevice ) {
		// remote device, no known sn, handle property in server.
		return parse_done ;
	}

	// separate filename.dot
//	filename = strsep(&dot, ".");
	if ( ow_regexec( &rx_extension, filename, &orm ) == 0 ) {
		// extension given
		extension_given = 1 ;
		ft =
			 bsearch(orm.pre[0], pdev->filetype_array,
					 (size_t) pdev->count_of_filetypes, sizeof(struct filetype), filetype_cmp) ;
		ow_regexec_free( &orm ) ;
	} else {
		// no extension given
		extension_given = 0 ;
		ft =
			 bsearch(filename, pdev->filetype_array,
					 (size_t) pdev->count_of_filetypes, sizeof(struct filetype), filetype_cmp) ;
	}

	pn->selected_filetype = ft ;
	if (ft == NO_FILETYPE ) {
		LEVEL_DEBUG("Unknown property for this device %s",SAFESTRING(filename) ) ;
		return parse_error;			/* filetype not found */
	}

	//printf("FP known filetype %s\n",pn->selected_filetype->name) ;
	/* Filetype found, now process extension */
	if (extension_given==0) {	/* no extension */
		if (ft->ag != NON_AGGREGATE) {
			return parse_error;	/* aggregate filetypes need an extension */
		}
		pn->extension = 0;	/* default when no aggregate */

	// Non-aggregate cannot have an extension
	} else if (ft->ag == NON_AGGREGATE) {
		return parse_error;	/* An extension not allowed when non-aggregate */

	// Sparse uses the extension verbatim (text or number)
	} else if (ft->ag->combined==ag_sparse)  { /* Sparse */
		if (ft->ag->letters == ag_letters) {	/* text string */
			pn->extension = 0;	/* text extension, not number */
			ow_regexec( &rx_extension, filename, &orm ) ; // don't need to test -- already succesful
			pn->sparse_name = owstrdup(orm.post[0]) ;
			ow_regexec_free( &orm ) ;
			LEVEL_DEBUG("Sparse alpha extension found: <%s>",pn->sparse_name);
		} else {			/* Numbers */
			if ( ow_regexec( &rx_number, filename, &orm ) == 0 ) {
				pn->extension = atoi( &orm.match[0][1] );	/* Number conversion */
				ow_regexec_free( &orm ) ;
				LEVEL_DEBUG("Sparse numeric extension found: <%ld>",(long int) pn->extension);
			} else {
				LEVEL_DEBUG("Non numeric extension for %s",filename ) ;
				return parse_error ;
			}
		}

	// Non-sparse "ALL"
	} else if (ow_regexec( &rx_all, filename, NULL ) == 0) {
		//printf("FP ALL\n");
		pn->extension = EXTENSION_ALL;	/* ALL */

	// Non-sparse "BYTE"
	} else if (ft->format == ft_bitfield && ow_regexec( &rx_byte, filename, NULL) == 0) {
		pn->extension = EXTENSION_BYTE;	/* BYTE */
		//printf("FP BYTE\n") ;

	// Non-sparse extension -- interpret and check bounds
	} else {				/* specific extension */
		if (ft->ag->letters == ag_letters) {	/* Letters */
			//printf("FP letters\n") ;
			if ( ow_regexec( &rx_letter, filename, &orm ) == 0 ) {
				pn->extension = toupper(orm.match[0][1]) - 'A';	/* Letter extension */
				ow_regexec_free( &orm ) ;
			} else {
				return parse_error;
			}
		} else {			/* Numbers */
			if ( ow_regexec( &rx_number, filename, &orm ) == 0 ) {
				pn->extension = atoi( &orm.match[0][1] );	/* Number conversion */
				ow_regexec_free( &orm ) ;
			} else {
				return parse_error;
			}
		}
		//printf("FP ext=%d nr_elements=%d\n", pn->extension, pn->selected_filetype->ag->elements) ;
		/* Now check range */
		if ((pn->extension < 0)
			|| (pn->extension >= ft->ag->elements)) {
			//printf("FP Extension out of range %d %d %s\n", pn->extension, pn->selected_filetype->ag->elements, pn->path);
			LEVEL_DEBUG("Extension %d out of range",pn->extension ) ;
			return parse_error;	/* Extension out of range */
		}
		//printf("FP in range\n") ;
	}

	//printf("FP Good\n") ;
	switch (ft->format) {
	case ft_directory:		// aux or main
		if ( pn->type == ePN_structure ) {
			// special case, structure for aux and main
			return parse_done;
		}
		if (BranchAdd(pn) != 0) {
			//printf("PN BranchAdd failed for %s\n", filename);
			return parse_error;
		}
		return parse_branch;
	case ft_subdir:
		//printf("PN %s is a subdirectory\n", filename);
		pn->subdir = ft;
		pn->selected_filetype = NO_FILETYPE;
		return parse_subprop;
	default:
		return parse_done;
	}
}

static int BranchAdd(struct parsedname *pn)
{
#if 0
	//printf("BRANCHADD\n");
	if ((pn->ds2409_depth % BRANCH_INCR) == 0) {
		void *temp = pn->bp;
		if ((pn->bp = owrealloc(temp, (BRANCH_INCR + pn->ds2409_depth) * sizeof(struct ds2409_hubs))) == NULL) {
			SAFEFREE(temp) ;
			RETURN_CODE_RETURN( 79 ) ; // unable to allocate memory
		}
	}
	memcpy(pn->bp[pn->ds2409_depth].sn, pn->sn, SERIAL_NUMBER_SIZE);	/* copy over DS2409 name */
	pn->bp[pn->ds2409_depth].branch = pn->selected_filetype->data.i;
	++pn->ds2409_depth;
#endif
	pn->selected_filetype = NO_FILETYPE;
	pn->selected_device = NO_DEVICE;
	return 0;
}

int filetype_cmp(const void *name, const void *ex)
{
	return strcmp((const char *) name, ((const struct filetype *) ex)->name);
}

/* Parse a path/file combination */
int FS_ParsedNamePlus(const char *path, const char *file, struct parsedname *pn)
{
	int ret = 0;
	char *fullpath;

	if (path == NO_PATH) {
		path = "" ;
	}
	if (file == NULL) {
		file = "" ;
	}

	fullpath = owmalloc(strlen(file) + strlen(path) + 2);
	if (fullpath == NO_PATH)
		return -79; // unable to allocate memory
	strcpy(fullpath, path);
	if (fullpath[strlen(fullpath) - 1] != '/') {
		strcat(fullpath, "/");
	}
	strcat(fullpath, file);
	//printf("PARSENAMEPLUS path=%s pre\n",fullpath) ;
	ret = FS_ParsedName(fullpath, pn);
	//printf("PARSENAMEPLUS path=%s post\n",fullpath) ;
	owfree(fullpath);
	//printf("PARSENAMEPLUS free\n") ;

	return ret;
}

/* Parse a path/file combination */
int FS_ParsedNamePlusExt(const char *path, const char *file, int extension, enum ag_index alphanumeric, struct parsedname *pn)
{
	if (extension == EXTENSION_BYTE ) {
		return FS_ParsedNamePlusText(path, file, "BYTE", pn);
	} else if (extension == EXTENSION_ALL ) {
		return FS_ParsedNamePlusText(path, file, "ALL", pn);
	} else if (alphanumeric == ag_letters) {
		char name[2] = { 'A'+extension, 0x00, } ;
		return FS_ParsedNamePlusText(path, file, name, pn);
	} else {
		char name[OW_FULLNAME_MAX];

		snprintf(name, OW_FULLNAME_MAX, "%d", extension);

		return FS_ParsedNamePlusText(path, file, name, pn);
	}
}

/* Parse a path/file combination */
int FS_ParsedNamePlusText(const char *path, const char *file, const char *extension, struct parsedname *pn)
{
	char name[OW_FULLNAME_MAX];

	snprintf(name, OW_FULLNAME_MAX, "%s.%s", file, extension );

	return FS_ParsedNamePlus(path, name, pn);
}

void FS_ParsedName_Placeholder( struct parsedname * pn )
{
	FS_ParsedName( NULL, pn ) ; // minimal parsename -- no destroy needed
}
