/*
 * libclamma - llama2 C library derived from llama2.c
 *
 * clamma-ws-demo.c
 *
 * Written in 2010-2023 by Andy Green <andy@warmcat.com>
 *
 * This file is made available under the Creative Commons CC0 1.0
 * Universal Public Domain Dedication.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <libwebsockets.h>
#include <clamma.h>

#include "private.h"

static const char * const policy = "{"
	"\"release\":\"01234567\","
	"\"product\":\"clammaserver\","
	"\"schema-version\":1,"
	"\"retry\": [{\"default\": {\"backoff\": [1000,2000,3000,5000,10000],"
	                           "\"conceal\":5,\"jitterpc\":20,"
				   "\"svalidping\":300,\"svalidhup\":310}}],"
	"\"s\": [ {"
		"\"clamma_srv\": {"
			"\"server\":true,"
			"\"port\":7681,"
			"\"protocol\":\"h1\","
			"\"http_no_content_length\":true,"
			"\"metadata\": ["
                          "{\"mime\": \"Content-Type:\","
			   "\"clen\": \"Content-Length:\","
                           "\"method\": \"\",\"path\": \"\"}"
                        "]},"
		"\"clamma_srv_unix\": {"
			"\"server\":true,"
		        "\"endpoint\":"     "\"+@com.warmcat.clamma_demo\","
			"\"protocol\":\"h1\","
			"\"http_no_content_length\":true,"
			"\"metadata\": ["
                          "{\"mime\": \"Content-Type:\","
			   "\"clen\": \"Content-Length:\","
                           "\"method\": \"\",\"path\": \"\"}"
                        "]},"
		"\"genpipe\": {\"protocol\":\"raw\"}"
		"}"
	"]"
"}";

char				model_desc[256];
clamma_sem_t			sem_kick;
struct txf			*t;

static int			test_result;
static char			going_down;
static clamma_thread_t		pt_llm;
static struct lws_context	*cx;

static int
smd_cb(void *opaque, lws_smd_class_t c, lws_usec_t ts, void *buf, size_t len)
{
	(void)opaque;
	(void)ts;

        if (!(c & LWSSMDCL_SYSTEM_STATE) ||
            lws_json_simple_strcmp(buf, len, "\"state\":", "OPERATIONAL") ||
            !lws_ss_create(cx, 0, &ssi_clamma_srv_t, NULL, NULL, NULL, NULL))
                return 0;

        lwsl_err("%s: failed to create secure stream\n", __func__);
        lws_default_loop_exit(cx);

        return -1;
}

static void
sigint_handler(int sig)
{
	(void)sig;
	going_down = 1;
	clamma_sem_post(&sem_kick);
        lws_default_loop_exit(cx);
}

/*
 * Separate thread to round-robin the llm work, so that we don't block the
 * lws event loop with it.  We use a pipe in the token emit callback to collect
 * the output on the read end of the pipe which has its own ss on the lws event
 * loop.
 */

static void *
llm_worker(void *tp)
{
	int ret = 1;

	(void)tp;

	clamma_sem_init(&sem_kick);
	do {
		do {
			;
		} while (clamma_sessions_step_next());

		clamma_sem_wait(&sem_kick);

	} while (!going_down);
	clamma_sem_destroy(&sem_kick);

	ret = 0;

	lwsl_user("%s: worker: exiting\n", __func__);

	return (void *)(intptr_t)ret;
}

int
main(int argc, const char *argv[])
{
        struct lws_context_creation_info info;
        clamma_txf_info_t ti;
        void *tret;

        lws_context_info_defaults(&info, policy);
        info.default_loglevel = LLL_USER | LLL_ERR | LLL_WARN;
        lws_cmdline_option_handle_builtin(argc, argv, &info);
        signal(SIGINT, sigint_handler);

        lwsl_user("Clamma + lws web service demo\n");

        info.early_smd_cb               = smd_cb;
        info.early_smd_class_filter     = LWSSMDCL_SYSTEM_STATE;
        info.fd_limit_per_thread        = 128;

	memset(&ti, 0, sizeof(ti));
	ti.clamma_api_version		= CLAMMA_API_VERSION;
	ti.tokenizer_path		= "tokenizer.bin";
	ti.checkpoint_path		= "stories110M.bin";
	ti.desc				= model_desc;
	ti.desc_max			= sizeof(model_desc);

	t = clamma_txf_construct(&ti);
	if (!t)
		goto bail;

        cx = lws_create_context(&info);
        if (!cx)
        	goto bail;

	if (pthread_create(&pt_llm, NULL, llm_worker, NULL)) {
		lwsl_err("llm thread startfail\n");
		goto bail1;
	}

        lws_context_default_loop_run_destroy(cx);

        cx = NULL;
        going_down = 1;
        clamma_sem_post(&sem_kick);
        pthread_join(pt_llm, &tret);

	clamma_txf_destroy(t);

        /* process ret 0 if actual is as expected (0, or--expected-exit 123) */

        return lws_cmdline_passfail(argc, argv, test_result);


bail1:
	lws_context_destroy(cx);
bail:
	lwsl_err("lws init failed\n");

	return 1;
}
