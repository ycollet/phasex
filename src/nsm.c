/*****************************************************************************
 *
 * nsm.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#include "phasex.h"
#include "config.h"
#include "nsm.h"


int nsm_active = 0;


#ifdef HAVE_LIBLO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <lo/lo.h>
#include "bank.h"
#include "session.h"
#include "debug.h"


/* NSM announce API version we speak. */
#define NSM_API_VERSION_MAJOR   1
#define NSM_API_VERSION_MINOR   2

/* How long to wait for the session manager to send /nsm/client/open
   before giving up and running standalone. */
#define NSM_OPEN_TIMEOUT_MS     10000
#define NSM_POLL_INTERVAL_MS    50


static lo_address   nsm_addr            = NULL;
static lo_server    nsm_server          = NULL;

static char     *nsm_session_path       = NULL;
static char     *nsm_client_id          = NULL;

static volatile int  nsm_open_pending    = 0;
static volatile int  nsm_open_failed     = 0;


static void
nsm_reply(const char *path, const char *message)
{
	lo_send_from(nsm_addr, nsm_server, LO_TT_IMMEDIATE, "/reply", "ss", path, message);
}


/*****************************************************************************
 * nsm_message_from_daemon()
 *
 * Reject anything not sent from the same port we announced to.  The OSC
 * wire protocol has no authentication, so this is only a sanity check, not
 * real authentication -- but it's cheap and rules out unrelated senders.
 *
 * Only the port is compared, not the host:  nsm_addr's hostname is whatever
 * textual host NSM_URL happened to use (often the machine's own hostname),
 * while lo_message_get_source() always reports the numeric peer address of
 * the packet actually received (e.g. "127.0.0.1"); comparing those strings
 * would reject the real daemon whenever the two representations don't match
 * textually, which is common in practice.
 *****************************************************************************/
static int
nsm_message_from_daemon(lo_message msg)
{
	lo_address  src;
	const char  *src_port;
	const char  *daemon_port;

	if ((src = lo_message_get_source(msg)) == NULL) {
		return 0;
	}

	src_port    = lo_address_get_port(src);
	daemon_port = lo_address_get_port(nsm_addr);

	if ((src_port == NULL) || (daemon_port == NULL)) {
		return 0;
	}

	return (strcmp(src_port, daemon_port) == 0);
}


/*****************************************************************************
 * nsm_path_is_safe()
 *
 * Reject empty paths and paths containing a ".." component, so a spoofed
 * open message can't point save/load at an arbitrary directory outside of
 * whatever the session manager intended.
 *****************************************************************************/
static int
nsm_path_is_safe(const char *path)
{
	size_t  len;

	if ((path == NULL) || (path[0] == '\0')) {
		return 0;
	}

	if ((strcmp(path, "..") == 0) ||
	    (strncmp(path, "../", 3) == 0) ||
	    (strstr(path, "/../") != NULL)) {
		return 0;
	}

	len = strlen(path);
	if ((len >= 3) && (strcmp(path + len - 3, "/..") == 0)) {
		return 0;
	}

	return 1;
}


static int
nsm_open_handler(const char *path, const char *UNUSED(types), lo_arg **argv,
                  int argc, lo_message msg, void *UNUSED(user_data))
{
	if (!nsm_message_from_daemon(msg)) {
		PHASEX_WARN("NSM: ignoring '%s' from unexpected sender.\n", path);
		return 0;
	}

	if (argc < 3) {
		return 0;
	}

	if (!nsm_path_is_safe(&argv[0]->s)) {
		PHASEX_ERROR("NSM: refusing unsafe session path '%s'.\n", &argv[0]->s);
		lo_send_from(nsm_addr, nsm_server, LO_TT_IMMEDIATE, "/error", "sis",
		             path, 1, "Unsafe session path.");
		return 0;
	}

	if (nsm_session_path != NULL) {
		free(nsm_session_path);
	}
	nsm_session_path = strdup(&argv[0]->s);

	if (nsm_client_id != NULL) {
		free(nsm_client_id);
	}
	nsm_client_id = strdup(&argv[2]->s);

	PHASEX_DEBUG(DEBUG_CLASS_SESSION, "NSM: open '%s' (client_id=%s)\n",
	             nsm_session_path, nsm_client_id);

	nsm_reply(path, "phasex is ready to roll.");

	nsm_open_pending = 0;

	return 0;
}


