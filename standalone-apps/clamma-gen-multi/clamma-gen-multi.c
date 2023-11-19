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
 * This example app runs by default two query sessions against the model
 * concurrently (you can select how many using -c <count>).
 *
 * These are output to /tmp/out0.txt, /tmp/out1.txt etc.
 *
 * The concurrent sessions have a RAM requirement each that depends on the
 * model.  For stories110M.bin, it's a relatively manageable 72MB, but for
 * llama2_7b_q80.bin, even with int8 quantization, it's 2GB.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <clamma.h>

#define MAX_SESSIONS 16

static int
issue_cb(void *opaque_user_pointer, const char *piece)
{
	int fd = (int)(long)opaque_user_pointer;
	ssize_t w;

	w = write(fd, piece, strlen(piece));
	if (w != (ssize_t)strlen(piece))
		fprintf(stderr, "write failed (%ld vs %lu) errno %d\n",
				(long)w, (unsigned long)strlen(piece), errno);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ret = 1, queries = 2, n, fd[MAX_SESSIONS];
	const char *prompt = NULL, *system = NULL;
	struct txf_session *ts[MAX_SESSIONS];
	unsigned int steps = 256;
	clamma_txf_info_t info;
	char path[64];
	struct txf *t;

	memset(ts, 0, sizeof(ts));

	memset(&info, 0, sizeof(info));
	info.clamma_api_version = CLAMMA_API_VERSION;
	info.tokenizer_path = "tokenizer.bin";
	info.checkpoint_path = argv[1];

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
		case 'm': info.model_access = atoi(argv[i + 1]); break;
		case 'c': queries = atoi(argv[i + 1]); break;
		case 'h': info.threads = atoi(argv[i + 1]); break;
		default:
			goto usage;
		}
	}

	t = clamma_txf_construct(&info);
	if (!t)
		goto bail;

	for (n = 0; n < queries; n++) {
		ts[n] = clamma_session_construct(t);
		if (!ts[n])
			goto bail1;

		snprintf(path, sizeof(path) - 1, "/tmp/out%d.txt", n);
		fd[n] = open(path, O_RDWR | O_CREAT | O_TRUNC, 0600);
		if (fd[n] < 0) {
			fprintf(stderr, "unable to open %s: %d\n", path, errno);
			goto bail2;
		}

		info.system = system;
		info.prompt = prompt;
		info.limit = steps;
		info.issue_cb = issue_cb;
		info.temperature = 1.0f;
		info.topp = 0.9f;
		info.opaque_user_pointer = (void *)(long)fd[n];
		info.null_on_destroy = (void **)&ts[n];

		if (clamma_session_query(ts[n], &info))
			goto bail2;
	}

	while (clamma_sessions_step_next())
		;

	ret = 0;

bail2:
	for (n = 0; n < queries; n++) {
		if (fd[n] > -1)
			close(fd[n]);

		if (ts[n])
			clamma_session_destroy(ts[n]);
	}
bail1:
	clamma_txf_destroy(t);
bail:
	return ret;

usage:
	fprintf(stderr, "Usage:   clamma-gen-multi <checkpoint> [options]\n"
			"Example: clamma-gen-multi model.bin -n 256 -i \"Once upon a time\"\n"
			"Options:\n"
			"  -t <float>  temperature in [0,inf], default 1.0\n"
			"  -p <float>  p value in top-p (nucleus) sampling in [0,1] default 0.9\n"
			"  -s <int>    random seed, default time(NULL)\n"
			"  -n <int>    number of steps to run for, default 256. 0 = max_seq_len\n"
			"  -z <string> optional path to custom tokenizer\n"
			"  -i <string> input prompt\n"
			"  -y <string> (optional) system prompt\n"
			"  -h <count>  Number of concurrent threads\n"
			"  -m <0-1>    model access method (0=mmap, 1=malloc cache)\n"
			"  -c <int>    Count of simultaneous query sessions\n");

	return 1;
}
