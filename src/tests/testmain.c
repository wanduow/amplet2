/*
 * This file is part of amplet2.
 *
 * Copyright (c) 2013-2016 The University of Waikato, Hamilton, New Zealand.
 *
 * Author: Brendon Jones
 *
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND
 * research group. For further information please see http://www.wand.net.nz/
 *
 * amplet2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations including
 * the two.
 *
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL. If you modify file(s) with this
 * exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do
 * so, delete this exception statement from your version. If you delete
 * this exception statement from all source files in the program, then
 * also delete it here.
 *
 * amplet2 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with amplet2. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>

#include "debug.h"
#include "tests.h"
#include "modules.h"
#include "ssl.h"
#include "global.h" /* just for temporary ssl testing stuff */
#include "ampresolv.h"
#include "testlib.h"
#include "../measured/control.h" /* just for CONTROL_PORT */

/* function all test modules must define to register themselves */
test_t *register_test(void);

struct option standalone_long_options[] = {
    {"cacert", required_argument, 0, '0'},
    {"cert", required_argument, 0, '9'},
    {"key", required_argument, 0, '8'},
    {"dns", required_argument, 0, '7'},
    {"dns-server", required_argument, 0, '7'},
    {"debug", no_argument, 0, 'x'},
    {"help", no_argument, 0, 'h'},
    {NULL, 0, 0, 0}
};



/*
 * Generic main function to allow all tests to be run as both normal binaries
 * and AMP libraries. This function will deal with converting command line
 * arguments into test arguments and a list of destinations (such as AMP
 * provides when it runs the tests).
 *
 * Arguments to the test should be provided as normal, and any destinations
 * included on the end after a -- seperator. For example:
 *
 * ./foo -a 1 -b 2 -c -- 10.0.0.1 10.0.0.2 10.0.0.3
 */
