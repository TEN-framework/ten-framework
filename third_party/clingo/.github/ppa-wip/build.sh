#!/bin/bash

function usage {
    echo "./$(basename $0) {stable,wip} {jammy,focal,bionic} {create,sync,changes,build,put,clean}*"
}

if [[ $# < 1 ]]; then
    usage
    exit 1
fi

ref="${1}"
shift

case "${ref}" in
    stable|wip)
        ;;
    *)
        usage
        exit 1
esac

rep="${1}"
shift

case "${rep}" in
    jammy|focal|bionic)
        ;;
    *)
        usage
        exit 1
esac

for act in "${@}"; do
    echo "${act}"
    case "${act}" in
        _ppa)
            apt-get install -y software-properties-common
            add-apt-repository -y ppa:potassco/${ref}
            apt-get update
            apt-get install -y tree debhelper
            ;;
        create)
            sudo pbuilder create --basetgz /var/cache/pbuilder/${ref}-${rep}.tgz --distribution ${rep} --debootstrapopts --variant=buildd
            sudo pbuilder execute --basetgz /var/cache/pbuilder/${ref}-${rep}.tgz --save-after-exec -- build.sh ${ref} ${rep} _ppa
            ;;
        sync)
            rsync -aq \
                --exclude __pycache__ \
                --exclude .mypy_cache \
                --exclude '*,cover' \
                --exclude '*.egg-info' \
                --exclude dist \
                --exclude build \
                --exclude third_party/catch \
                ../../app \
                ../../cmake \
                ../../clasp \
                ../../lib* \
                ../../third_party \
                ../../CMakeLists.txt \
                ../../README.md \
                ../../INSTALL.md \
                ../../LICENSE.md \
                ../../CHANGES.md \
                $rep/
            sed -i "s/export CLINGO_BUILD_REVISION =.*/export CLINGO_BUILD_REVISION = $(git rev-parse --short HEAD)/" ${rep}/debian/rules
            ;;
        changes)
            VERSION="$(sed -n '/#define CLINGO_VERSION "/s/.*"\([0-9]\+\.[0-9\+]\.[0-9]\+\)".*/\1/p' ../../libclingo/clingo.h)"
            BUILD=$(curl -sL http://ppa.launchpad.net/potassco/${ref}/ubuntu/pool/main/c/clingo/ | sed -n "/${VERSION//./\\.}-${rep}[0-9]\+\.dsc/s/.*${rep}\([0-9]\+\).*/\1/p" | sort -rn | head -1)
            cat > ${rep}/debian/changelog <<EOF
clingo (${VERSION}-${rep}$[BUILD+1]) ${rep}; urgency=medium

  * build for git revision $(git rev-parse HEAD)

 -- Roland Kaminski <kaminski@cs.uni-potsdam.de>  $(date -R)
EOF
            ;;
        build)
            VERSION="$(head -n 1 ${rep}/debian/changelog | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+\(-[a-z0-9]\+\)\?')"
            (
                cd "${rep}"
                pdebuild --buildresult .. --auto-debsign --debsign-k 744d959e10f5ad73f9cf17cc1d150536980033d5 -- --basetgz /var/cache/pbuilder/${ref}-${rep}.tgz --source-only-changes
                sed -i '/\.buildinfo$/d' ../clingo_${VERSION}_source.changes
                debsign --re-sign -k744d959e10f5ad73f9cf17cc1d150536980033d5 ../clingo_${VERSION}_source.changes
            )
            ;;
        put)
            VERSION="$(head -n 1 ${rep}/debian/changelog | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+\(-[a-z0-9]\+\)\?')"
            dput ppa:potassco/${ref} clingo_${VERSION}_source.changes
            ;;
        clean)
            rm -rf \
                "${rep}"/app \
                "${rep}"/cmake \
                "${rep}"/clasp \
                "${rep}"/lib* \
                "${rep}"/third_party \
                "${rep}"/CMakeLists.txt \
                "${rep}"/README.md \
                "${rep}"/INSTALL.md \
                "${rep}"/LICENSE.md \
                "${rep}"/CHANGES.md \
                "${rep}"/debian/files \
                "${rep}"/debian/.debhelper \
                "${rep}"/debian/clingo.debhelper.log \
                "${rep}"/debian/clingo.substvars \
                "${rep}"/debian/clingo \
                "${rep}"/debian/debhelper-build-stamp \
                "${rep}"/debian/tmp \
                "${rep}"/obj-x86_64-linux-gnu \
                *.build \
                *.deb \
                *.dsc \
                *.buildinfo \
                *.changes \
                *.ddeb \
                *.tar.xz \
                *.upload
            git checkout "${rep}/debian/changelog" "${rep}/debian/rules"
            ;;
        *)
            usage
            exit 1
    esac
done
