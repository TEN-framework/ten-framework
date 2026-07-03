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
 * A helper for running multiple unit tests against abstract protocols.
 *
 * An lws_seq_t is used to base its actions in the event loop and manage
 * the sequencing of multiple tests.  A new abstract connection is instantiated
 * for each test using te
 */

#include <private-lib-core.h>

struct lws_seq_test_sequencer {
	lws_abs_t			original_abs;

	lws_test_sequencer_args_t	args;

	struct lws_context		*context;
	struct lws_vhost		*vhost;
	struct lws_sequencer		*unit_test_seq;

	/* holds the per-test token for the unit-test transport to consume */
	lws_token_map_t			uttt[4];

	lws_abs_t			*instance;

	int				state;
};

/* sequencer messages specific to this sequencer */

enum {
	SEQ_MSG_PASS = LWSSEQ_USER_BASE,
	SEQ_MSG_FAIL,
	SEQ_MSG_FAIL_TIMEOUT,
};

/*
 * We get called back when the unit test transport has decided if the test
 * passed or failed.  We get the priv, and report to the sequencer message queue
 * what the result was.
 */

static int
unit_test_result_cb(const void *cb_user, int disposition)
{
	const struct lws_seq_test_sequencer *s =
			(const struct lws_seq_test_sequencer *)cb_user;
	int r;

	lwsl_debug("%s: disp %d\n", __func__, disposition);

	switch (disposition) {
	case LPE_FAILED_UNEXPECTED_PASS:
	case LPE_FAILED_UNEXPECTED_CLOSE:
	case LPE_FAILED:
		r = SEQ_MSG_FAIL;
		break;

	case LPE_FAILED_UNEXPECTED_TIMEOUT:
		r = SEQ_MSG_FAIL_TIMEOUT;
		break;

	case LPE_SUCCEEDED:
		r = SEQ_MSG_PASS;
		break;

	default:
		assert(0);
		return -1;
	}

	lws_seq_queue_event(s->unit_test_seq, r, NULL, NULL);

	((struct lws_seq_test_sequencer *)s)->instance = NULL;

	return 0;
}

/*
 * We receive the unit test result callback's messages via the message queue.
 *
 * We log the results and always move on to the next test until there are no
 * more tests.
 */

