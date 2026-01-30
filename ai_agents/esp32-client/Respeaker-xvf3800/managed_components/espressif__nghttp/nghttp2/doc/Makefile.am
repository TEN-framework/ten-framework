# nghttp2 - HTTP/2 C Library

# Copyright (c) 2012 Tatsuhiro Tsujikawa

# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:

# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

man_MANS = nghttp.1 nghttpd.1 nghttpx.1 h2load.1

APIDOCS= \
	macros.rst \
	enums.rst \
	types.rst \
	nghttp2_check_authority.rst \
	nghttp2_check_header_name.rst \
	nghttp2_check_header_value.rst \
	nghttp2_check_header_value_rfc9113.rst \
	nghttp2_check_method.rst \
	nghttp2_check_path.rst \
	nghttp2_extpri_parse_priority.rst \
	nghttp2_hd_deflate_bound.rst \
	nghttp2_hd_deflate_change_table_size.rst \
	nghttp2_hd_deflate_del.rst \
	nghttp2_hd_deflate_get_dynamic_table_size.rst \
	nghttp2_hd_deflate_get_max_dynamic_table_size.rst \
	nghttp2_hd_deflate_get_num_table_entries.rst \
	nghttp2_hd_deflate_get_table_entry.rst \
	nghttp2_hd_deflate_hd.rst \
	nghttp2_hd_deflate_hd2.rst \
	nghttp2_hd_deflate_hd_vec.rst \
	nghttp2_hd_deflate_hd_vec2.rst \
	nghttp2_hd_deflate_new.rst \
	nghttp2_hd_deflate_new2.rst \
	nghttp2_hd_inflate_change_table_size.rst \
	nghttp2_hd_inflate_del.rst \
	nghttp2_hd_inflate_end_headers.rst \
	nghttp2_hd_inflate_get_dynamic_table_size.rst \
	nghttp2_hd_inflate_get_max_dynamic_table_size.rst \
	nghttp2_hd_inflate_get_num_table_entries.rst \
	nghttp2_hd_inflate_get_table_entry.rst \
	nghttp2_hd_inflate_hd.rst \
	nghttp2_hd_inflate_hd2.rst \
	nghttp2_hd_inflate_hd3.rst \
	nghttp2_hd_inflate_new.rst \
	nghttp2_hd_inflate_new2.rst \
	nghttp2_http2_strerror.rst \
	nghttp2_is_fatal.rst \
	nghttp2_nv_compare_name.rst \
	nghttp2_option_del.rst \
	nghttp2_option_new.rst \
	nghttp2_option_set_builtin_recv_extension_type.rst \
	nghttp2_option_set_max_deflate_dynamic_table_size.rst \
	nghttp2_option_set_max_reserved_remote_streams.rst \
	nghttp2_option_set_max_send_header_block_length.rst \
	nghttp2_option_set_no_auto_ping_ack.rst \
	nghttp2_option_set_no_auto_window_update.rst \
	nghttp2_option_set_no_closed_streams.rst \
	nghttp2_option_set_no_http_messaging.rst \
	nghttp2_option_set_no_recv_client_magic.rst \
	nghttp2_option_set_no_rfc9113_leading_and_trailing_ws_validation.rst \
	nghttp2_option_set_peer_max_concurrent_streams.rst \
	nghttp2_option_set_server_fallback_rfc7540_priorities.rst \
	nghttp2_option_set_user_recv_extension_type.rst \
	nghttp2_option_set_max_continuations.rst \
	nghttp2_option_set_max_outbound_ack.rst \
	nghttp2_option_set_max_settings.rst \
	nghttp2_option_set_stream_reset_rate_limit.rst \
	nghttp2_pack_settings_payload.rst \
	nghttp2_pack_settings_payload2.rst \
	nghttp2_priority_spec_check_default.rst \
	nghttp2_priority_spec_default_init.rst \
	nghttp2_priority_spec_init.rst \
	nghttp2_rcbuf_decref.rst \
	nghttp2_rcbuf_get_buf.rst \
	nghttp2_rcbuf_incref.rst \
	nghttp2_rcbuf_is_static.rst \
	nghttp2_select_next_protocol.rst \
	nghttp2_select_alpn.rst \
	nghttp2_session_callbacks_del.rst \
	nghttp2_session_callbacks_new.rst \
	nghttp2_session_callbacks_set_before_frame_send_callback.rst \
	nghttp2_session_callbacks_set_data_source_read_length_callback.rst \
	nghttp2_session_callbacks_set_data_source_read_length_callback2.rst \
	nghttp2_session_callbacks_set_error_callback.rst \
	nghttp2_session_callbacks_set_error_callback2.rst \
	nghttp2_session_callbacks_set_on_begin_frame_callback.rst \
	nghttp2_session_callbacks_set_on_begin_headers_callback.rst \
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback.rst \
	nghttp2_session_callbacks_set_on_extension_chunk_recv_callback.rst \
	nghttp2_session_callbacks_set_on_frame_not_send_callback.rst \
	nghttp2_session_callbacks_set_on_frame_recv_callback.rst \
	nghttp2_session_callbacks_set_on_frame_send_callback.rst \
	nghttp2_session_callbacks_set_on_header_callback.rst \
	nghttp2_session_callbacks_set_on_header_callback2.rst \
	nghttp2_session_callbacks_set_on_invalid_frame_recv_callback.rst \
	nghttp2_session_callbacks_set_on_invalid_header_callback.rst \
	nghttp2_session_callbacks_set_on_invalid_header_callback2.rst \
	nghttp2_session_callbacks_set_on_stream_close_callback.rst \
	nghttp2_session_callbacks_set_pack_extension_callback.rst \
	nghttp2_session_callbacks_set_pack_extension_callback2.rst \
	nghttp2_session_callbacks_set_recv_callback.rst \
	nghttp2_session_callbacks_set_recv_callback2.rst \
	nghttp2_session_callbacks_set_select_padding_callback.rst \
	nghttp2_session_callbacks_set_select_padding_callback2.rst \
	nghttp2_session_callbacks_set_send_callback.rst \
	nghttp2_session_callbacks_set_send_callback2.rst \
	nghttp2_session_callbacks_set_send_data_callback.rst \
	nghttp2_session_callbacks_set_unpack_extension_callback.rst \
	nghttp2_session_change_extpri_stream_priority.rst \
	nghttp2_session_change_stream_priority.rst \
	nghttp2_session_check_request_allowed.rst \
	nghttp2_session_check_server_session.rst \
	nghttp2_session_client_new.rst \
	nghttp2_session_client_new2.rst \
	nghttp2_session_client_new3.rst \
	nghttp2_session_consume.rst \
	nghttp2_session_consume_connection.rst \
	nghttp2_session_consume_stream.rst \
	nghttp2_session_create_idle_stream.rst \
	nghttp2_session_del.rst \
	nghttp2_session_find_stream.rst \
	nghttp2_session_get_effective_local_window_size.rst \
	nghttp2_session_get_effective_recv_data_length.rst \
	nghttp2_session_get_extpri_stream_priority.rst \
	nghttp2_session_get_hd_deflate_dynamic_table_size.rst \
	nghttp2_session_get_hd_inflate_dynamic_table_size.rst \
	nghttp2_session_get_last_proc_stream_id.rst \
	nghttp2_session_get_local_settings.rst \
	nghttp2_session_get_local_window_size.rst \
	nghttp2_session_get_next_stream_id.rst \
	nghttp2_session_get_outbound_queue_size.rst \
	nghttp2_session_get_remote_settings.rst \
	nghttp2_session_get_remote_window_size.rst \
	nghttp2_session_get_root_stream.rst \
	nghttp2_session_get_stream_effective_local_window_size.rst \
	nghttp2_session_get_stream_effective_recv_data_length.rst \
	nghttp2_session_get_stream_local_close.rst \
	nghttp2_session_get_stream_local_window_size.rst \
	nghttp2_session_get_stream_remote_close.rst \
	nghttp2_session_get_stream_remote_window_size.rst \
	nghttp2_session_get_stream_user_data.rst \
	nghttp2_session_mem_recv.rst \
	nghttp2_session_mem_recv2.rst \
	nghttp2_session_mem_send.rst \
	nghttp2_session_mem_send2.rst \
	nghttp2_session_recv.rst \
	nghttp2_session_resume_data.rst \
	nghttp2_session_send.rst \
	nghttp2_session_server_new.rst \
	nghttp2_session_server_new2.rst \
	nghttp2_session_server_new3.rst \
	nghttp2_session_set_local_window_size.rst \
	nghttp2_session_set_next_stream_id.rst \
	nghttp2_session_set_stream_user_data.rst \
	nghttp2_session_set_user_data.rst \
	nghttp2_session_terminate_session.rst \
	nghttp2_session_terminate_session2.rst \
	nghttp2_session_upgrade.rst \
	nghttp2_session_upgrade2.rst \
	nghttp2_session_want_read.rst \
	nghttp2_session_want_write.rst \
	nghttp2_set_debug_vprintf_callback.rst \
	nghttp2_stream_get_first_child.rst \
	nghttp2_stream_get_next_sibling.rst \
	nghttp2_stream_get_parent.rst \
	nghttp2_stream_get_previous_sibling.rst \
	nghttp2_stream_get_state.rst \
	nghttp2_stream_get_sum_dependency_weight.rst \
	nghttp2_stream_get_weight.rst \
	nghttp2_strerror.rst \
	nghttp2_submit_altsvc.rst \
	nghttp2_submit_data.rst \
	nghttp2_submit_data2.rst \
	nghttp2_submit_extension.rst \
	nghttp2_submit_goaway.rst \
	nghttp2_submit_headers.rst \
	nghttp2_submit_origin.rst \
	nghttp2_submit_ping.rst \
	nghttp2_submit_priority.rst \
	nghttp2_submit_priority_update.rst \
	nghttp2_submit_push_promise.rst \
	nghttp2_submit_request.rst \
	nghttp2_submit_request2.rst \
	nghttp2_submit_response.rst \
	nghttp2_submit_response2.rst \
	nghttp2_submit_rst_stream.rst \
	nghttp2_submit_settings.rst \
	nghttp2_submit_shutdown_notice.rst \
	nghttp2_submit_trailer.rst \
	nghttp2_submit_window_update.rst \
	nghttp2_version.rst

