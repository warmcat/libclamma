/*
 * libclamma - llama2 C library derived from llama2.c
 *
 * ss-clamma-srv.c
 *
 * Written in 2010-2023 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <libwebsockets.h>
#include <clamma.h>

#include "private.h"

static const struct lws_http_mount mount[] = {
	{ /* second, catch everything else, including /, serve as static */
	.mountpoint	 = "/",
	.mountpoint_len  = 1,
	.origin		 = CLAMMA_INSTALL_SHARE,
	.def		 = "index.html",
	.origin_protocol = LWSMPRO_FILE,
	},
	{ /* first exclude /clamma from the static mount that is next */
	.mount_next	 = &mount[0],
	.mountpoint	 = "/clamma",
	.mountpoint_len  = 7,
	.origin		 = CLAMMA_INSTALL_SHARE,
	.def		 = "index.html",
	.origin_protocol = LWSMPRO_NO_MOUNT, /* handle as dynamic */
	},
};

/*
 * Called from llm session thread.  We put the data into a pipe, so we can
 * collect it from the lws event loop at the read end of the pipe from lws
 * thread cleanly
 */

static int
issue_cb(void *opaque_user_pointer, const char *piece)
{
	clamma_srv_t *g = (clamma_srv_t *)opaque_user_pointer;
	ssize_t n;

	if (!g)
		return 0;

	n = write(g->fd[1], piece, strlen(piece));
	if (n != (ssize_t)strlen(piece))
		lwsl_warn("%s: pipe write failed %d\n", __func__, errno);

	return 0;
}

/*
 * From lws event loop thread, return collected tokens on this session / query
 * when we're ready to send more
 */

static lws_ss_state_return_t
clamma_srv_tx(void *userobj, lws_ss_tx_ordinal_t ord, uint8_t *buf, size_t *len,
              int *flags)
{
        clamma_srv_t *g = (clamma_srv_t *)userobj;
        lws_ss_state_return_t r = LWSSSSRET_OK;
        size_t a;

        (void)ord;

        if (!g->inward) {
        	a = strlen(model_desc) < *len ? strlen(model_desc) : *len;
        	memcpy(buf, model_desc, a);
        	*len = a;
        	*flags = LWSSS_FLAG_SOM | LWSSS_FLAG_EOM;
        	return r;
        }

        if (!g->inward->pending)
                return LWSSSSRET_TX_DONT_SEND;

        if (*len > g->inward->pending)
                *len = g->inward->pending;

        *flags = 0;
        if (!g->inward->total)
                *flags |= LWSSS_FLAG_SOM;

        a = *len;
        if (g->inward->tail + a > sizeof(g->inward->payload)) {
        	size_t b = sizeof(g->inward->payload) - g->inward->tail;
        	memcpy(buf, g->inward->payload + g->inward->tail, b);
        	memcpy(buf + b, g->inward->payload, *len - b);
        	g->inward->tail = *len - b;
        } else {
        	memcpy(buf, g->inward->payload + g->inward->tail, *len);
        	g->inward->tail = (g->inward->tail + *len ) %
        				sizeof(g->inward->payload);
        }

        g->inward->total += *len;
        g->inward->pending = (g->inward->head - g->inward->tail) %
        			sizeof(g->inward->payload);

        if (g->inward->pending)
		r = lws_ss_request_tx_len(lws_ss_from_user(g),
        				  (unsigned long)g->inward->pending);
        else
        	if (g->inward->eom_when_empty)
        		*flags |= LWSSS_FLAG_EOM;

        return r;
}

