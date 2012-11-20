#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <syslog.h>

#include "debug.h"
#include "daemonise.h"



/* TODO allow the default value of this to be set at build or run time? */
int log_level = LOG_INFO;



/*
 * Using the log levels from syslog for easy integration later if desired.
 * This means that the highest priority messages have the lowest integer values.
 * Values range from LOG_EMERG=0 to LOG_DEBUG=7, so need to check that the
 * given priority is actually less than our logging level.
 *
 * LOG_ALERT: user action must be taken, program won't run (config files etc)
 * LOG_ERR: something has gone quite wrong, program will terminate
 * LOG_WARNING: something has gone wrong, but program will continue
 * LOG_NOTICE: normal but significant condition (not used yet? remove?)
 * LOG_INFO: informational but non-critical messages about program status
 * LOG_DEBUG: debug-level messages
 */
void Log(int priority, const char *fmt, ...)
{
    va_list ap;
    char buffer[513];
    char *prefix;

    assert(daemonised == 0 || daemonised == 1);

    /* don't print anything if the priority doesn't meet minimum requirements */
    if ( priority > log_level ) {
	return;
    }

    /* 
     * ideally this shouldn't be needed, but the syslog prioritynames array is 
     * out of order and is all lowercase.
     */
    switch ( priority ) {
	case LOG_EMERG: prefix = "EMERG"; break;
	case LOG_ALERT: prefix = "ALERT"; break;
	case LOG_CRIT: prefix = "CRIT"; break;
	case LOG_ERR: prefix = "ERR"; break;
	case LOG_WARNING: prefix = "WARNING"; break;
	case LOG_NOTICE: prefix = "NOTICE"; break;
	case LOG_INFO: prefix = "INFO"; break;
	case LOG_DEBUG: prefix = "DEBUG"; break;
	default: prefix = "???"; break;
    };

    va_start(ap, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);

    /* write the log message to the appropriate place */
    if ( !daemonised ) {
	time_t ts = time(NULL);
	char date[32];

	/* format date and chop newline from end of formatted string */
	ctime_r(&ts, date);
	date[strlen(date)-1] = '\0';

	/* chop any newline that is in the error message */
	if ( buffer[strlen(buffer)-1] == '\n' )
	    buffer[strlen(buffer)-1] = '\0';

	fprintf(stderr, "%s %s: %s\n", date, prefix, buffer);

    } else {
	//TODO print to our own file or use syslog if setup right
	//syslog(priority, "%s", buffer);
    }
    va_end(ap);

}