static int
nsm_save_handler(const char *path, const char *UNUSED(types), lo_arg **UNUSED(argv),
                  int UNUSED(argc), lo_message msg, void *UNUSED(user_data))
{
	if (!nsm_message_from_daemon(msg)) {
		PHASEX_WARN("NSM: ignoring '%s' from unexpected sender.\n", path);
		return 0;
	}

	if (nsm_session_path == NULL) {
		nsm_reply(path, "No session to save.");
		return 0;
	}

	if (save_session(nsm_session_path, visible_sess_num, 1) != 0) {
		lo_send_from(nsm_addr, nsm_server, LO_TT_IMMEDIATE, "/error", "sis",
		             path, 1, "Unable to save session.");
		PHASEX_ERROR("NSM: unable to save session to '%s'.\n", nsm_session_path);
		return 0;
	}

	PHASEX_DEBUG(DEBUG_CLASS_SESSION, "NSM: saved session to '%s'\n", nsm_session_path);

	nsm_reply(path, "Session saved.");

	return 0;
}


static int
nsm_error_handler(const char *UNUSED(path), const char *UNUSED(types), lo_arg **argv,
                   int argc, lo_message UNUSED(msg), void *UNUSED(user_data))
{
	if (argc >= 3) {
		PHASEX_ERROR("NSM: error replying to '%s':  %s\n", &argv[0]->s, &argv[2]->s);
	}

	nsm_open_failed = 1;

	return 0;
}


/*****************************************************************************
 * nsm_init()
 *
 * If NSM_URL is set in the environment, announce ourselves to the session
 * manager and block (briefly) waiting for it to tell us where our session
 * data lives.  Returns 0 if now running under session management, or -1 if
 * NSM is not in use (or the handshake failed / timed out).
 *****************************************************************************/
int
nsm_init(const char *exe_name)
{
	const char  *nsm_url;
	int         waited_ms = 0;

	nsm_url = getenv("NSM_URL");
	if (nsm_url == NULL) {
		return -1;
	}

	nsm_server = lo_server_new(NULL, NULL);
	if (nsm_server == NULL) {
		PHASEX_ERROR("NSM: unable to create OSC server.\n");
		return -1;
	}

	nsm_addr = lo_address_new_from_url(nsm_url);
	if (nsm_addr == NULL) {
		PHASEX_ERROR("NSM: invalid NSM_URL '%s'.\n", nsm_url);
		lo_server_free(nsm_server);
		nsm_server = NULL;
		return -1;
	}

	lo_server_add_method(nsm_server, "/nsm/client/open", "sss", nsm_open_handler, NULL);
	lo_server_add_method(nsm_server, "/nsm/client/save", "", nsm_save_handler, NULL);
	lo_server_add_method(nsm_server, "/error", "sis", nsm_error_handler, NULL);

	nsm_open_pending = 1;
	nsm_open_failed  = 0;

	lo_send_from(nsm_addr, nsm_server, LO_TT_IMMEDIATE, "/nsm/client/announce", "sssiii",
	             "PHASEX", "", exe_name,
	             NSM_API_VERSION_MAJOR, NSM_API_VERSION_MINOR, (int) getpid());

	while (nsm_open_pending && !nsm_open_failed && (waited_ms < NSM_OPEN_TIMEOUT_MS)) {
		lo_server_recv_noblock(nsm_server, NSM_POLL_INTERVAL_MS);
		waited_ms += NSM_POLL_INTERVAL_MS;
	}

	if (nsm_open_pending || nsm_open_failed) {
		PHASEX_WARN("NSM: session announce failed or timed out.  Running standalone.\n");
		lo_address_free(nsm_addr);
		lo_server_free(nsm_server);
		nsm_addr   = NULL;
		nsm_server = NULL;
		return -1;
	}

	nsm_active = 1;

	return 0;
}


/*****************************************************************************
 * nsm_check_events()
 *
 * Called periodically from the audio watchdog loop to process incoming
 * requests (currently just /nsm/client/save) from the session manager.
 *****************************************************************************/
void
nsm_check_events(void)
{
	if (!nsm_active) {
		return;
	}

	lo_server_recv_noblock(nsm_server, 0);
}


/*****************************************************************************
 * nsm_finish()
 *****************************************************************************/
void
nsm_finish(void)
{
	if (nsm_addr != NULL) {
		lo_address_free(nsm_addr);
		nsm_addr = NULL;
	}
	if (nsm_server != NULL) {
		lo_server_free(nsm_server);
		nsm_server = NULL;
	}
	if (nsm_session_path != NULL) {
		free(nsm_session_path);
		nsm_session_path = NULL;
	}
	if (nsm_client_id != NULL) {
		free(nsm_client_id);
		nsm_client_id = NULL;
	}

	nsm_active = 0;
}


const char *
nsm_get_session_path(void)
{
	return nsm_session_path;
}


const char *
nsm_get_client_id(void)
{
	return nsm_client_id;
}


#else /* !HAVE_LIBLO */


int
nsm_init(const char *UNUSED(exe_name))
{
	return -1;
}


void
nsm_check_events(void)
{
}


void
nsm_finish(void)
{
}


const char *
nsm_get_session_path(void)
{
	return NULL;
}


const char *
nsm_get_client_id(void)
{
	return NULL;
}


#endif /* HAVE_LIBLO */
