/*
 * libclamma - llama2 C library derived from llama2.c
 *
 * See https://github.com/karpathy/llama2.c for MIT-licensed original
 *
 * Changes Copyright (C) 2023 Andy Green <andy@warmcat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#if !defined(__LLC_CLAMMA_PUBLIC_H__)
#define __LLC_CLAMMA_PUBLIC_H__

/*
 * These are the public apis for libclamma, everything else is private.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* bump this when struct clamma_txf_info layout changes */
#define CLAMMA_API_VERSION	0xabcd0101

#define TOK_BOS (1)
#define TOK_EOS (2)

#define CLAMMA_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#if defined(__LLC_PRIVATE_H__)
#define CLAMMA_VISIBLE __attribute__((__visibility__("default")))
#else
#define CLAMMA_VISIBLE
#endif

#if defined(LIBCLAMMA_SMP)

#if defined(LIBCLAMMA_WITH_PTHREADS)
#include <pthread.h>
#include <semaphore.h>

typedef pthread_mutex_t clamma_mutex_t;
typedef pthread_t clamma_thread_t;

#define clamma_mutex_init(x)    pthread_mutex_init(x, NULL)
#define clamma_mutex_destroy(x) pthread_mutex_destroy(x)
#define clamma_mutex_lock(x)    pthread_mutex_lock(x)
#define clamma_mutex_unlock(x)  pthread_mutex_unlock(x)

#if defined(__APPLE__)
#include <dispatch/dispatch.h>

typedef dispatch_semaphore_t clamma_sem_t;
static inline int clamma_sem_init(clamma_sem_t * p) { *p = dispatch_semaphore_create(0); return !(*p); }
#define clamma_sem_destroy(x) (void)(x)
#define clamma_sem_post(x)      dispatch_semaphore_signal(*(x))
#define clamma_sem_wait(x)      dispatch_semaphore_wait(*(x), DISPATCH_TIME_FOREVER)

#else
typedef sem_t clamma_sem_t;
#define clamma_sem_init(x)      sem_init(x, 0, 0)
#define clamma_sem_destroy(x)   sem_destroy(x)
#define clamma_sem_post(x)      sem_post(x)
#define clamma_sem_wait(x)      sem_wait(x)
#endif

#endif
#else

#define clamma_mutex_init(x)
#define clamma_mutex_destroy(x)
#define clamma_mutex_lock(x)
#define clamma_mutex_unlock(x)

#endif

typedef int tok_id_t;
typedef float txi_t;

/*
 * These are completely opaque to user code... the constructor allocates the
 * correct sized object on the heap and returns it if successful
 */

struct txf;
struct txf_session;

typedef int (*issue_cb_t)(void *opaque_user_pointer, const char *token);

typedef enum {
	CLAMMA_MODEL_ACCESS_MMAP, /* file-backed mmap() access (default) */
	CLAMMA_MODEL_ACCESS_MALLOC_CACHE, /* no mmap, use malloc'd cache */
	CLAMMA_MODEL_ACCESS_ABSOLUTE_ADDRESS /* just access it from memory */
} clamma_model_access_t;

typedef enum {
	CLAMMA_MODEL_GEN, /**< just combine any prompts inline, non-chat */
	CLAMMA_MODEL_CHAT /**< use llama2 chat style <<SYS>> */
} clamma_model_type_t;

/*
 * Transformer and session construction use the same info struct, in the
 * common case you only have one session, you can just fill it in once
 * and use it for both construction calls.
 */

