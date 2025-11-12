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
 */

#if !defined(__LWS_PLUGIN_SSH_H__)
#define __LWS_PLUGIN_SSH_H__

#define LWS_CALLBACK_SSH_UART_SET_RXFLOW (LWS_CALLBACK_USER + 800)

#define LWS_SSH_OPS_VERSION 2

struct lws_ssh_pty {
	char term[16];
	char *modes;
	uint32_t width_ch;
	uint32_t height_ch;
	uint32_t width_px;
	uint32_t height_px;
	uint32_t modes_len;
};

#define SSHMO_TTY_OP_END 0 /* Indicates end of options. */
#define SSHMO_VINTR	 1 /* Interrupt character; 255 if none.  Similarly
			    * for the other characters.  Not all of these
			    * characters are supported on all systems. */
#define SSHMO_VQUIT	 2 /* The quit character (sends SIGQUIT signal on
			    * POSIX systems). */
#define SSHMO_VERASE	 3 /* Erase the character to left of the cursor. */
#define SSHMO_VKILL	 4 /* Kill the current input line. */
#define SSHMO_VEOF	 5 /* End-of-file character (sends EOF from the
			    * terminal). */
#define SSHMO_VEOL	 6 /* End-of-line character in addition to
			    * carriage return and/or linefeed. */
#define SSHMO_VEOL2	 7 /* Additional end-of-line character. */
#define SSHMO_VSTART	 8 /* Continues paused output (normally
			    * control-Q). */
#define SSHMO_VSTOP	 9 /* Pauses output (normally control-S). */
#define SSHMO_VSUSP	10 /* Suspends the current program. */
#define SSHMO_VDSUSP	11 /* Another suspend character. */
#define SSHMO_VREPRINT	12 /* Reprints the current input line. */
#define SSHMO_VWERASE	13 /* Erases a word left of cursor. */
#define SSHMO_VLNEXT	14 /* Enter the next character typed literally,
			    * even if it is a special character */
#define SSHMO_VFLUSH	15 /* Character to flush output. */
#define SSHMO_VSWTCH	16 /* Switch to a different shell layer. */
#define SSHMO_VSTATUS	17 /* Prints system status line (load, command,
			    * pid, etc). */
#define SSHMO_VDISCARD	18 /* Toggles the flushing of terminal output. */
#define SSHMO_IGNPAR	30 /* The ignore parity flag.  The parameter
			    * SHOULD be 0 if this flag is FALSE,
			    * and 1 if it is TRUE. */
#define SSHMO_PARMRK	31 /* Mark parity and framing errors. */
#define SSHMO_INPCK	32 /* Enable checking of parity errors. */
#define SSHMO_ISTRIP	33 /* Strip 8th bit off characters. */
#define SSHMO_INLCR	34 /* Map NL into CR on input. */
#define SSHMO_IGNCR	35 /* Ignore CR on input. */
#define SSHMO_ICRNL	36 /* Map CR to NL on input. */
#define SSHMO_IUCLC	37 /* Translate uppercase characters to lowercase. */
#define SSHMO_IXON	38 /* Enable output flow control. */
#define SSHMO_IXANY	39 /* Any char will restart after stop. */
#define SSHMO_IXOFF	40 /* Enable input flow control. */
#define SSHMO_IMAXBEL	41 /* Ring bell on input queue full. */
#define SSHMO_ISIG	50 /* Enable signals INTR, QUIT, [D]SUSP. */
#define SSHMO_ICANON	51 /* Canonicalize input lines. */
#define SSHMO_XCASE	52 /* Enable input and output of uppercase
			    * characters by preceding their lowercase
			    * equivalents with "\". */
#define SSHMO_ECHO	53 /* Enable echoing. */
#define SSHMO_ECHOE	54 /* Visually erase chars. */
#define SSHMO_ECHOK	55 /* Kill character discards current line. */
#define SSHMO_ECHONL	56 /* Echo NL even if ECHO is off. */
#define SSHMO_NOFLSH	57 /* Don't flush after interrupt. */
#define SSHMO_TOSTOP	58 /* Stop background jobs from output. */
#define SSHMO_IEXTEN	59 /* Enable extensions. */
#define SSHMO_ECHOCTL	60 /* Echo control characters as ^(Char). */
#define SSHMO_ECHOKE	61 /* Visual erase for line kill. */
#define SSHMO_PENDIN	62 /* Retype pending input. */
#define SSHMO_OPOST	70 /* Enable output processing. */
#define SSHMO_OLCUC	71 /* Convert lowercase to uppercase. */
#define SSHMO_ONLCR	72 /* Map NL to CR-NL. */
#define SSHMO_OCRNL	73 /* Translate carriage return to newline (out). */
#define SSHMO_ONOCR	74 /* Translate newline to CR-newline (out). */
#define SSHMO_ONLRET	75 /* Newline performs a carriage return (out). */
#define SSHMO_CS7	90 /* 7 bit mode. */
#define SSHMO_CS8	91 /* 8 bit mode. */
#define SSHMO_PARENB	92 /* Parity enable. */
#define SSHMO_PARODD	93 /* Odd parity, else even. */
#define SSHMO_TTY_OP_ISPEED	128 /* Specifies the input baud rate in
				     * bits per second. */
