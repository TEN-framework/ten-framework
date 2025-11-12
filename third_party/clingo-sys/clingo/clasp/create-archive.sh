#! /bin/bash
VERSION=$1
SOURCE=clasp-$VERSION.tar
git archive --prefix=clasp-$VERSION/ -o $SOURCE origin/dev
echo Running git archive submodules...
p=`pwd` && (echo .; git submodule foreach) | while read entering path; do \
	temp="${path%\'}"; \
	temp="${temp#\'}"; \
	path=$temp; \
	[ "$path" = "" ] && continue; \
	(cd $path && git archive --prefix=clasp-$VERSION/$path/ HEAD > $p/tmp.tar && tar --concatenate --file=$p/$SOURCE $p/tmp.tar && rm $p/tmp.tar); \
done
gzip $SOURCE