typedef struct clamma_txf_info {

	/*
	 * this section used for transformer construction + config,
	 * ignored for session construction
	 */

	/**> MANDATORY: CLAMMA_API_VERSION used to detect app / lib api match */
	unsigned int		clamma_api_version;
	/**> NULL if CLAMMA_MODEL_ACCESS_ABSOLUTE_ADDRESS, else path to the
		 * model checkpoint file */
	const char		*checkpoint_path;
	/**> NULL for "./tokenizer.bin", or filepath to the tokenizer */
	const char		*tokenizer_path;

	/**> 0 for default MMAP (if platform supports) */
	clamma_model_access_t	model_access;
	/**> NULL or base ads for CLAMMA_MODEL_ACCESS_ABSOLUTE_ADDRESS */
	void			*model_base;
	/**> 0 or byte count of model if CLAMMA_MODEL_ACCESS_ABSOLUTE_ADDRESS */
	size_t			model_size;
	/**> 0 or max malloc cache limit for CLAMMA_MODEL_ACCESS_MALLOC_CACHE */
	size_t			cache_limit;
	/**> 0 for default (8 if smp enabled, 1 if single-threaded), else count
	 * of threads to spawn for concurrent matrix math processing */
	unsigned int		threads;
	/**> CLAMMA_MODEL_GEN or CLAMMA_MODEL_CHAT */
	clamma_model_type_t	model_type;
	/**> 0 for unlimited, or max sessions allowed */
	unsigned int		max_sessions;
	/**> UI name for this transformer, or empty string */
	char			name[32];
	/**> NULL, or buffer to receive model configuration description */
	char			*desc;
	/**> 0, or size of buffer at desc */
	size_t			desc_max;

	/*
	 * this section used for session construction + query,
	 * ignored for transformer construction
	 */

	/**> per-token callback, if NULL then default print to stderr cb used */
	issue_cb_t		issue_cb;
	/**> opaque user pointer that's available in the token issue callback */
	void			*opaque_user_pointer;
	/**> NULL = none, else system prompt to use for query */
	const char		*system;
	/**> NULL = none, else prompt to use for query */
	const char		*prompt;
	/**> 0 = max for model, else requested max tokens to generate (clipped
	 * to the model maximum)  */
	size_t			limit;
	/**> 0 = default */
	float			temperature;
	/**> 0 = default */
	float			topp;
	/**> 0 = PRNG based on time, else use the seed given */
	uint64_t		rng_seed;
	/**> NULL, or pointer to pointer to set to NULL on session destroy */
	void			**null_on_destroy;

} clamma_txf_info_t;

/**
 * \b clamma_txf_construct() - construct a clamma transformer
 *
 * \p info: struct describing details of transformer to create
 *
 * \p info struct should be zero'd down and at least .clamma_api_version set to
 * CLAMMA_API_VERSION.
 *
 * On success, returns an opaque pointer to an allocated and initialized +
 * configured transformer using the model given in info->checkpoint_path.
 *
 * On failure, returns NULL with nothing allocated.
 */
CLAMMA_VISIBLE struct txf *
clamma_txf_construct(const clamma_txf_info_t *info);

/**
 * clamma_txf_destroy() - destroy a previously allocated clamma transformer
 *
 * \p t: the previously allocated transformer
 *
 * Frees any allocations inside the transformer and then the transformer itself.
 */
CLAMMA_VISIBLE void
clamma_txf_destroy(struct txf *t);


/**
 * \b clamma_session_construct() - construct a clamma transformer session
 *
 * \p info struct should be zero'd down and at least .clamma_api_version set to
 * CLAMMA_API_VERSION.
 *
 * On success, returns an opaque pointer to an allocated and initialized +
 * configured transformer using the model given in info->checkpoint_path.
 *
 * On failure, returns NULL with nothing allocated.
 */
CLAMMA_VISIBLE struct txf_session *
clamma_session_construct(const struct txf *t);

/**
 * clamma_session_destroy() - destroy a previously allocated clamma transformer
 *
 * \p t: the previously allocated transformer
 *
 * Frees any allocations inside the transformer and then the transformer itself.
 */
CLAMMA_VISIBLE void
clamma_session_destroy(struct txf_session *ts);

/**
 * clamma_txf_query() - feed the transformer a prompt and receive output
 *
 * \p ts: the transformer session object
 * \p system: the system prompt utf-8 string
 * \p prompt: the incoming prompt utf-8 string
 * \p limit: 0, or the max number of tokens to produce
 *
 * Encodes the prompt string, iteratively runs the transformer with it and
 * sends any output tokens to the sessions's issue_cb() along with the session's
 * opaque pointer set at session creation time.
 *
 * The actual number of tokens to be produced is restricted by the model
 * checkpoint sequence length discovered at runtime.
 *
 * returns 0 if successful.
 */
CLAMMA_VISIBLE int
clamma_session_query(struct txf_session *ts,
		     const clamma_txf_info_t *info);

/**
 * clamma_sessions_step_next() - make the next token for the next query session
 *
 * Issues the next token for the next active query session in round-robin mode.
 *
 * Returns 1 if a token was produced, or 0 if there's no longer any active query.
 * Keep calling this to generate the output callbacks for each running query
 * session in turn, until it returns 0 and is idle until another
 * clamma_session_query() is started.
 */
CLAMMA_VISIBLE int
clamma_sessions_step_next(void);

/**
 * clamma_sessions_query_cancel() - mark session as needing to be cancelled
 *
 * \p ts: the transformer session object
 *
 * Used if something used in the callback is itself about to be destroyed.  This
 * stops any further callbacks and marks the session for deletion next time it
 * is stepped.
 */
CLAMMA_VISIBLE void
clamma_sessions_query_cancel(struct txf_session *ts);

#endif