int main(int argc, char *argv[]) {
    test_t *test_info;
    struct addrinfo **dests;
    struct addrinfo *addrlist = NULL, *rp;
    int count;
    int opt;
    int i;
    char *nameserver = NULL;
    pthread_mutex_t addrlist_lock;
    int forcev4 = 0;
    int forcev6 = 0;
    char *sourcev4 = NULL;
    char *sourcev6 = NULL;
    amp_test_result_t *result;
    int test_argc;
    char **test_argv;
    int do_ssl;

    /* there should be only a single test linked, so register it directly */
    test_info = register_test();

    /* suppress "invalid argument" errors from getopt */
    opterr = 0;

    /* start building new argv for the test, which will be a subset of argv */
    test_argc = 1;
    test_argv = calloc(2, sizeof(char*));
    test_argv[0] = argv[0];

    /*
     * deal with command line arguments - split them into actual arguments
     * and destinations in the style the AMP tests want. Using "-" as the
     * optstring means that non-option arguments are treated as having an
     * option with character code 1, which makes different style arguments
     * (both styles of long arguments, and short ones) appear consistently.
     * We could have not used it so that unknown arguments are shuffled to
     * the end of the list and then taken just the argv array after the last
     * known argument, but for some reason the permutation isn't working?
     */
    while ( (opt = getopt_long(argc, argv, "-x0:9:8:7:4::6::",
                    standalone_long_options, NULL)) != -1 ) {
	/* generally do nothing, just use up arguments until the -- marker */
        switch ( opt ) {
            /* -x is an option only we care about for now - enable debug */
            case 'x': log_level = LOG_DEBUG;
                      log_level_override = 1;
                      break;
            /* nameserver config is also only for us and not passed on */
            case '7': nameserver = optarg;
                      break;
            /* use these for nameserver config, but also pass onto the test */
            case '4': forcev4 = 1;
                      sourcev4 = parse_optional_argument(argv);
                      test_argv[test_argc++] = "-4";
                      test_argv = realloc(test_argv,
                              (test_argc+1) * sizeof(char*));
                      if ( sourcev4 ) {
                          test_argv[test_argc++] = sourcev4;
                          test_argv = realloc(test_argv,
                                  (test_argc+1) * sizeof(char*));
                      }
                      break;
            case '6': forcev6 = 1;
                      sourcev6 = parse_optional_argument(argv);
                      test_argv[test_argc++] = "-6";
                      test_argv = realloc(test_argv,
                              (test_argc+1) * sizeof(char*));
                      if ( sourcev6 ) {
                          test_argv[test_argc++] = sourcev6;
                          test_argv = realloc(test_argv,
                                  (test_argc+1) * sizeof(char*));
                      }
                      break;
            /* configure ssl certs if we want to talk to a real server */
            case '0': vars.amqp_ssl.cacert = optarg;
                      break;
            case '9': vars.amqp_ssl.cert = optarg;
                      break;
            case '8': vars.amqp_ssl.key = optarg;
                      break;
            /* add any unknown options to a new argv for the test */
            default:  test_argv[test_argc++] = argv[optind-1];
                      test_argv = realloc(test_argv,
                              (test_argc+1) * sizeof(char*));
                      break;
        };
    }

    /* null terminate the new argv for the test */
    test_argv[test_argc] = NULL;

    /* make sure all or none of the SSL settings are set */
    if ( vars.amqp_ssl.cacert || vars.amqp_ssl.cert || vars.amqp_ssl.key ) {
        if ( !vars.amqp_ssl.cacert || !vars.amqp_ssl.cert ||
                !vars.amqp_ssl.key ) {
            Log(LOG_WARNING, "SSL needs --cacert, --cert and --key to be set");
            exit(EXIT_FAILURE);
        }
        do_ssl = 1;
    } else {
        do_ssl = 0;
    }

    /* set the nameserver to our custom one if specified */
    if ( nameserver ) {
        /* TODO we could parse the string and get up to MAXNS servers */
        vars.ctx = amp_resolver_context_init(&nameserver, 1, sourcev4,sourcev6);
    } else {
        vars.ctx = amp_resolver_context_init(NULL, 0, sourcev4, sourcev6);
    }

    if ( vars.ctx == NULL ) {
        Log(LOG_ALERT, "Failed to configure resolver, aborting.");
        exit(EXIT_FAILURE);
    }

    dests = NULL;
    count = 0;
    pthread_mutex_init(&addrlist_lock, NULL);

    /* process all destinations */
    for ( i=optind; i<argc; i++ ) {
        int family;

	/* check if adding the new destination would be allowed by the test */
	if ( test_info->max_targets > 0 &&
                (i-optind) >= test_info->max_targets ) {
	    /* ignore any extra destinations but continue with the test */
	    printf("Exceeded max of %d destinations, skipping remainder\n",
		    test_info->max_targets);
	    break;
	}

        /* limit name resolution if address families are specified */
        if ( forcev4 && !forcev6 ) {
            family = AF_INET;
        } else if ( forcev6 && !forcev4 ) {
            family = AF_INET6;
        } else {
            family = AF_UNSPEC;
        }

        /* TODO update max targets and pass through to the resolver? */
        amp_resolve_add(vars.ctx, &addrlist, &addrlist_lock, argv[i],
                family, -1);
    }

    if ( optind < argc ) {
        /* wait for all the responses to come in */
        ub_wait(vars.ctx);
    }

    /* add all the results of to the list of destinations */
    for ( rp=addrlist; rp != NULL; rp=rp->ai_next ) {
	if ( test_info->max_targets > 0 && count >= test_info->max_targets ) {
	    /* ignore any extra destinations but continue with the test */
	    printf("Exceeded max of %d destinations, skipping remainder\n",
		    test_info->max_targets);
	    break;
	}

        /* make room for a new destination and fill it */
        dests = realloc(dests, (count + 1) * sizeof(struct addrinfo*));
        dests[count] = rp;
        count++;

        /*
         * Remove the destination from the argument list if it is still
         * present. Involves iterating over argv once for each address, but
         * there shouldn't be too many when run interactively. The usefulness
         * of the error checking will hopefully outweigh the time spent doing
         * this.
         */
        for ( i=optind; i<argc; i++ ) {
            if ( argv[i] == NULL ) {
                continue;
            }

            /* null the destination to mark it as resolved */
            if ( strcmp(argv[i], rp->ai_canonname) == 0 ) {
                argv[i] = NULL;
                continue;
            }
        }
    }

    /* check if any destinations didn't resolve so we can warn the user */
    for ( i=optind; i<argc; i++ ) {
        if ( argv[i] != NULL ) {
            Log(LOG_WARNING, "Host '%s' did not resolve!", argv[i]);
        }
    }

    /*
     * Initialise SSL if the test requires a remote server and SSL
     * configuration has been provided. This can either be used to start a
     * remote server on an amplet client, or to talk securely to a standalone
     * test server.
     */
    if ( test_info->server_callback != NULL && do_ssl ) {
        /*
         * These need values for standalone tests to work with remote servers,
         * but there aren't really any good default values we can use. If the
         * user wants to test to a real server, they will need to specify the
         * locations of all the certs/keys/etc.
         */
        if ( initialise_ssl(&vars.amqp_ssl, NULL) < 0 ) {
            Log(LOG_ALERT, "Failed to initialise SSL, aborting");
            exit(EXIT_FAILURE);
        }
        if ( (ssl_ctx = initialise_ssl_context(&vars.amqp_ssl)) == NULL ) {
            Log(LOG_ALERT, "Failed to initialise SSL context, aborting");
            exit(EXIT_FAILURE);
        }
    }

    Log(LOG_DEBUG, "test_argc: %d, test_argv:", test_argc);
    for ( i = 0; i < test_argc; i++ ) {
        Log(LOG_DEBUG, "test_argv[%d] = %s\n", i, test_argv[i]);
    }

    /* reset optind so the test can call getopt normally on it's arguments */
    optind = 1;

    /* make sure the RNG is seeded so the tests don't have to worry */
    srandom(time(NULL));

    /* pass arguments and destinations through to the main test run function */
    result = test_info->run_callback(test_argc, test_argv, count, dests);

    if ( result ) {
        test_info->print_callback(result);
        free(result->data);
        free(result);
    }

    amp_resolve_freeaddr(addrlist);


    /* tidy up after ourselves */
    if ( dests ) {
        free(dests);
    }

    free(test_argv);

    ub_ctx_delete(vars.ctx);

    if ( ssl_ctx != NULL ) {
        ssl_cleanup();
    }

    free(test_info->name);
    free(test_info);

    return EXIT_SUCCESS;
}
