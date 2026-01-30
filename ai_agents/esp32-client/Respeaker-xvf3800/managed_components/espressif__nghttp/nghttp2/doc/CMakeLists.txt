# Generated documents
set(APIDOCS
  macros.rst
  enums.rst
  types.rst
  nghttp2_check_header_name.rst
  nghttp2_check_header_value.rst
  nghttp2_hd_deflate_bound.rst
  nghttp2_hd_deflate_change_table_size.rst
  nghttp2_hd_deflate_del.rst
  nghttp2_hd_deflate_get_dynamic_table_size.rst
  nghttp2_hd_deflate_get_max_dynamic_table_size.rst
  nghttp2_hd_deflate_get_num_table_entries.rst
  nghttp2_hd_deflate_get_table_entry.rst
  nghttp2_hd_deflate_hd.rst
  nghttp2_hd_deflate_hd_vec.rst
  nghttp2_hd_deflate_new.rst
  nghttp2_hd_deflate_new2.rst
  nghttp2_hd_inflate_change_table_size.rst
  nghttp2_hd_inflate_del.rst
  nghttp2_hd_inflate_end_headers.rst
  nghttp2_hd_inflate_get_dynamic_table_size.rst
  nghttp2_hd_inflate_get_max_dynamic_table_size.rst
  nghttp2_hd_inflate_get_num_table_entries.rst
  nghttp2_hd_inflate_get_table_entry.rst
  nghttp2_hd_inflate_hd.rst
  nghttp2_hd_inflate_hd2.rst
  nghttp2_hd_inflate_new.rst
  nghttp2_hd_inflate_new2.rst
  nghttp2_http2_strerror.rst
  nghttp2_is_fatal.rst
  nghttp2_nv_compare_name.rst
  nghttp2_option_del.rst
  nghttp2_option_new.rst
  nghttp2_option_set_builtin_recv_extension_type.rst
  nghttp2_option_set_max_deflate_dynamic_table_size.rst
  nghttp2_option_set_max_reserved_remote_streams.rst
  nghttp2_option_set_max_send_header_block_length.rst
  nghttp2_option_set_no_auto_ping_ack.rst
  nghttp2_option_set_no_auto_window_update.rst
  nghttp2_option_set_no_http_messaging.rst
  nghttp2_option_set_no_recv_client_magic.rst
  nghttp2_option_set_peer_max_concurrent_streams.rst
  nghttp2_option_set_user_recv_extension_type.rst
  nghttp2_option_set_max_settings.rst
  nghttp2_pack_settings_payload.rst
  nghttp2_priority_spec_check_default.rst
  nghttp2_priority_spec_default_init.rst
  nghttp2_priority_spec_init.rst
  nghttp2_rcbuf_decref.rst
  nghttp2_rcbuf_get_buf.rst
  nghttp2_rcbuf_incref.rst
  nghttp2_rcbuf_is_static.rst
  nghttp2_select_next_protocol.rst
  nghttp2_session_callbacks_del.rst
  nghttp2_session_callbacks_new.rst
  nghttp2_session_callbacks_set_before_frame_send_callback.rst
  nghttp2_session_callbacks_set_data_source_read_length_callback.rst
  nghttp2_session_callbacks_set_error_callback.rst
  nghttp2_session_callbacks_set_on_begin_frame_callback.rst
  nghttp2_session_callbacks_set_on_begin_headers_callback.rst
  nghttp2_session_callbacks_set_on_data_chunk_recv_callback.rst
  nghttp2_session_callbacks_set_on_extension_chunk_recv_callback.rst
  nghttp2_session_callbacks_set_on_frame_not_send_callback.rst
  nghttp2_session_callbacks_set_on_frame_recv_callback.rst
  nghttp2_session_callbacks_set_on_frame_send_callback.rst
  nghttp2_session_callbacks_set_on_header_callback.rst
  nghttp2_session_callbacks_set_on_header_callback2.rst
  nghttp2_session_callbacks_set_on_invalid_frame_recv_callback.rst
  nghttp2_session_callbacks_set_on_invalid_header_callback.rst
  nghttp2_session_callbacks_set_on_invalid_header_callback2.rst
  nghttp2_session_callbacks_set_on_stream_close_callback.rst
  nghttp2_session_callbacks_set_pack_extension_callback.rst
  nghttp2_session_callbacks_set_recv_callback.rst
  nghttp2_session_callbacks_set_select_padding_callback.rst
  nghttp2_session_callbacks_set_send_callback.rst
  nghttp2_session_callbacks_set_send_data_callback.rst
  nghttp2_session_callbacks_set_unpack_extension_callback.rst
  nghttp2_session_change_stream_priority.rst
  nghttp2_session_check_request_allowed.rst
  nghttp2_session_check_server_session.rst
  nghttp2_session_client_new.rst
  nghttp2_session_client_new2.rst
  nghttp2_session_client_new3.rst
  nghttp2_session_consume.rst
  nghttp2_session_consume_connection.rst
  nghttp2_session_consume_stream.rst
  nghttp2_session_create_idle_stream.rst
  nghttp2_session_del.rst
  nghttp2_session_find_stream.rst
  nghttp2_session_get_effective_local_window_size.rst
  nghttp2_session_get_effective_recv_data_length.rst
  nghttp2_session_get_hd_deflate_dynamic_table_size.rst
  nghttp2_session_get_hd_inflate_dynamic_table_size.rst
  nghttp2_session_get_last_proc_stream_id.rst
  nghttp2_session_get_local_settings.rst
  nghttp2_session_get_local_window_size.rst
  nghttp2_session_get_next_stream_id.rst
  nghttp2_session_get_outbound_queue_size.rst
  nghttp2_session_get_remote_settings.rst
  nghttp2_session_get_remote_window_size.rst
  nghttp2_session_get_root_stream.rst
  nghttp2_session_get_stream_effective_local_window_size.rst
  nghttp2_session_get_stream_effective_recv_data_length.rst
  nghttp2_session_get_stream_local_close.rst
  nghttp2_session_get_stream_local_window_size.rst
  nghttp2_session_get_stream_remote_close.rst
  nghttp2_session_get_stream_remote_window_size.rst
  nghttp2_session_get_stream_user_data.rst
  nghttp2_session_mem_recv.rst
  nghttp2_session_mem_send.rst
  nghttp2_session_recv.rst
  nghttp2_session_resume_data.rst
  nghttp2_session_send.rst
  nghttp2_session_server_new.rst
  nghttp2_session_server_new2.rst
  nghttp2_session_server_new3.rst
  nghttp2_session_set_local_window_size.rst
  nghttp2_session_set_next_stream_id.rst
  nghttp2_session_set_stream_user_data.rst
  nghttp2_session_terminate_session.rst
  nghttp2_session_terminate_session2.rst
  nghttp2_session_upgrade.rst
  nghttp2_session_upgrade2.rst
  nghttp2_session_want_read.rst
  nghttp2_session_want_write.rst
  nghttp2_set_debug_vprintf_callback.rst
  nghttp2_stream_get_first_child.rst
  nghttp2_stream_get_next_sibling.rst
  nghttp2_stream_get_parent.rst
  nghttp2_stream_get_previous_sibling.rst
  nghttp2_stream_get_state.rst
  nghttp2_stream_get_sum_dependency_weight.rst
  nghttp2_stream_get_weight.rst
  nghttp2_strerror.rst
  nghttp2_submit_altsvc.rst
  nghttp2_submit_data.rst
  nghttp2_submit_extension.rst
  nghttp2_submit_goaway.rst
  nghttp2_submit_headers.rst
  nghttp2_submit_ping.rst
  nghttp2_submit_priority.rst
  nghttp2_submit_push_promise.rst
  nghttp2_submit_request.rst
  nghttp2_submit_response.rst
  nghttp2_submit_rst_stream.rst
  nghttp2_submit_settings.rst
  nghttp2_submit_shutdown_notice.rst
  nghttp2_submit_trailer.rst
  nghttp2_submit_window_update.rst
  nghttp2_version.rst
)

