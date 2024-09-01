#!/usr/bin/env bash
# vim :set ts=4 sw=4 sts=4 et:
die() { printf $'Error: %s\n' "$*" >&2; exit 1; }
root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)
self=${BASH_SOURCE[0]:?}
project=${root##*/}
pexec() { >&2 printf exec; >&2 printf ' %q' "$@"; >&2 printf '\n'; exec "$@"; }
prun() { >&2 printf run; >&2 printf ' %q' "$@"; >&2 printf '\n'; "$@"; }
go() { "go-$@"; }
next() { "${FUNCNAME[1]:?}-$@"; }
#---

cache=${root:?}/cache

go-Optimize-Cache() {
    prun mkdir -p "/mnt/seenas2/data${cache:?}" \
        || die "Failed to create alternate: ${_:?}"
    
    prun ln -sf "/mnt/seenas2/data${cache:?}" "${cache%/*}/" \
        || die "Failed to create alternate: ${_:?}"
}

JSDeps_data_tar=${cache:?}/JS-Deps.data.tar.gz
JSDeps_data_url=https://accona.eecs.utk.edu/JS-Deps.data.tar.gz
JSDeps_data_dir=${root:?}/examples/JS-Deps/data

go-Download-JSDeps() {
    pexec wget \
        --quiet \
        --timestamping \
        --directory-prefix="${cache:?}" \
        "${JSDeps_data_url:?}" \
    ##
}

go-Optimize-JSDeps() {
    prun mkdir -p "/mnt/seenas2/data${JSDeps_data_dir:?}" \
        || die "Failed to create alternate: ${_:?}"
    
    prun ln -sf "/mnt/seenas2/data${JSDeps_data_dir:?}" "${JSDeps_data_dir:?}" \
        || die "Failed to create alternate: ${_:?}"
}

go-Extract-JSDeps() {
    prun cd "${JSDeps_data_dir%/*}" \
        || die "Failed to change directory: ${_:?}"
    
    prun tar \
        --extract \
        --file="${JSDeps_data_tar:?}" \
    ##
}

SOAnswers_data_tar=${cache:?}/SO-Answers.data.tar.gz
SOAnswers_data_url=https://accona.eecs.utk.edu/SO-Answers.data.tar.gz
SOAnswers_data_dir=${root:?}/examples/SO-Answers/data

go-Download-SOAnswers() {
    pexec wget \
        --quiet \
        --timestamping \
        --directory-prefix="${cache:?}" \
        "${SOAnswers_data_url:?}" \
    ##
}

go-Optimize-SOAnswers() {
    prun mkdir -p "/mnt/seenas2/data${SOAnswers_data_dir:?}" \
        || die "Failed to create alternate: ${_:?}"
    
    prun ln -sf "/mnt/seenas2/data${SOAnswers_data_dir:?}" "${SOAnswers_data_dir%/*}/" \
        || die "Failed to create alternate: ${_:?}"
}

go-Extract-SOAnswers() {
    prun cd "${SOAnswers_data_dir%/*}" \
        || die "Failed to change directory: ${_:?}"
    
    prun tar \
        --extract \
        --file="${SOAnswers_data_tar:?}" \
    ##
}

NBERPatents_data_tar=${cache:?}/NBER-Patents.data.tar.gz
NBERPatents_data_url=https://accona.eecs.utk.edu/NBER-Patents.data.tar.gz
NBERPatents_data_dir=${root:?}/examples/NBER-Patents/data

go-Download-NBERPatents() {
    pexec wget \
        --quiet \
        --timestamping \
        --directory-prefix="${cache:?}" \
        "${NBERPatents_data_url:?}" \
    ##
}

go-Optimize-NBERPatents() {
    prun mkdir -p "/mnt/seenas2/data${NBERPatents_data_dir:?}" \
        || die "Failed to create alternate: ${_:?}"
    
    prun ln -sf "/mnt/seenas2/data${NBERPatents_data_dir:?}" "${NBERPatents_data_dir:?}" \
        || die "Failed to create alternate: ${_:?}"
}

go-Extract-NBERPatents() {
    prun cd "${NBERPatents_data_dir%/*}" \
        || die "Failed to change directory: ${_:?}"
    
    prun tar \
        --extract \
        --file="${NBERPatents_data_tar:?}" \
    ##
}


docker_image_source_dir=${root:?}
docker_image_tag=${project,,}:latest
docker_container_name=${project,,}

go-Build-Image() {
    pexec docker build \
        --progress=plain \
        --tag="${docker_image_tag:?}" \
        "${docker_image_source_dir:?}" \
    ##
}

go-Start-Container() {
    pexec docker run \
        --rm \
        --init \
        --detach \
        --ulimit=core=0 \
        --cap-add=SYS_PTRACE \
        --net=host \
        --runtime=nvidia \
        --name="${docker_container_name:?}" \
        --mount="type=bind,src=${root:?},dst=${root:?},readonly=false" \
        --mount="type=bind,src=${HOME:?},dst=${HOME:?},readonly=false" \
        --mount="type=bind,src=/etc/passwd,dst=/etc/passwd,readonly=true" \
        --mount="type=bind,src=/etc/group,dst=/etc/group,readonly=true" \
        --mount="type=bind,src=/mnt/seenas2/data,dst=/mnt/seenas2/data,readonly=false" \
        "${docker_image_tag:?}" \
        sleep infinity \
    ##
}

go-Stop-Container() {
    pexec docker stop \
        --time=0 \
        "${docker_container_name:?}" \
    ##
}

go-Invoke-Container() {
    local tty
    if [ -t 0 ]; then
        tty=
    fi

    pexec docker exec \
        ${tty+--tty} \
        --interactive \
        --detach-keys="ctrl-q,ctrl-q" \
        --user="$(id -u):$(id -g)" \
        --env=USER \
        --env=HOSTNAME \
        --workdir="${PWD:?}" \
        "${docker_container_name:?}" \
        "${@:?Invoke-Container: missing command}" \
    ##
}

environment=${root:?}/venv

go-New-Environment() {
    pexec "${self:?}" Invoke-Container "${self:?}" --New-Environment "$@"
}

go---New-Environment() {
    exec python3.9 -m virtualenv \
        "${environment:?}" \
    ##
}

go-Optimize-Environment() {
    cd "${root:?}" \
        || die "Failed to change directory: ${root:?}"

    test -d "${environment:?}" \
        || die "Environment not found: ${environment:?} (run: ${self:?} New-Environment?)"
    
    test -d "/mnt/seenas2/data${root:?}" \
        || die "Alternate not found: /mnt/seenas2/data${root:?} (run: mkdir -p /mnt/seenas2/data${root:?})"
    
    prun mv "${environment#${root:?}}" "/mnt/seenas2/data${environment:?}" \
        || die "Failed to move environment: ${environment:?} -> /mnt/seenas2/data${environment:?}"
    
    pexec ln \
        -sf \
        "/mnt/seenas2/data${environment:?}" \
    ##
}

go-Initialize-Environment() {
    pexec "${self:?}" Invoke-Container "${self:?}" --Initialize-Environment "$@"
}

go---Initialize-Environment() {
    install=(
        jupyter
        mediocreatbest
        numpy
    )

    exec "${environment:?}/bin/pip" install \
        "${install[@]?}" \
    ##
}

go-Invoke-Environment() {
    pexec "${self:?}" Invoke-Container "${self:?}" --Invoke-Environment "$@"
}

go---Invoke-Environment() {
    source "${environment:?}/bin/activate" \
        || die "Failed to activate environment: ${environment:?}"

    exec "${@:?Invoke-Environment: missing command}" \
    ##
}

source_dir=${root:?}
binary_dir=${root:?}/build
prefix_dir=${root:?}/stage

go-Configure() {
    pexec "${self:?}" Invoke-Environment "${self:?}" --Configure "$@"
}

go---Configure() {
    config=(
        -L
        -DCMAKE_BUILD_TYPE:STRING=Debug
    )

    exec cmake \
        -H"${source_dir:?}" \
        -B"${binary_dir:?}" \
        -DCMAKE_INSTALL_PREFIX:PATH="${prefix_dir:?}" \
        "${config[@]?}" \
    ##
}

go-Build() {
    pexec "${self:?}" Invoke-Environment "${self:?}" --Build "$@"
}

go---Build() {
    exec cmake \
        --build "${binary_dir:?}" \
        --parallel \
        --verbose \
    ##
}

go-Install() {
    pexec "${self:?}" Invoke-Environment "${self:?}" --Install "$@"
}

go---Install() {
    exec cmake \
        --install "${binary_dir:?}" \
        --verbose \
    ##
}

unset jupyter_bind
unset jupyter_port

go-Invoke-Jupyter() {
    pexec "${self:?}" Invoke-Environment "${self:?}" --Invoke-Jupyter "$@"
}

go---Invoke-Jupyter() {
    JUPYTER_TOKEN=168baa157591be6f6e8188b791b67784dc3e185d8b552a2d \
    exec jupyter notebook \
        --no-browser \
        --port-retries=0 \
        --ip="${jupyter_bind:?}" \
        --port="${jupyter_port:?}" \
    ##
}


#---
test -f "${root:?}/env.sh" && source "${_:?}"
"go-$@"