RST_FILES = \
	README.rst \
	programmers-guide.rst \
	nghttp.1.rst \
	nghttpd.1.rst \
	nghttpx.1.rst \
	h2load.1.rst

EXTRA_DIST = \
	CMakeLists.txt \
	mkapiref.py \
	$(RST_FILES) \
	$(APIDOCS) \
	sources/index.rst \
	sources/tutorial-client.rst \
	sources/tutorial-server.rst \
	sources/tutorial-hpack.rst \
	sources/nghttpx-howto.rst \
	sources/h2load-howto.rst \
	sources/building-android-binary.rst \
	sources/contribute.rst \
	_exts/rubydomain/LICENSE.rubydomain \
	_exts/rubydomain/__init__.py \
	_exts/rubydomain/rubydomain.py \
	$(man_MANS) \
	bash_completion/nghttp \
	bash_completion/nghttpd \
	bash_completion/nghttpx \
	bash_completion/h2load

# Makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   ?= sphinx-build
PAPER         =
BUILDDIR      = manual

# Internal variables.
PAPEROPT_a4     = -D latex_paper_size=a4
PAPEROPT_letter = -D latex_paper_size=letter
ALLSPHINXOPTS   = -d $(BUILDDIR)/doctrees $(PAPEROPT_$(PAPER)) $(SPHINXOPTS) .