set(MAN_PAGES
  nghttp.1
  nghttpd.1
  nghttpx.1
  h2load.1
)

# Other .rst files from the source tree that need to be copied
# XXX move them to sources/ and create .in files?
set(RST_FILES
  README.rst
  programmers-guide.rst
  nghttp.1.rst
  nghttpd.1.rst
  nghttpx.1.rst
  h2load.1.rst
)

# XXX unused for now
set(EXTRA_DIST
  mkapiref.py
  ${RST_FILES}
  ${APIDOCS}
  sources/index.rst
  sources/tutorial-client.rst
  sources/tutorial-server.rst
  sources/tutorial-hpack.rst
  sources/nghttpx-howto.rst
  sources/h2load-howto.rst
  sources/building-android-binary.rst
  sources/contribute.rst
  _exts/rubydomain/LICENSE.rubydomain
  _exts/rubydomain/__init__.py
  _exts/rubydomain/rubydomain.py
  _themes/sphinx_rtd_theme/__init__.py
  _themes/sphinx_rtd_theme/breadcrumbs.html
  _themes/sphinx_rtd_theme/footer.html
  _themes/sphinx_rtd_theme/layout.html
  _themes/sphinx_rtd_theme/layout_old.html
  _themes/sphinx_rtd_theme/search.html
  _themes/sphinx_rtd_theme/searchbox.html
  _themes/sphinx_rtd_theme/static/css/badge_only.css
  _themes/sphinx_rtd_theme/static/css/theme.css
  _themes/sphinx_rtd_theme/static/fonts/FontAwesome.otf
  _themes/sphinx_rtd_theme/static/fonts/fontawesome-webfont.eot
  _themes/sphinx_rtd_theme/static/fonts/fontawesome-webfont.svg
  _themes/sphinx_rtd_theme/static/fonts/fontawesome-webfont.ttf
  _themes/sphinx_rtd_theme/static/fonts/fontawesome-webfont.woff
  _themes/sphinx_rtd_theme/static/js/theme.js
  _themes/sphinx_rtd_theme/theme.conf
  _themes/sphinx_rtd_theme/versions.html
  ${MAN_PAGES}
  bash_completion/nghttp
  bash_completion/nghttpd
  bash_completion/nghttpx
  bash_completion/h2load
)

