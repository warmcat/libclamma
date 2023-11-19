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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <clamma.h>

static int
issue_cb(void *opaque_user_pointer, const char *piece)
{
	(void)opaque_user_pointer;
	fprintf(stdout, "%s", piece);
	fflush(stdout);

	return 0;
}

int
main(int argc, char *argv[])
{
	const char *prompt = NULL, *system = NULL;
	unsigned int steps = 256;
	clamma_txf_info_t info;
	struct txf_session *ts;
	char inbuf[4096];
	struct txf *t;
	int ret = 1;

	memset(&info, 0, sizeof(info));
	info.clamma_api_version		= CLAMMA_API_VERSION;
	info.model_type			= CLAMMA_MODEL_CHAT;
	info.checkpoint_path		= argv[1];

	info.temperature = 1.0f;
	info.topp = 0.9f;

	if (argc < 2)
		goto usage;


	for (int i = 2; i < argc; i += 2) {
		if (i + 1 >= argc || argv[i][0] != '-' || strlen(argv[i]) != 2)
			goto usage;
		switch (argv[i][1]) {
		case 't': info.temperature = atof(argv[i + 1]); break;
		case 'p': info.topp = atof(argv[i + 1]); break;
		case 's': info.rng_seed = atoi(argv[i + 1]); break;
		case 'n': steps = (unsigned int)atol(argv[i + 1]); break;
		case 'y': system = argv[i + 1]; break;
		case 'i': prompt = argv[i + 1]; break;
		case 'z': info.tokenizer_path = argv[i + 1]; break;
		case 'h': info.threads = atoi(argv[i + 1]); break;
		default:
			goto usage;
		}
	}

	t = clamma_txf_construct(&info);
	if (!t)
		goto bail;

	info.prompt = prompt;
	do {
		ssize_t n;

		ts = clamma_session_construct(t);
		if (!ts)
			goto bail1;

		info.system		= system;
		info.limit		= steps;
		info.issue_cb		= issue_cb;

		if (clamma_session_query(ts, &info))
			goto bail2;

		while (clamma_sessions_step_next())
			;

		fprintf(stdout, "\nprompt: ");
		fflush(stdout);

		n = read(0, inbuf, sizeof(inbuf) - 1);
		if (n <= 0)
			break;
		inbuf[n] = '\0';
		info.prompt = inbuf;

	} while (1);

	ret = 0;

bail2:
	clamma_session_destroy(ts);
bail1:
	clamma_txf_destroy(t);
bail:
	return ret;

usage:
	fprintf(stderr, "Usage:   clamma-chat <checkpoint> [options]\n"
			"Example: clamma-chat model.bin -n 256 -i \"Once upon a time\"\n"
			"Options:\n"
			"  -t <float>  temperature in [0,inf], default 1.0\n"
			"  -p <float>  p value in top-p (nucleus) sampling in [0,1] default 0.9\n"
			"  -s <int>    random seed, default time(NULL)\n"
			"  -n <int>    number of steps to run for, default 256. 0 = max_seq_len\n"
			"  -z <string> optional path to custom tokenizer\n"
			"  -h <count>  Number of concurrent threads\n"
			"  -i <string> input prompt\n"
			"  -y <string> (optional) system prompt\n");

	return 1;
}
