#
# libwebsockets - small server side websockets and web server implementation
#
# Copyright (C) 2010 - 2020 Andy Green <andy@warmcat.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# The strategy is to only export to PARENT_SCOPE
#
#  - changes to LIB_LIST
#  - changes to SOURCES
#  - includes via include_directories
#
# and keep everything else private

include_directories(./async-dns)

list(APPEND SOURCES
	system/system.c)
	
if (LWS_WITH_NETWORK)

	if (LWS_WITH_SYS_ASYNC_DNS)
		list(APPEND SOURCES
			system/async-dns/async-dns.c
			system/async-dns/async-dns-parse.c)
	endif()

	if (LWS_WITH_SYS_NTPCLIENT)
		list(APPEND SOURCES
			system/ntpclient/ntpclient.c)
	endif()

	if (LWS_WITH_SYS_DHCP_CLIENT)
		list(APPEND SOURCES
			system/dhcpclient/dhcpclient.c
			system/dhcpclient/dhcpc4.c)
	endif()

	if (LWS_WITH_SYS_SMD)
		add_subdir_include_dirs(smd)
	endif()
	
	if (LWS_WITH_SYS_FAULT_INJECTION)
		include_directories(./fault-injection)
		list(APPEND SOURCES
			system/fault-injection/fault-injection.c)
	endif()

	add_subdir_include_dirs(metrics)

endif()

#
# Keep explicit parent scope exports at end
#

exports_to_parent_scope()