static lws_ss_state_return_t
clamma_srv_state(void *userobj, void *sh, lws_ss_constate_t state,
                 lws_ss_tx_ordinal_t ack)
{
        clamma_srv_t *g = (clamma_srv_t *)userobj;
        lws_sock_file_fd_type sffd;
        struct lws_ss_handle *pss;
        struct txf_session *ts;
	clamma_txf_info_t info;
	struct lws_vhost *v;
	const char *path;
	size_t plen = 0;
	char desc[512];
	ssize_t w;	
	int n;

        (void)sh;
        (void)ack;

        switch ((int)state) {
        case LWSSSCS_CREATING:
                return lws_ss_request_tx(lws_ss_from_user(g));

        case LWSSSCS_CONNECTING:
        	lwsl_ss_debug(lws_ss_from_user(g), "LWSSSCS_CONNECTING\n");
        	v = lws_ss_get_vhost(lws_ss_from_user(g));
        	lws_vhost_set_mounts(v, &mount[1]);
        	break;

        case LWSSSCS_SERVER_TXN:
                /*
                 * A transaction is starting on an accepted connection.  Say
                 * that we're OK with the transaction, prepare the user
                 * object with the response, and request tx to start sending it.
                 */

        	lwsl_ss_debug(lws_ss_from_user(g), "LWSSSCS_SERVER_TXN\n");

        	if (lws_ss_get_metadata(lws_ss_from_user(g), "path",
        			        (const void **)&path, &plen))
        		return LWSSSSRET_DISCONNECT_ME;


        	if (plen == 12 && !memcmp(path, "/clamma/desc", 12)) {

                        lws_ss_server_ack(lws_ss_from_user(g), 0);
                        g->eom_when_empty = 1;

                        return lws_ss_request_tx_len(lws_ss_from_user(g),
                        				strlen(model_desc));
        	}

        	memset(&info, 0, sizeof(info));
        	info.system			= NULL;
        	info.prompt			= NULL;
        	info.limit			= 0;
        	info.issue_cb			= issue_cb;
        	info.temperature		= 1.0f;
        	info.topp			= 0.9f;
        	info.opaque_user_pointer	= g;

        	n = pipe(g->fd);
        	if (n < 0) {
        		lwsl_err("%s: pipe() failed %d\n", __func__, n);
        		goto bail;
        	}

        	if (lws_ss_create(lws_ss_cx_from_user(g), 0,
        			  &ssi_clamma_genpipe_t, g, &pss, NULL, NULL)) {
        		lwsl_ss_warn(lws_ss_from_user(g),
        				"Failed to create pipe ss\n");
        		goto bail1;
        	}

        	sffd.filefd		= g->fd[0];
        	g->inward		= lws_ss_to_user_object(pss);
        	g->inward->ss_client	= lws_ss_from_user(g);

        	if (lws_ss_adopt_raw(pss, sffd)) {
        		lwsl_ss_warn(lws_ss_from_user(g),
        				"Failed to adopt pipe on to ss\n");
        		goto bail1;
        	}

        	ts = clamma_session_construct(t);
        	if (!ts)
        		goto bail1;

        	g->inward->ts		= ts;
        	info.desc		= desc;
        	info.desc_max		= sizeof(desc);

        	if (clamma_session_query(ts, &info))
        		goto bail2;

        	w = write(g->fd[1], desc, strlen(desc));
		if (w != (ssize_t)strlen(desc))
			fprintf(stderr, "write failed (%ld vs %lu) errno %d\n",
					(long)w, (unsigned long)strlen(desc), errno);

        	clamma_sem_post(&sem_kick);

                /*
                 * Say that we're OK with the transaction, prepare the user
                 * object with the response, and request tx to start sending it.
                 */

                lws_ss_server_ack(lws_ss_from_user(g), 0);

                return LWSSSSRET_OK;

        case LWSSSCS_DISCONNECTED:
        	lwsl_ss_debug(lws_ss_from_user(g), "LWSSSCS_DISCONNECTED\n");
        	if (!g->inward)
        		break;

        	/*
        	 * Since we have a connection to the inward pipe, tell it to
        	 * shut down and sever its link to us.
        	 */

        	lws_ss_start_timeout(lws_ss_from_user(g->inward), 1);
        	if (g->inward->ts)
        		clamma_sessions_query_cancel(g->inward->ts);
        	g->inward->ss_client = NULL;
        	break;
        }

        return LWSSSSRET_OK;

bail2:
	clamma_session_destroy(ts);
bail1:
	close(g->fd[0]);
	close(g->fd[1]);
bail:
	return LWSSSSRET_DISCONNECT_ME;
}

LWS_SS_INFO("clamma_srv", clamma_srv_t)
	.tx		= clamma_srv_tx,
        .state		= clamma_srv_state,
};
