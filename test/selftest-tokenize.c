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
 * This test app clamma-selftest-tokenize checks that the library produces the
 * expected inference output for known prompts (the same text) showing that the
 * roundtrip through the tokenizer and detokenizer works.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "clamma.h"

struct test_vector {
	const char *string;
	const char *expected;
};

static const struct test_vector vectors[] = {
	{
		.string   = "I believe the meaning of life is ",
		.expected = "I believe the meaning of life is The two friends, "
			    "Jack and Mary, saw the avocado tree and Jack said, "
			    "“I can see why the tree is so big.” \n"
			    "Mary was curious, so she asked “What makes it so big?” \n"
			    "Jack said, “The"
	},
	{
		.string   = "When life gives you lemons ",
		.expected = "When life gives you lemons One day, a little girl "
			    "called Rachel and her best friend, Lola, were "
			    "walking through a field full of beautiful flowers. "
			    "Rachel noticed a lemon and said, “Let’s pick this lemon!”\n"
			    "Lola replied, “No, Rach"
	},
};

struct test_gather {
	char buf[16384];
	size_t pos;
};


static int
iss_cb(void *opaque_user_pointer, const char *piece)
{
	struct test_gather *g = (struct test_gather *)opaque_user_pointer;

	g->pos += snprintf(g->buf + g->pos, sizeof(g->buf) - 1, "%s", piece);

	// fprintf(stdout, "%s\n", g->buf);
	// fflush(stdout);

	return 0;
}

int
main(int argc, char *argv[])
{
	clamma_txf_info_t info;
	struct test_gather gather;
	struct txf_session *ts;
	struct txf *t;
	int ret = 1;

	memset(&info, 0, sizeof(info));
	info.clamma_api_version = CLAMMA_API_VERSION;
	info.model_access = CLAMMA_MODEL_ACCESS_MMAP;
	info.checkpoint_path = argc > 1 ? argv[1] : "stories110M.bin";
	info.tokenizer_path = "tokenizer.bin";

	info.opaque_user_pointer = &gather;
	info.temperature = 1.0f;
	info.issue_cb = iss_cb;
	info.rng_seed = 0x1234; /* ie, deterministic */
	info.topp = 0.9f;

	t = clamma_txf_construct(&info);
	if (!t)
		goto bail;

	ts = clamma_session_construct(t);
	if (!ts)
		goto bail1;

	for (size_t n = 0; n < CLAMMA_ARRAY_SIZE(vectors); n++) {
		memset(&gather, 0, sizeof(gather));

		info.prompt = vectors[n].string;
		info.limit = 64;

		if (clamma_session_query(ts, &info))
			goto bail2;

		while (clamma_sessions_step_next())
			;

		if (gather.pos == strlen(vectors[n].expected) &&
		    !memcmp(vectors[n].expected, gather.buf, gather.pos))
			continue;

		fprintf(stderr, "test %d: got %ld rather than %ld tokens\n",
				(int)n, (unsigned long)gather.pos,
				(unsigned long)strlen(vectors[n].expected));

		for (size_t m = 0; m < gather.pos; m++)
			fprintf(stderr, "%02X ", (uint8_t)gather.buf[m]);

		fprintf(stderr, "\n");
		goto bail2;
	}

	ret = 0;
	printf("ALL OK\n");

bail2:
	clamma_session_destroy(ts);
bail1:
	clamma_txf_destroy(t);
bail:
	if (ret)
		printf("FAILED\n");

	return ret;
}