.PHONY: help clean html dirhtml singlehtml pickle json htmlhelp qthelp devhelp epub latex latexpdf text man changes linkcheck doctest

help:
	@echo "Please use \`make <target>' where <target> is one of"
	@echo "  html       to make standalone HTML files"
	@echo "  dirhtml    to make HTML files named index.html in directories"
	@echo "  singlehtml to make a single large HTML file"
	@echo "  pickle     to make pickle files"
	@echo "  json       to make JSON files"
	@echo "  htmlhelp   to make HTML files and a HTML help project"
	@echo "  qthelp     to make HTML files and a qthelp project"
	@echo "  devhelp    to make HTML files and a Devhelp project"
	@echo "  epub       to make an epub"
	@echo "  latex      to make LaTeX files, you can set PAPER=a4 or PAPER=letter"
	@echo "  latexpdf   to make LaTeX files and run them through pdflatex"
	@echo "  text       to make text files"
	@echo "  man        to make manual pages"
	@echo "  changes    to make an overview of all changed/added/deprecated items"
	@echo "  linkcheck  to check all external links for integrity"
	@echo "  doctest    to run all doctests embedded in the documentation (if enabled)"

apiref.rst: \
	$(top_builddir)/lib/includes/nghttp2/nghttp2ver.h \
	$(top_srcdir)/lib/includes/nghttp2/nghttp2.h
	for i in $(RST_FILES); do [ -e $(builddir)/$$i ] || cp $(srcdir)/$$i $(builddir); done
	$(PYTHON) $(top_srcdir)/doc/mkapiref.py \
	apiref.rst macros.rst enums.rst types.rst . $^

$(APIDOCS): apiref.rst

