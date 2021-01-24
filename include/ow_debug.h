/*
   Debug and error messages
   Meant to be included in ow.h
   Separated just for readability
*/

/* OWFS source code
   1-wire filesystem for linux
   {c} 2006 Paul H Alfille
   License GPL2.0
*/

#ifndef OW_DEBUG_H
#define OW_DEBUG_H

#include <stdarg.h>

/* error functions */
enum e_err_level { e_err_default, e_err_call, e_err_data,
	e_err_detail, e_err_debug, e_err_beyond,
};
enum e_err_type { e_err_type_level, e_err_type_error, };
enum e_err_print { e_err_print_mixed, e_err_print_syslog,
	e_err_print_console,
};

void err_msg(enum e_err_type errnoflag, enum e_err_level level, const char * file, int line, const char * func, const char *fmt, ...);
void fatal_error(const char * file, int line, const char * func, const char *fmt, ...);
static inline int return_ok(void) { return 0; }

void print_timestamp_(const char * file, int line, const char * func, const char *fmt, ...);
#define print_timestamp(...)    print_timestamp_(__FILE__,__LINE__,__func__,__VA_ARGS__);

extern int log_available;

#if OW_DEBUG
#define LEVEL_DEFAULT(...)    if (Globals.error_level>=e_err_default) {\
    err_msg(e_err_type_level,e_err_default,__FILE__,__LINE__,__func__,__VA_ARGS__); }
#define LEVEL_CALL(...)       if (Globals.error_level>=e_err_call)  {\
    err_msg(e_err_type_level,e_err_call,   __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define LEVEL_DATA(...)       if (Globals.error_level>=e_err_data) {\
    err_msg(e_err_type_level,e_err_data,   __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define LEVEL_DETAIL(...)     if (Globals.error_level>=e_err_detail) {\
    err_msg(e_err_type_level,e_err_detail, __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define LEVEL_DEBUG(...)      if (Globals.error_level>=e_err_debug) {\
    err_msg(e_err_type_level,e_err_debug,  __FILE__,__LINE__,__func__,__VA_ARGS__); }
    
#define ERROR_DEFAULT(...)    if (Globals.error_level>=e_err_default) {\
    err_msg(e_err_type_error,e_err_default,__FILE__,__LINE__,__func__,__VA_ARGS__); }
#define ERROR_CALL(...)       if (Globals.error_level>=e_err_call)  {\
    err_msg(e_err_type_error,e_err_call,   __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define ERROR_DATA(...)       if (Globals.error_level>=e_err_data) {\
    err_msg(e_err_type_error,e_err_data,   __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define ERROR_DETAIL(...)     if (Globals.error_level>=e_err_detail) {\
    err_msg(e_err_type_error,e_err_detail, __FILE__,__LINE__,__func__,__VA_ARGS__); }
#define ERROR_DEBUG(...)      if (Globals.error_level>=e_err_debug) {\
    err_msg(e_err_type_error,e_err_debug,  __FILE__,__LINE__,__func__,__VA_ARGS__); }

#define FATAL_ERROR(...) fatal_error(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define Debug_OWQ(owq)        if (Globals.error_level>=e_err_debug) { _print_owq(owq); }

#else /* not OW_DEBUG */

#define LEVEL_DEFAULT(...)    do { } while (0)
#define LEVEL_CONNECT(...)    do { } while (0)
#define LEVEL_CALL(...)       do { } while (0)
#define LEVEL_DATA(...)       do { } while (0)
#define LEVEL_DETAIL(...)     do { } while (0)
#define LEVEL_DEBUG(...)      do { } while (0)

#define ERROR_DEFAULT(...)    do { } while (0)
#define ERROR_CONNECT(...)    do { } while (0)
#define ERROR_CALL(...)       do { } while (0)
#define ERROR_DATA(...)       do { } while (0)
#define ERROR_DETAIL(...)     do { } while (0)
#define ERROR_DEBUG(...)      do { } while (0)

#define FATAL_ERROR(...)      do { exit(EXIT_FAILURE); } while (0)

#define Debug_Bytes(title,buf,length)    do { } while (0)
#define Debug_OWQ(owq)        do { } while (0)

#endif /* OW_DEBUG */

/* Make sure strings are safe for printf */
#define SAFESTRING(x) ((x!=NULL) ? (x):"")

/* Easy way to show 64bit serial numbers */
#define SNformat	"%.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X"
#define SNvar(sn)	(sn)[0],(sn)[1],(sn)[2],(sn)[3],(sn)[4],(sn)[5],(sn)[6],(sn)[7]

#endif							/* OW_DEBUG_H */