#define SSHMO_TTY_OP_OSPEED	129 /* Specifies the output baud rate in
				     * bits per second. */

/*! \defgroup ssh-base plugin: lws-ssh-base
 * \ingroup Protocols-and-Plugins
 *
 * ##Plugin lws-ssh-base
 *
 * This is the interface to customize the ssh server per-vhost.  A pointer
 * to your struct lws_ssh_ops with the members initialized is passed in using
 * pvo when you create the vhost.  The pvo is attached to the protocol name
 *
 *  - "lws-ssh-base" - the ssh serving part
 *
 *  - "lws-telnetd-base" - the telnet serving part
 *
 *  This way you can have different instances of ssh servers wired up to
 *  different IO and server keys per-vhost.
 *
 *  See also ./READMEs/README-plugin-sshd-base.md
 */
///@{

typedef void (*lws_ssh_finish_exec)(void *handle, int retcode);

struct lws_ssh_ops {
	/**
	 * channel_create() - Channel created
	 *
	 * \param wsi: raw wsi representing this connection
	 * \param priv: pointer to void * you can allocate and attach to the
	 *		channel
	 *
	 * Called when new channel created, *priv should be set to any
	 * allocation your implementation needs
	 *
	 * You probably want to save the wsi inside your priv struct.  Calling
	 * lws_callback_on_writable() on this wsi causes your ssh server
	 * instance to call .tx_waiting() next time you can write something
	 * to the client.
	 */
	int (*channel_create)(struct lws *wsi, void **priv);

	/**
	 * channel_destroy() - Channel is being destroyed
	 *
	 * \param priv: void * you set when channel was created (or NULL)
	 *
	 * Called when channel destroyed, priv should be freed if you allocated
	 * into it.
	 */
	int (*channel_destroy)(void *priv);

	/**
	 * rx() - receive payload from peer
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param wsi:  struct lws * for the ssh connection
	 * \param buf:	pointer to start of received data
	 * \param len:	bytes of received data available at buf
	 *
	 * len bytes of payload from the peer arrived and is available at buf
	 */
	int (*rx)(void *priv, struct lws *wsi, const uint8_t *buf, uint32_t len);

	/**
	 * tx_waiting() - report if data waiting to transmit on the channel
	 *
	 * \param priv:	void * you set when this channel was created
	 *
	 * returns a bitmask of LWS_STDOUT and LWS_STDERR, with the bits set
	 * if they have tx waiting to send, else 0 if nothing to send
	 *
	 * You should use one of the lws_callback_on_writable() family to
	 * trigger the ssh protocol to ask if you have any tx waiting.
	 *
	 * Returning -1 from here will close the tcp connection to the client.
	 */
	int (*tx_waiting)(void *priv);

	/**
	 * tx() - provide data to send on the channel
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param stdch: LWS_STDOUT or LWS_STDERR
	 * \param buf:	start of the buffer to copy the transmit data into
	 * \param len: 	max length of the buffer in bytes
	 *
	 * copy and consume up to len bytes into *buf,
	 * return the actual copied count.
	 *
	 * You should use one of the lws_callback_on_writable() family to
	 * trigger the ssh protocol to ask if you have any tx waiting.  If you
	 * do you will get calls here to fetch it, for each of LWS_STDOUT or
	 * LWS_STDERR that were reported to be waiting by tx_waiting().
	 */
	size_t (*tx)(void *priv, int stdch, uint8_t *buf, size_t len);

	/**
	 * get_server_key() - retreive the secret keypair for this server
	 *
	 * \param wsi:  the wsi representing the connection to the client
	 * \param buf:	start of the buffer to copy the keypair into
	 * \param len: 	length of the buffer in bytes
	 *
	 * load the server key into buf, max len len.  Returns length of buf
	 * set to key, or 0 if no key or other error.  If there is no key,
	 * the error isn't fatal... the plugin will generate a random key and
	 * store it using *get_server_key() for subsequent times.
	 */
	size_t (*get_server_key)(struct lws *wsi, uint8_t *buf, size_t len);

	/**
	 * set_server_key() - store the secret keypair of this server
	 *
	 * \param wsi:  the wsi representing the connection to the client
	 * \param buf:	start of the buffer containing the keypair
	 * \param len: 	length of the keypair in bytes
	 *
	 * store the server key in buf, length len, to nonvolatile stg.
	 * Return length stored, 0 for fail.
	 */
	size_t (*set_server_key)(struct lws *wsi, uint8_t *buf, size_t len);

