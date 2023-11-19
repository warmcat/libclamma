/*
 * libclamma - llama2 C library derived from llama2.c
 *
 * private.h
 *
 * Written in 2010-2023 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

/*
 * This SS is the read end of a pipe() created with the clamma_session_query().
 * The write end of the pipe is filled on the llm service thread, this lets us
 * collect the output on the lws event loop thread cleanly.
 */

LWS_SS_USER_TYPEDEF
	struct lws_ss_handle	*ss_client;
        char                    payload[2048];
        size_t                  head;
        size_t                  tail;
        size_t			pending;
        size_t			total;

	char			eom_when_empty;
	struct txf_session	*ts;

} clamma_genpipe_t;

/*
 * These SS are created when a client connection appears at the web server.
 */

LWS_SS_USER_TYPEDEF
        int			fd[2];
        clamma_genpipe_t	*inward;

        char			eom_when_empty;
} clamma_srv_t;

extern const lws_ss_info_t	ssi_clamma_genpipe_t;
extern const lws_ss_info_t	ssi_clamma_srv_t;
extern char			model_desc[256];
extern struct txf		*t;
extern clamma_sem_t		sem_kick;