clean-local:
	if [ $(srcdir) != $(builddir) ]; then for i in $(RST_FILES); do rm -f $(builddir)/$$i; done fi
	-rm -f apiref.rst
	-rm -f $(APIDOCS)
	-rm -rf $(BUILDDIR)/*

html-local: apiref.rst
	$(SPHINXBUILD) -b html $(ALLSPHINXOPTS) $(BUILDDIR)/html
	@echo
	@echo "Build finished. The HTML pages are in $(BUILDDIR)/html."

dirhtml:
	$(SPHINXBUILD) -b dirhtml $(ALLSPHINXOPTS) $(BUILDDIR)/dirhtml
	@echo
	@echo "Build finished. The HTML pages are in $(BUILDDIR)/dirhtml."

singlehtml:
	$(SPHINXBUILD) -b singlehtml $(ALLSPHINXOPTS) $(BUILDDIR)/singlehtml
	@echo
	@echo "Build finished. The HTML page is in $(BUILDDIR)/singlehtml."

pickle:
	$(SPHINXBUILD) -b pickle $(ALLSPHINXOPTS) $(BUILDDIR)/pickle
	@echo
	@echo "Build finished; now you can process the pickle files."

json:
	$(SPHINXBUILD) -b json $(ALLSPHINXOPTS) $(BUILDDIR)/json
	@echo
	@echo "Build finished; now you can process the JSON files."

htmlhelp:
	$(SPHINXBUILD) -b htmlhelp $(ALLSPHINXOPTS) $(BUILDDIR)/htmlhelp
	@echo
	@echo "Build finished; now you can run HTML Help Workshop with the" \
	      ".hhp project file in $(BUILDDIR)/htmlhelp."

qthelp:
	$(SPHINXBUILD) -b qthelp $(ALLSPHINXOPTS) $(BUILDDIR)/qthelp
	@echo
	@echo "Build finished; now you can run "qcollectiongenerator" with the" \
	      ".qhcp project file in $(BUILDDIR)/qthelp, like this:"
	@echo "# qcollectiongenerator $(BUILDDIR)/qthelp/nghttp2.qhcp"
	@echo "To view the help file:"
	@echo "# assistant -collectionFile $(BUILDDIR)/qthelp/nghttp2.qhc"

devhelp:
	$(SPHINXBUILD) -b devhelp $(ALLSPHINXOPTS) $(BUILDDIR)/devhelp
	@echo
	@echo "Build finished."
	@echo "To view the help file:"
	@echo "# mkdir -p $$HOME/.local/share/devhelp/nghttp2"
	@echo "# ln -s $(BUILDDIR)/devhelp $$HOME/.local/share/devhelp/nghttp2"
	@echo "# devhelp"

epub:
	$(SPHINXBUILD) -b epub $(ALLSPHINXOPTS) $(BUILDDIR)/epub
	@echo
	@echo "Build finished. The epub file is in $(BUILDDIR)/epub."

latex:
	$(SPHINXBUILD) -b latex $(ALLSPHINXOPTS) $(BUILDDIR)/latex
	@echo
	@echo "Build finished; the LaTeX files are in $(BUILDDIR)/latex."
	@echo "Run \`make' in that directory to run these through (pdf)latex" \
	      "(use \`make latexpdf' here to do that automatically)."

latexpdf:
	$(SPHINXBUILD) -b latex $(ALLSPHINXOPTS) $(BUILDDIR)/latex
	@echo "Running LaTeX files through pdflatex..."
	$(MAKE) -C $(BUILDDIR)/latex all-pdf
	@echo "pdflatex finished; the PDF files are in $(BUILDDIR)/latex."

text:
	$(SPHINXBUILD) -b text $(ALLSPHINXOPTS) $(BUILDDIR)/text
	@echo
	@echo "Build finished. The text files are in $(BUILDDIR)/text."

man: apiref.rst
	$(SPHINXBUILD) -b man $(ALLSPHINXOPTS) $(BUILDDIR)/man
	@echo
	@echo "Build finished. The manual pages are in $(BUILDDIR)/man."

changes:
	$(SPHINXBUILD) -b changes $(ALLSPHINXOPTS) $(BUILDDIR)/changes
	@echo
	@echo "The overview file is in $(BUILDDIR)/changes."

linkcheck:
	$(SPHINXBUILD) -b linkcheck $(ALLSPHINXOPTS) $(BUILDDIR)/linkcheck
	@echo
	@echo "Link check complete; look for any errors in the above output " \
	      "or in $(BUILDDIR)/linkcheck/output.txt."

doctest:
	$(SPHINXBUILD) -b doctest $(ALLSPHINXOPTS) $(BUILDDIR)/doctest
	@echo "Testing of doctests in the sources finished, look at the " \
	      "results in $(BUILDDIR)/doctest/output.txt."