	/**
	 * set_env() - Set environment variable
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param name: env var name
	 * \param value: value to set env var to
	 *
	 * Client requested to set environment var.  Return nonzero to fail.
	 */
	int (*set_env)(void *priv, const char *name, const char *value);

	/**
	 * exec() - spawn command and wire up stdin/out/err to ssh channel
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param wsi: the struct lws the connection belongs to
	 * \param command:	string containing path to app and arguments
	 * \param finish: function to call to indicate the exec finished
	 * \param finish_handle: opaque handle identifying this exec for use with \p finish
	 *
	 * Client requested to exec something.  Return nonzero to fail.
	 */
	int (*exec)(void *priv, struct lws *wsi, const char *command, lws_ssh_finish_exec finish, void *finish_handle);

	/**
	 * shell() - Spawn shell that is appropriate for user
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param wsi: the struct lws the connection belongs to
	 * \param finish: function to call to indicate the exec finished
	 * \param finish_handle: opaque handle identifying this exec for use with \p finish
	 *
	 * Spawn the appropriate shell for this user.  Return 0 for OK
	 * or nonzero to fail.
	 */
	int (*shell)(void *priv, struct lws *wsi, lws_ssh_finish_exec finish, void *finish_handle);

	/**
	 * pty_req() - Create a Pseudo-TTY as described in pty
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param pty:	pointer to struct describing the desired pty
	 *
	 * Client requested a pty.  Return nonzero to fail.
	 */
	int (*pty_req)(void *priv, struct lws_ssh_pty *pty);

	/**
	 * child_process_io() - Child process has IO
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param wsi: the struct lws the connection belongs to
	 * \param args: information related to the cgi IO events
	 *
	 * Child process has IO
	 */
	int (*child_process_io)(void *priv, struct lws *wsi,
				struct lws_cgi_args *args);

	/**
	 * child_process_io() - Child process has terminated
	 *
	 * \param priv:	void * you set when this channel was created
	 * \param wsi: the struct lws the connection belongs to
	 *
	 * Child process has terminated
	 */
	int (*child_process_terminated)(void *priv, struct lws *wsi);

	/**
	 * disconnect_reason() - Optional notification why connection is lost
	 *
	 * \param reason: one of the SSH_DISCONNECT_ constants
	 * \param desc: UTF-8 description of reason
	 * \param desc_lang: RFC3066 language for description
	 *
	 * The remote peer may tell us why it's going to disconnect.  Handling
	 * this is optional.
	 */
	void (*disconnect_reason)(uint32_t reason, const char *desc,
				  const char *desc_lang);

	/**
	 * is_pubkey_authorized() - check if auth pubkey is valid for user
	 *
	 * \param username:	username the key attempted to authenticate
	 * \param type:		"ssh-rsa"
	 * \param peer:		start of Public key peer used to authenticate
	 * \param peer_len:	length of Public key at peer
	 *
	 * We confirmed the client has the private key for this public key...
	 * but is that keypair something authorized for this username on this
	 * server? 0 = OK, 1 = fail
	 *
	 * Normally this checks for a copy of the same public key stored
	 * somewhere out of band, it's the same procedure as openssh does
	 * when looking in ~/.ssh/authorized_keys
	 */
	int (*is_pubkey_authorized)(const char *username,
			const char *type, const uint8_t *peer, int peer_len);

	/**
	 * banner() - copy the connection banner to buffer
	 *
	 * \param buf:	start of the buffer to copy to
	 * \param max_len: maximum number of bytes the buffer can hold
	 * \param lang:	start of the buffer to copy language descriptor to
	 * \param max_lang_len: maximum number of bytes lang can hold
	 *
	 * Copy the text banner to be returned to client on connect,
	 * before auth, into buf.  The text should be in UTF-8.
	 * if none wanted then leave .banner as NULL.
	 *
	 * lang should have a RFC3066 language descriptor like "en/US"
	 * copied to it.
	 *
	 * Returns the number of bytes copies to buf.
	 */
	size_t (*banner)(char *buf, size_t max_len, char *lang,
			 size_t max_lang_len);

	/**
	 * SSH version string sent to client (required)
	 * By convention a string like "SSH-2.0-Libwebsockets"
	 */
	const char *server_string;

	/**
	 * set to the API version you support (current is in
	 * LWS_SSH_OPS_VERSION) You should set it to an integer like 1,
	 * that reflects the latest api at the time your code was written.  If
	 * the ops api_version is not equal to the LWS_SSH_OPS_VERSION of the
	 * plugin, it will error out at runtime.
	 */
	char api_version;
};
///@}

#endif

