 /*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010 - 2019 Andy Green <andy@warmcat.com>
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
 * lws_test_sequencer manages running an array of unit tests.
 */

typedef void (*lws_test_sequence_cb)(const void *cb_user);

typedef struct lws_test_sequencer_args {
	lws_abs_t		*abs; /* abstract protocol + unit test txport */
	lws_unit_test_t	*tests; /* array of lws_unit_test_t */
	int			*results; /* takes result dispositions */
	int			results_max; /* max space usable in results */
	int			*count_tests; /* count of done tests */
	int			*count_passes; /* count of passed tests */
	lws_test_sequence_cb	cb; /* completion callback */
	void			*cb_user; /* opaque user ptr given to cb */
} lws_test_sequencer_args_t;

/**
 * lws_abs_unit_test_sequencer() - helper to sequence multiple unit tests
 *
 * \param args: lws_test_sequencer_args_t prepared with arguments for the tests
 *
 * This helper sequences one or more unit tests to run and collects the results.
 *
 * The incoming abs should be set up for the abstract protocol you want to test
 * and the lws unit-test transport.
 *
 * Results are one of
 *
 * 	LPE_SUCCEEDED
 *	LPE_FAILED
 *	LPE_FAILED_UNEXPECTED_TIMEOUT
 *	LPE_FAILED_UNEXPECTED_PASS
 *	LPE_FAILED_UNEXPECTED_CLOSE
 *
 * The callback args->cb is called when the tests have been done.
 */
LWS_VISIBLE LWS_EXTERN int
lws_abs_unit_test_sequencer(const lws_test_sequencer_args_t *args);