# Based on Makefile for Sphinx documentation

# You can set these variables from the command line.
set(SPHINXOPTS)
set(SPHINXBUILD sphinx-build)
set(PAPER)
set(BUILDDIR    manual)

# Internal variables.
set(PAPEROPT_a4     -D latex_paper_size=a4)
set(PAPEROPT_letter -D latex_paper_size=letter)
set(ALLSPHINXOPTS   -d ${BUILDDIR}/doctrees ${PAPEROPT_${PAPER}} ${SPHINXOPTS} .)

# "Please use `make <target>' where <target> is one of"
# "  html       to make standalone HTML files"
# "  dirhtml    to make HTML files named index.html in directories"
# "  singlehtml to make a single large HTML file"
# "  pickle     to make pickle files"
# "  json       to make JSON files"
# "  htmlhelp   to make HTML files and a HTML help project"
# "  qthelp     to make HTML files and a qthelp project"
# "  devhelp    to make HTML files and a Devhelp project"
# "  epub       to make an epub"
# "  latex      to make LaTeX files, you can set PAPER=a4 or PAPER=letter"
# "  latexpdf   to make LaTeX files and run them through pdflatex"
# "  text       to make text files"
# "  man        to make manual pages"
# "  changes    to make an overview of all changed/added/deprecated items"
# "  linkcheck  to check all external links for integrity"
# "  doctest    to run all doctests embedded in the documentation (if enabled)"


# Copy files for out-of-tree builds
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  set(RST_BUILD_FILES)
  foreach(rstfile IN LISTS RST_FILES)
    set(outfile "${CMAKE_CURRENT_BINARY_DIR}/${rstfile}")
    add_custom_command(OUTPUT "${outfile}"
      COMMAND ${CMAKE_COMMAND} -E copy
              "${CMAKE_CURRENT_SOURCE_DIR}/${rstfile}" "${outfile}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${rstfile}"
    )
    list(APPEND RST_BUILD_FILES "${outfile}")
  endforeach()
