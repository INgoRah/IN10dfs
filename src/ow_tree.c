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
#include "ow_devices.h"
#include <search.h>

static int device_compare(const void *a, const void *b);
static int file_compare(const void *a, const void *b);
static void Device2Tree(const struct device *d, enum ePN_type type);
static void External_Process(void);

static int device_compare(const void *a, const void *b)
{
	return strcmp(((const struct device *) a)->family_code, ((const struct device *) b)->family_code);
}

static int file_compare(const void *a, const void *b)
{
	return strcmp(((const struct filetype *) a)->name, ((const struct filetype *) b)->name);
}

void *Tree[ePN_max_type];

#ifdef __FreeBSD__
static void Device2Tree(const struct device *d, enum ePN_type type)
{
	// FreeBSD fix from Robert Nilsson
	/*  In order for DeviceDestroy to work on FreeBSD we must copy the keys.
	    Otherwise, tdestroy will attempt to free implicitly allocated structures.
	*/
	// Note, I'm not using owmalloc since owfree is probably not calledby FreeBSD's tdestroy //
	struct device *d_copy ;
	if ((d_copy = (struct device *) malloc(sizeof(struct device)))) {
		memmove(d_copy, d, sizeof(struct device));
	} else {
		LEVEL_DATA("Could not allocate memory for device %s", d->readable_name);
		return ;
	}
	// Add in the device into the appropriate red/black tree
	tsearch(d_copy, &Tree[type], device_compare);
	// Sort all the file types alphabetically for search and listing
	if (d_copy->filetype_array != NULL) {
		qsort(d_copy->filetype_array, (size_t) d_copy->count_of_filetypes, sizeof(struct filetype), file_compare);
	}
}
#else    /* not FreeBSD */
static void Device2Tree(const struct device *d, enum ePN_type type)
{
	// Add in the device into the appropriate red/black tree
	tsearch(d, &Tree[type], device_compare);
	// Sort all the file types alphabetically for search and listing
	if (d->filetype_array != NULL) {
		qsort(d->filetype_array, (size_t) d->count_of_filetypes, sizeof(struct filetype), file_compare);
	}
}
#endif							/* __FreeBSD__ */

static void free_node(void *nodep)
{
	(void) nodep;
	/* nothing to free */
	return;
}

void DeviceDestroy(void)
{
	unsigned int i;

	// clear external trees
/*	tdestroy( sensor_tree, owfree_func ) ;
	tdestroy( family_tree, owfree_func ) ;
	tdestroy( property_tree, owfree_func ) ;
*/
	// clear these trees -- have static data
	for (i = 0; i < (sizeof(Tree) / sizeof(void *)); i++) {
		/* ePN_structure is just a duplicate of ePN_real */
		if (i != ePN_structure) {
			if ((Tree[i]) !=NULL) {
                tdestroy(Tree[i],free_node);
                Tree[i] = NULL;
            }
		} else {
			/* ePN_structure (will be cleared in ePN_real) */
			Tree[i] = NULL;
		}
	}
}

void DeviceSort(void)
{
	memset(Tree, 0, sizeof(void *) * ePN_max_type);

	/* Sort the filetypes for the unrecognized device */
	qsort(UnknownDevice.filetype_array,
        (size_t) UnknownDevice.count_of_filetypes,
        sizeof(struct filetype), file_compare);
    /*
	Device2Tree( & d_DS18B20,        ePN_real);
	Device2Tree( & d_DS2408,         ePN_real);
	Device2Tree( & d_DS2413,         ePN_real);
	Device2Tree( & d_interface_settings,   ePN_interface);
	Device2Tree( & d_interface_statistics, ePN_interface);
    */

	/* structure uses same tree as real */
	Tree[ePN_structure] = Tree[ePN_real];
}

struct device * FS_devicefindhex(uint8_t f, struct parsedname *pn)
{
	char ID[] = "XX";
	const struct device d = { ID, NULL, 0, 0, NULL, NO_GENERIC_READ, NO_GENERIC_WRITE };
	struct device_opaque *p;

	num2string(ID, f);
	if ((p = tfind(&d, &Tree[pn->type], device_compare))) {
		return p->key;
	} else {
		num2string(ID, f ^ 0x80);
		if ((p = tfind(&d, &Tree[pn->type], device_compare))) {
			return p->key;
		}
	}
	return &UnknownDevice ;
}

void FS_devicefind(const char *code, struct parsedname *pn)
{
	const struct device d = { code, NULL, 0, 0, NULL, NO_GENERIC_READ, NO_GENERIC_WRITE };
	struct device_opaque *p = tfind(&d, &Tree[pn->type], device_compare);
	if (p) {
		pn->selected_device = p->key;
	} else {
		pn->selected_device = &UnknownDevice;
	}
}

/* Need to lock struct global_namefind_struct since twalk requires global data -- can't pass void pointer */
/* Except all *_detect routines are done sequentially, not concurrently */
struct {
	const struct family_node * f ;
	int count ;
} global_externalcount_struct;

static void External_propertycount_action(const void *nodep, const VISIT which, const int depth)
{
#if 0
	const struct property_node *p = *(struct property_node * const *) nodep;
	(void) depth;

	switch (which) {
	case leaf:
	case postorder:
		if (strcmp(p->family, global_externalcount_struct.f->family) == 0 ) {
			++global_externalcount_struct.count;
		}
	case preorder:
	case endorder:
		break;
	}
#endif
}

static void External_propertycopy_action(const void *nodep, const VISIT which, const int depth)
{
#if 0
	const struct property_node *p = *(struct property_node * const *) nodep;
	(void) depth;

	switch (which) {
	case leaf:
	case postorder:
		if (strcmp(p->family, global_externalcount_struct.f->family) == 0 ) {
			memcpy(
				& (global_externalcount_struct.f->dev.filetype_array[global_externalcount_struct.count]),
				&(p->ft),
				sizeof(struct filetype)
			) ;
			++global_externalcount_struct.count;
		}
	case preorder:
	case endorder:
		break;
	}
#endif
}

// First loop through families -- to count properties and allocate filetype array.
static void External_family_action(const void *nodep, const VISIT which, const int depth)
{
#if 0
	const struct family_node *p = *(struct family_node * const *) nodep;
	struct family_node * non_const_f ; // to allow assignments
	(void) depth;

 	switch (which) {
	case leaf:
	case postorder:
		// First count
		global_externalcount_struct.f = p ;
		global_externalcount_struct.count = 0 ;
		twalk( property_tree, External_propertycount_action);

		// Refind this node to allow assignment
		non_const_f = Find_External_Family( p->family ) ;
		non_const_f->dev.filetype_array = owcalloc(
			global_externalcount_struct.count,
			sizeof( struct filetype)
		);
		non_const_f->dev.count_of_filetypes = global_externalcount_struct.count ;

		// Next copy
		global_externalcount_struct.count = 0 ;
		twalk( property_tree, External_propertycopy_action);

		// Finally add to tree
		Device2Tree( & (p->dev), ePN_real);
		break ;
	case preorder:
	case endorder:
		break;
	}
#endif
}

static void External_Process(void)
{
	//twalk(family_tree, External_family_action);
}