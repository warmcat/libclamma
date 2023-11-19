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
 *
 * This test app clamma-selftest-absolute checks that the library produces the
 * expected inference for a deterministic prompt generation when given the
 * model checkpoint already in memory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

#include "clamma.h"

struct test_vector {
	const char *string;
	const char *expected;
};

static const struct test_vector vectors[] = {
	{
		.string   = "My cat ",
		.expected = "My cat The old man was very wise and one day the "
			    "three-year-old child asked him: \"Old man, why are "
			    "you so wise?\" \n"
			    "The old man smiled and replied: \"Because I have "
			    "lived a long time and I have learned many things.\"\n"
			    "The child'"
	},
};

struct test_gather {
	char buf[2048];
	size_t pos;
};

static int
iss_cb(void *opaque_user_pointer, const char *piece)
{
	struct test_gather *g = (struct test_gather *)opaque_user_pointer;

	g->pos += snprintf(g->buf + g->pos, sizeof(g->buf) - 1, "%s", piece);
	// fprintf(stderr, "%s\n", g->buf);

	return 0;
}

/*
 * We're going to manually mmap the model checkpoint, and pass in the map
 * address to clamma, simulating a memory-mapped read-only copy of the model
 * and the construction api used for this case.
 */

int
main(int argc, char *argv[])
{
	clamma_txf_info_t info;
	struct test_gather gather;
	struct txf_session *ts;
	int ret = 1, fd;
	struct txf *t;

	memset(&info, 0, sizeof(info));
	info.clamma_api_version = CLAMMA_API_VERSION;

	info.model_access = CLAMMA_MODEL_ACCESS_ABSOLUTE_ADDRESS;
	info.tokenizer_path = "tokenizer.bin";
	info.checkpoint_path = argc > 2 ? argv[1] : "stories110M.bin";

	info.opaque_user_pointer = &gather;
	info.temperature = 1.0f;
	info.issue_cb = iss_cb;
	info.rng_seed = 0x1234; /* ie, deterministic */
	info.topp = 0.9f;

	/*
	 * This manual mmap outside clamma simulates availability of the data
	 * directly in the memory map somehow already, eg, in flash.
	 *
	 * If you want libclamma to access the model checkpoint file by mmap,
	 * you don't have to copy this, just use
	 * info.model_access = CLAMMA_MODEL_ACCESS_MMAP
	 */

	fd = open(info.checkpoint_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Couldn't open file %s\n", info.checkpoint_path);
		goto bail;
	}

	info.model_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	info.model_base = mmap(NULL, info.model_size, PROT_READ, MAP_PRIVATE,
				fd, 0);
	if (info.model_base == MAP_FAILED) {
		fprintf(stderr, "MMAP failed %s\n", argv[1]);
		goto bail1;
	}

	/*
	 * Now we did the mmap() to fake up in-memory access, we can create the
	 * transformer and do the test
	 */

	t = clamma_txf_construct(&info);
	if (!t)
		goto bail2;

	ts = clamma_session_construct(t);
	if (!ts)
		goto bail3;

	for (size_t n = 0; n < CLAMMA_ARRAY_SIZE(vectors); n++) {

		memset(&gather, 0, sizeof(gather));

		info.limit = 64;
		info.prompt = vectors[n].string;
		if (clamma_session_query(ts, &info))
			goto bail4;

		while (clamma_sessions_step_next())
			;

		if (gather.pos == strlen(vectors[n].expected) &&
		    !memcmp(vectors[n].expected, gather.buf,
			    gather.pos))
			continue;

		fprintf(stderr, "test %d: got %ld rather than %ld chars\n",
				(int)n, (unsigned long)gather.pos,
				(unsigned long)strlen(vectors[n].expected));

		for (size_t m = 0; m < gather.pos; m++)
			fprintf(stderr, "%02X ", (uint8_t)gather.buf[m]);

		fprintf(stderr, "\n");
		goto bail4;
	}

	ret = 0;
	printf("ALL OK\n");

bail4:
	clamma_session_destroy(ts);
bail3:
	clamma_txf_destroy(t);
bail2:
	munmap(info.model_base, info.model_size);
bail1:
	close(fd);
bail:
	return ret;
}
