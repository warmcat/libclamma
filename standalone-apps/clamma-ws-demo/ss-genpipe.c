/*
 * libclamma - llama2 C library derived from llama2.c
 *
 * ss-genpipe.c
 *
 * Written in 2010-2023 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libwebsockets.h>
#include <clamma.h>

#include "private.h"

/* this is all happening on the lws event loop thread */

static lws_ss_state_return_t
clamma_genpipe_rx(void *userobj, const uint8_t *buf, size_t len, int flags)
{
	clamma_genpipe_t *g = (clamma_genpipe_t *)userobj;

	(void)flags;

	while (len--) {
		if (*buf == TOK_EOS) {
			g->eom_when_empty = 1;
			g->payload[g->head] = ' ';
			g->head = (g->head + 1) % sizeof(g->payload);
			break;
		}
		g->payload[g->head] = *buf++;
		g->head = (g->head + 1) % sizeof(g->payload);
	}

	g->pending = (g->head - g->tail) % sizeof(g->payload);

	if (g->pending && g->ss_client)
		return lws_ss_request_tx_len(g->ss_client,
					     (unsigned long)g->pending);

	return LWSSSSRET_OK;
}

static lws_ss_state_return_t
clamma_genpipe_state(void *userobj, void *sh, lws_ss_constate_t state,
               lws_ss_tx_ordinal_t ack)
{
	clamma_genpipe_t *g = (clamma_genpipe_t *)userobj;

	lwsl_ss_debug(lws_ss_from_user(g), "state %s",
				lws_ss_state_name(state));
	(void)sh;
	(void)ack;

	return LWSSSSRET_OK;
}

LWS_SS_INFO("genpipe", clamma_genpipe_t)
	.rx		= clamma_genpipe_rx,
        .state		= clamma_genpipe_state,
};
