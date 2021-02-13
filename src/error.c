/*
    OWFS -- One-Wire filesystem
    OWHTTPD -- One-Wire Web Server
    Written 2003 Paul H Alfille
    email: paul.alfille@gmail.com
    Released under the GPL
    See the header file: ow.h for full attribution
    1wire/iButton system from Dallas Semiconductor
*/

/* error.c stolen nearly verbatim from
    "Unix Network Programming Volume 1 The Sockets Networking API (3rd Ed)"
   by W. Richard Stevens, Bill Fenner, Andrew M Rudoff
  Addison-Wesley Professional Computing Series
  Addison-Wesley, Boston 2003
  http://www.unpbook.com
  *
  * Although it's been considerably modified over time -- don't blame the authors''
*/

#include "ow.h"
#include <stdarg.h>
#include <syslog.h>

static void err_format(char * format, int errno_save, const char * level_string, const char * file, int line, const char * func, const char * fmt);

/* See man page for explanation */
int log_available = 0;

/* Limits for prettier byte printing */

/* Print message and return to caller
 * Caller specifies "errnoflag" and "level" */
#define MAXLINE     1023

void err_msg(enum e_err_type errnoflag, enum e_err_level level, const char * file, int line, const char * func, const char *fmt, ...)
{
	int errno_save = (errnoflag==e_err_type_error) ? errno:0;		/* value caller might want printed */
	char format[MAXLINE + 3];
	char buf[MAXLINE + 3];
	enum e_err_print sl;		// 2=console 1=syslog
	va_list ap;
	const char * level_string;

	switch (level) {
	case e_err_default:
		level_string = "DEFAULT: ";
		break;
	case e_err_call:
		level_string = "   CALL: ";
		break;
	case e_err_data:
		level_string = "   DATA: ";
		break;
	case e_err_detail:
		level_string = " DETAIL: ";
		break;
	case e_err_debug:
	case e_err_beyond:
	default:
		level_string = "  DEBUG: ";
		break;
	}
	printf ("%s%s:%d (%s)\n", level_string, file, line, func);

	/* Print where? */
	switch (Globals.error_print) {
	case e_err_print_mixed:
		switch (Globals.daemon_status) {
			case e_daemon_want_bg:
			case e_daemon_bg:
				sl = e_err_print_syslog ;
				break ;
			default:
				sl = e_err_print_console;
				break ;
		}
		break;
	case e_err_print_syslog:
		sl = e_err_print_syslog;
		break;
	case e_err_print_console:
		sl = e_err_print_console;
		break;
	default:
		printf ("error_print=%d, daemon=%d %s\n", Globals.error_print, Globals.daemon_status, buf);

		return;
	}
	va_start(ap, fmt);
	err_format( format, errno_save, level_string, file, line, func, fmt) ;

	/* Create output string */
#ifdef    HAVE_VSNPRINTF
	vsnprintf(buf, MAXLINE, format, ap);	/* safe */
#else
	vsprintf(buf, format, ap);		/* not safe */
#endif
	va_end(ap);

	sl = e_err_print_console;
	if (sl == e_err_print_syslog) {	/* All output to syslog */
		if (!log_available) {
			openlog("OWFS", LOG_PID, LOG_DAEMON);
			log_available = 1;
		}
		syslog(level <= e_err_default ? LOG_INFO : LOG_NOTICE, "%s\n", buf);
	} else {
		fflush(stdout);			/* in case stdout and stderr are the same */
		fputs(buf, stderr);
		fputs("\n", stderr);
		fflush(stderr);
	}

	return;
}

/* calls exit() so never returns */
void fatal_error(const char * file, int line, const char * func, const char *fmt, ...)
{
	va_list ap;
	char format[MAXLINE + 1];
	char buf[MAXLINE + 1];
	enum e_err_print sl;		// 2=console 1=syslog
	va_start(ap, fmt);

	err_format( format, 0, "FATAL ERROR: ", file, line, func, fmt) ;

#ifdef OWNETC_OW_DEBUG
	{
		fprintf(stderr, "%s:%d ", file, line);
#ifdef HAVE_VSNPRINTF
		vsnprintf(buf, MAXLINE, format, ap);
#else
		vsprintf(buf, fmt, ap);
#endif
		fprintf(stderr, "%s", buf);
	}
#else /* OWNETC_OW_DEBUG */
	if(Globals.fatal_debug) {

#ifdef HAVE_VSNPRINTF
		vsnprintf(buf, MAXLINE, format, ap);
#else
		vsprintf(buf, format, ap);
#endif
		/* Print where? */
		switch (Globals.error_print) {
			case e_err_print_mixed:
				switch (Globals.daemon_status) {
					case e_daemon_want_bg:
					case e_daemon_bg:
						sl = e_err_print_syslog ;
						break ;
					default:
						sl = e_err_print_console;
						break ;
				}
				break;
			case e_err_print_syslog:
				sl = e_err_print_syslog;
				break;
			case e_err_print_console:
				sl = e_err_print_console;
				break;
			default:
				va_end(ap);
				return;
		}

		if (sl == e_err_print_syslog) {	/* All output to syslog */
			if (!log_available) {
				openlog("OWFS", LOG_PID, LOG_DAEMON);
				log_available = 1;
			}
			syslog(LOG_USER|LOG_INFO, "%s\n", buf);
		} else {
			fflush(stdout);			/* in case stdout and stderr are the same */
			fputs(buf, stderr);
			fprintf(stderr,"\n");
			fflush(stderr);
		}
	}


	if(Globals.fatal_debug_file != NULL) {
		FILE *fp;
		char filename[64];
		sprintf(filename, "%s.%d", Globals.fatal_debug_file, getpid());
		if((fp = fopen(filename, "a")) != NULL) {
			if(!Globals.fatal_debug) {
#ifdef HAVE_VSNPRINTF
				vsnprintf(buf, MAXLINE, format, ap);
#else
				vsprintf(buf, format, ap);
#endif
			}
			fprintf(fp, "%s:%d %s\n", file, line, buf);
			fclose(fp);
		}
	}
#endif /* OWNETC_OW_DEBUG */
	va_end(ap);
	//exit(EXIT_FAILURE) ;
}

static void err_format(char * format, int errno_save, const char * level_string, const char * file, int line, const char * func, const char * fmt)
{
	/* Create output string */
#ifdef    HAVE_VSNPRINTF
	if (errno_save) {
		snprintf(format, MAXLINE, "%s%s:%s(%d) [%s] %s", level_string,file,func,line,strerror(errno_save),fmt);	/* safe */
	} else {
		snprintf(format, MAXLINE, "%s%s:%s(%d) %s", level_string,file,func,line,fmt);	/* safe */
	}
#else
	if (errno_save) {
		sprintf(format, "%s%s:%s(%d) [%s] %s", level_string,file,func,line,strerror(errno_save),fmt);		/* not safe */
	} else {
		sprintf(format, "%s%s:%s(%d) %s", level_string,file,func,line,fmt);		/* not safe */
	}
#endif
	/* Add CR at end */
}

