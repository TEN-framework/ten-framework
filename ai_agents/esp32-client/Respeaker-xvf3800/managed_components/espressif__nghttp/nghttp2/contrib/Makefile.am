# nghttp2 - HTTP/2 C Library

# Copyright (c) 2014 Tatsuhiro Tsujikawa

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

configfiles = nghttpx-init nghttpx.service nghttpx-upstart.conf

EXTRA_DIST = \
	CMakeLists.txt \
	$(configfiles:%=%.in) \
	nghttpx-logrotate \
	tlsticketupdate.go

edit = sed -e 's|@bindir[@]|$(bindir)|g'

nghttpx-init: $(srcdir)/nghttpx-init.in
	rm -f $@ $@.tmp
	$(edit) $< > $@.tmp
	chmod +x $@.tmp
	mv $@.tmp $@

nghttpx.service: $(srcdir)/nghttpx.service.in
	$(edit) $< > $@

nghttpx-upstart.conf: $(srcdir)/nghttpx-upstart.conf.in
	$(edit) $< > $@

$(configfiles): Makefile

all-local: $(configfiles)

clean-local:
	-rm -f nghttpx-init.tmp $(configfiles)
