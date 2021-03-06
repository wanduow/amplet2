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

#ifndef _MEASURED_ASNSOCK_H
#define _MEASURED_ASNSOCK_H

#include <event2/event.h>

#include "iptrie.h"
#include "asn.h"

/*
 * Ideally the cache could refresh individual items and expunge those
 * that haven't been used for a while. In the meantime, lets just empty
 * the whole cache and start again every 24 hours + 0-60 minutes.
 */
#define MIN_ASN_CACHE_REFRESH 86400
#define MAX_ASN_CACHE_REFRESH_OFFSET 3600

void asn_socket_event_callback(evutil_socket_t evsock,
        __attribute__((unused))short flags, void *evdata);

struct amp_asn_info* initialise_asn_info(void);
void amp_asn_info_delete(struct amp_asn_info *info);
#endif