else()
  set(RST_BUILD_FILES "${RST_FILES}")
endif()

set(apiref_SOURCES
  ${CMAKE_BINARY_DIR}/lib/includes/nghttp2/nghttp2ver.h
  ${CMAKE_SOURCE_DIR}/lib/includes/nghttp2/nghttp2.h
)
# Generates apiref.rst and other files
add_custom_command(
  OUTPUT
    apiref.rst
    ${APIDOCS}
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/mkapiref.py"
    apiref.rst macros.rst enums.rst types.rst .
    ${apiref_SOURCES}
  DEPENDS
    ${RST_BUILD_FILES}
    ${apiref_SOURCES}
)



set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${BUILDDIR}")

# Invokes sphinx-build and prints the given messages when completed
function(sphinxbuild builder)
  set(echo_commands)
  foreach(message IN LISTS ARGN)
    list(APPEND echo_commands COMMAND ${CMAKE_COMMAND} -E echo "${message}")
  endforeach()
  add_custom_target(${builder}
    COMMAND "${SPHINXBUILD}" -b ${builder} ${ALLSPHINXOPTS} "${BUILDDIR}/${builder}"
    COMMAND ${CMAKE_COMMAND} -E echo
    ${echo_commands}
    VERBATIM
    DEPENDS apiref.rst
  )
endfunction()

foreach(builder html dirhtml singlehtml)
  sphinxbuild(${builder}
    "Build finished. The HTML pages are in ${BUILDDIR}/${builder}.")
endforeach()
sphinxbuild(pickle  "Build finished; now you can process the pickle files.")
sphinxbuild(json    "Build finished; now you can process the JSON files.")
sphinxbuild(htmlhelp
  "Build finished; now you can run HTML Help Workshop with the"
  ".hhp project file in ${BUILDDIR}/htmlhelp."
)
sphinxbuild(qthelp
  "Build finished; now you can run \"qcollectiongenerator\" with the"
  ".qhcp project file in ${BUILDDIR}/qthelp, like this:"
  "# qcollectiongenerator ${BUILDDIR}/qthelp/nghttp2.qhcp"
  "To view the help file:"
  "# assistant -collectionFile ${BUILDDIR}/qthelp/nghttp2.qhc"
)
sphinxbuild(devhelp
  "Build finished."
  "To view the help file:"
  "# mkdir -p ~/.local/share/devhelp/nghttp2"
  "# ln -s ${BUILDDIR}/devhelp ~/.local/share/devhelp/nghttp2"
  "# devhelp"
)
sphinxbuild(epub    "Build finished. The epub file is in ${BUILDDIR}/epub.")
sphinxbuild(latex
  "Build finished; the LaTeX files are in ${BUILDDIR}/latex."
  "Run `make' in that directory to run these through (pdf)latex"
  "(use `make latexpdf' here to do that automatically)."
)

# Invoke the Makefile generated by sphinx
add_custom_target(latexpdf
  COMMAND ${CMAKE_COMMAND} -E echo "Running LaTeX files through pdflatex..."
  COMMAND make -C "${BUILDDIR}/latex" all-pdf
  COMMAND ${CMAKE_COMMAND} -E echo "pdflatex finished; the PDF files are in ${BUILDDIR}/latex."
  DEPENDS latex
)

sphinxbuild(text    "Build finished. The text files are in ${BUILDDIR}/text.")
sphinxbuild(man     "Build finished. The manual pages are in ${BUILDDIR}/man.")
sphinxbuild(changes "The overview file is in ${BUILDDIR}/changes.")
sphinxbuild(linkcheck
  "Link check complete; look for any errors in the above output"
  "or in ${BUILDDIR}/linkcheck/output.txt."
)
sphinxbuild(doctest
  "Testing of doctests in the sources finished, look at the"
  "results in ${BUILDDIR}/doctest/output.txt."
)

foreach(_man_page IN LISTS MAN_PAGES)
  install(FILES ${_man_page}
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
  )
endforeach()
