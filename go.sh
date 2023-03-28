#!/usr/bin/env bash

die() { printf $'Error: %s\n' "$*" >&2; exit 1; }
root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
self=${BASH_SOURCE[0]:?}
project=${root##*/}


#---


# tag=fg_$USER:latest
# name=fg_$USER
# target=
# data='/mnt/seenas2/data/snap'
# cache=
# registry= #accona.eecs.utk.edu:5000
# xauth=
# entrypoint=
# ipc=
# net=
# user=1
# cwd=1
# interactive=1
# script=
# port=8889
# constraint=
# runtime=
# network=fg_$USER
# cap_add=SYS_PTRACE
# replicas=1
# tty=1
# declare -A urls

# test -f "${root:?}/env.sh" && source "${_:?}"


#--- Python

go-python() {
    "${FUNCNAME[0]:?}-$@"
}

go-python-exec() {
    exec "$@"
}

go---python() {
    go-python-exec "${self:?}" "$@"
}


#--- csv2bin

go-csv2bin.py() {
    exec python3 \
        "${root:?}/csv2bin.py" \
        "$@"
}


#--- Docker

docker_source=${root:?}
docker_tag=${project:?}:latest
docker_name=${project:?}
docker_build=(
)
docker_run=(
    --mount="type=bind,src=/mnt/seenas2/data/snap,dst=/mnt/seenas2/data/snap"
    --cap-add=SYS_PTRACE
)

go-docker() {
    "${FUNCNAME[0]:?}-$@"
}

go-docker-build() {
    exec docker buildx build \
        --rm=false \
        --tag "${docker_tag:?}" \
        "${docker_build[@]}" \
        "${docker_source:?}" \
        ##
}

go-docker-run() {
    exec docker run \
        --rm \
        --detach \
        --name "${docker_name:?}" \
        --mount "type=bind,src=/etc/passwd,dst=/etc/passwd,ro" \
        --mount "type=bind,src=/etc/group,dst=/etc/group,ro" \
        --mount "type=bind,src=${HOME:?},dst=${HOME:?},ro" \
        --mount "type=bind,src=${root:?},dst=${root:?}" \
        "${docker_run[@]}" \
        "${docker_tag:?}" \
        "$@" \
        ##
}

go-docker-start() {
    go-docker-run \
        sleep infinity
}

go-docker-stop() {
    exec docker stop \
        --time 0 \
        "${docker_name:?}"
}

go-docker-exec() {
    exec docker exec \
        --interactive \
        --detach-keys="ctrl-q,ctrl-q" \
        --tty \
        --user "$(id -u):$(id -g)" \
        --workdir "${PWD:?}" \
        --env USER \
        --env HOSTNAME \
        "${docker_name:?}" \
        "$@"
}

go---docker() {
    go-docker-exec "${self:?}" "$@"
}


#--- fg

fg_source=${root:?}


go-fg() {
    "${FUNCNAME[0]:?}-$@"
}

go-fg-build() {
    exec make \
        -C "${fg_source:?}" \
        "$@" \
        ##
}


#---

go-exec() {
    exec "$@"
}


#---

test -f "${root:?}/env.sh" && source "${_:?}"
go-"$@"