static lws_seq_cb_return_t
test_sequencer_cb(struct lws_sequencer *seq, void *user, int event, void *data,
		  void *aux)
{
	struct lws_seq_test_sequencer *s =
				(struct lws_seq_test_sequencer *)user;
	lws_unit_test_packet_t *exp = (lws_unit_test_packet_t *)
					s->args.tests[s->state].expect_array;
	lws_abs_t test_abs;

	switch ((int)event) {
	case LWSSEQ_CREATED: /* our sequencer just got started */
		lwsl_notice("%s: %s: created\n", __func__,
			    lws_seq_name(seq));
		s->state = 0;  /* first thing we'll do is the first url */
		goto step;

	case LWSSEQ_DESTROYED:
		/*
		 * We are going down... if we have a child unit test sequencer
		 * still around inform and destroy it
		 */
		if (s->instance) {
			s->instance->at->close(s->instance);
			s->instance = NULL;
		}
		break;

	case SEQ_MSG_FAIL_TIMEOUT: /* current step timed out */
		if (exp->flags & LWS_AUT_EXPECT_SHOULD_TIMEOUT) {
			lwsl_user("%s: test %d got expected timeout\n",
				  __func__, s->state);

			goto pass;
		}
		lwsl_user("%s: seq timed out at step %d\n", __func__, s->state);

		s->args.results[s->state] = LPE_FAILED_UNEXPECTED_TIMEOUT;
		goto done; /* always move on to the next test */

	case SEQ_MSG_FAIL:
		if (exp->flags & LWS_AUT_EXPECT_SHOULD_FAIL) {
			/*
			 * in this case, we expected to fail like this, it's OK
			 */
			lwsl_user("%s: test %d failed as expected\n",
				  __func__, s->state);

			goto pass; /* always move on to the next test */
		}

		lwsl_user("%s: seq failed at step %d\n", __func__, s->state);

		s->args.results[s->state] = LPE_FAILED;
		goto done; /* always move on to the next test */

	case SEQ_MSG_PASS:
		if (exp->flags & (LWS_AUT_EXPECT_SHOULD_FAIL |
				  LWS_AUT_EXPECT_SHOULD_TIMEOUT)) {
			/*
			 * In these specific cases, done would be a failure,
			 * we expected to timeout or fail
			 */
			lwsl_user("%s: seq failed at step %d\n", __func__,
				  s->state);

			s->args.results[s->state] = LPE_FAILED_UNEXPECTED_PASS;

			goto done; /* always move on to the next test */
		}
		lwsl_info("%s: seq done test %d\n", __func__, s->state);
pass:
		(*s->args.count_passes)++;
		s->args.results[s->state] = LPE_SUCCEEDED;

done:
		lws_seq_timeout_us(lws_seq_from_user(s), LWSSEQTO_NONE);
		s->state++;
step:
		if (!s->args.tests[s->state].name) {
			/* the sequence has completed */
			lwsl_user("%s: sequence completed OK\n", __func__);

			if (s->args.cb)
				s->args.cb(s->args.cb_user);

			return LWSSEQ_RET_DESTROY;
		}
		lwsl_info("%s: starting test %d\n", __func__, s->state);

		if (s->state >= s->args.results_max) {
			lwsl_err("%s: results array is too small\n", __func__);

			return LWSSEQ_RET_DESTROY;
		}
		test_abs = s->original_abs;
		s->uttt[0].name_index = LTMI_PEER_V_EXPECT_TEST;
		s->uttt[0].u.value = (void *)&s->args.tests[s->state];
		s->uttt[1].name_index = LTMI_PEER_V_EXPECT_RESULT_CB;
		s->uttt[1].u.value = (void *)unit_test_result_cb;
		s->uttt[2].name_index = LTMI_PEER_V_EXPECT_RESULT_CB_ARG;
		s->uttt[2].u.value = (void *)s;
		/* give the unit test transport the test tokens */
		test_abs.at_tokens = s->uttt;

		s->instance = lws_abs_bind_and_create_instance(&test_abs);
		if (!s->instance) {
			lwsl_notice("%s: failed to create step %d unit test\n",
				    __func__, s->state);

			return LWSSEQ_RET_DESTROY;
		}
		(*s->args.count_tests)++;
		break;

	default:
		break;
	}

	return LWSSEQ_RET_CONTINUE;
}


/*
 * Creates an lws_sequencer to manage the test sequence
 */

int
lws_abs_unit_test_sequencer(const lws_test_sequencer_args_t *args)
{
	struct lws_seq_test_sequencer *s;
	struct lws_sequencer *seq;
	lws_seq_info_t i;

	memset(&i, 0, sizeof(i));
	i.context = args->abs->vh->context;
	i.user_size = sizeof(struct lws_seq_test_sequencer);
	i.puser = (void **)&s;
	i.cb = test_sequencer_cb;
	i.name = "test-seq";

	/*
	 * Create a sequencer in the event loop to manage the tests
	 */

	seq = lws_seq_create(&i);
	if (!seq) {
		lwsl_err("%s: unable to create sequencer\n", __func__);
		return 1;
	}

	/*
	 * Take a copy of the original lws_abs_t we were passed so we can use
	 * it as the basis of the lws_abs_t we create the individual tests with
	 */
	s->original_abs = *args->abs;

	s->args = *args;

	s->context = args->abs->vh->context;
	s->vhost = args->abs->vh;
	s->unit_test_seq = seq;

	*s->args.count_tests = 0;
	*s->args.count_passes = 0;

	return 0;
}
