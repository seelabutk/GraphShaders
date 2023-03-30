#!/bin/bash

root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

set -- env
for f in ${!FG_*}; do
    set -- "$@" "${f}=${!f}"
done
set -- "$@" fg

exec "${root:?}/go.sh" docker \
exec "${root:?}/go.sh" fg \
exec "$@"
