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

go-csv2bin() {
    "${FUNCNAME[0]:?}-$@"
}

go-csv2bin-JS-Deps() {
    if [ $# -eq 0 ]; then
        set -- x y date nmaintainers cve index
    fi

    for arg; do
        "${FUNCNAME[0]:?}-${arg:?}"
    done
}

go-csv2bin-JS-Deps-x() {
    local domain
    domain=-1666.310422874415,4145.933813716894

    python3 csv2bin.py \
        --input=data/JS-Deps-filtered/nodes.csv \
        --column=x \
        --transform=float \
        --format=f \
        --output=./JS-Deps,kind=node,name=x,type=f32.bin \
        ${domain:+--domain="${domain:?}"} \
        ${domain:+--transform=normalize} \
        ##
}

go-csv2bin-JS-Deps-y() {
    local domain
    domain=-2541.5300207136556,2247.2635371167767

    python3 csv2bin.py \
        --input=data/JS-Deps-filtered/nodes.csv \
        --column=y \
        --transform=float \
        --format=f \
        --output=./JS-Deps,kind=node,name=y,type=f32.bin \
        ${domain:+--domain="${domain:?}"} \
        ${domain:+--transform=normalize} \
        ##
}

go-csv2bin-JS-Deps-date() {
    local domain
    # domain='-2541.5300207136556,607.5603606114231'

    python3 csv2bin.py \
        --input=data/JS-Deps-filtered/nodes.csv \
        --column=date \
        --transform=int \
        --format=I \
        --output=./JS-Deps,kind=node,name=date,type=u32.bin \
        ${domain:+--domain="${domain:?}"} \
        ${domain:+--transform=normalize} \
        ##
}

go-csv2bin-JS-Deps-nmaintainers() {
    local domain
    # domain='-2541.5300207136556,607.5603606114231'

    python3 csv2bin.py \
        --input=data/JS-Deps-filtered/nodes.csv \
        --column=nmaintainers \
        --transform=int \
        --format=I \
        --output=./JS-Deps,kind=node,name=nmaintainers,type=u32.bin \
        ${domain:+--domain="${domain:?}"} \
        ${domain:+--transform=normalize} \
        ##
}

go-csv2bin-JS-Deps-cve() {
    local domain
    # domain='-2541.5300207136556,607.5603606114231'

    python3 csv2bin.py \
        --input=data/JS-Deps-filtered/nodes.csv \
        --column=cve \
        --transform=int \
        --format=I \
        --output=./JS-Deps,kind=node,name=cve,type=u32.bin \
        ${domain:+--domain="${domain:?}"} \
        ${domain:+--transform=normalize} \
        ##
}

go-csv2bin-JS-Deps-index() {
    python3 csv2bin.py \
        --input=sorted.csv \
        --column=src \
        --column=tgt \
        --transform=int \
        --transform=-1 \
        --format=II \
        --output=./JS-Deps,kind=edge,name=index,type=2u32.bin \
        ##
}

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
fg_build=${root:?}/build


go-fg() {
    "${FUNCNAME[0]:?}-$@"
}

go-fg-build() {
    exec make \
        -C "${fg_source:?}" \
        build/fg \
        "$@" \
        ##
}

go-fg-exec() {
    PATH=${fg_build:?}${PATH:+:${PATH:?}} \
    exec "$@"
}


#---

go-graph() {
    exec make \
        -f "${root:?}/graph/Makefile" \
        "$@" \
        ##
}

go-Sort() {
    "${FUNCNAME[0]:?}-$@"
}

go-Sort-JS-Deps() {
    >sorted.csv \
    awk \
        '
            BEGIN{ FS=OFS="," }

            # Like NR (num records), but NF (num files)
            BEGIN { NF = 0 }
            FNR == 1 { ++NF }

            # N (node count)
            BEGIN{ N=1 }
            NF==1 && FNR>1 {
                X[N] = 0+$1;
                Y[N] = 0+$2;
                DATE[N] = 0+$3;
                DEVS[N] = 0+$4;
                VULN[N] = 0+$5;
                ++N;
            }

            # E (edge count)
            BEGIN{ E=1 }
            NF==2 && FNR>2 {
                SRC[E] = 0+$1;
                TGT[E] = 0+$2;
                ++E;
            }

            function compare(a_edge_index, _v1, b_edge_index, _v2,   a_src_date, a_tgt_date, b_src_date, b_tgt_date) {
                a_src_date = DATE[SRC[a_edge_index]];
                a_tgt_date = DATE[TGT[a_edge_index]];

                b_src_date = DATE[SRC[b_edge_index]];
                b_tgt_date = DATE[TGT[b_edge_index]];

                if (a_src_date < b_src_date) return -1;
                else if (a_src_date > b_src_date) return 1;
                else if (a_tgt_date < b_tgt_date) return -1;
                else if (a_tgt_date > b_tgt_date) return 1;
                else if (a_edge_index < b_edge_index) return -1;
                else if (a_edge_index > b_edge_index) return 1;
                else return 0;
            }

            END {
                n = asorti(SRC, INDICES, "compare");
                print "src", "tgt";
                for (i=1; i<E; ++i) {
                    print SRC[INDICES[i]], TGT[INDICES[i]];
                }
            }
        ' \
        data/JS-Deps-filtered/nodes.csv \
        data/JS-Deps-filtered/edges.csv \
        ##
}

go-Sort-SO-Answers() {
    >SO-Answers.edges.sort_by_when.csv \
    awk '
        NR==1{ print; next }
        { print | "sort -t, -k3n" }
    ' \
        "${root:?}/data/SO-Answers-edgetime/edges.csv" \
        ##
}


#---

go-Large() {
    "${self:?}" \
        "${1:?}" \
        -e FG_TILE_WIDTH 2048 \
        -e FG_TILE_HEIGHT 2048 \
        -e FG_TILE_Z 0 \
        -e FG_TILE_X 0 \
        -e FG_TILE_Y 0 \
        "${@:2}" \
        ##
}

go-Stitch() {
    dsts=()

    for z in 1; do
    for x in 0 1; do
    for y in 0 1; do
        dst=${root:?}/${1:?}.z${z:?}x${x:?}y${y:?}.jpg

        "${self:?}" \
            "${1:?}" \
            -e FG_TILE_Z "${z:?}" \
            -e FG_TILE_X "${x:?}" \
            -e FG_TILE_Y "${y:?}" \
            -e FG_OUTPUT "${dst:?}" \
            "${@:2}" \
            ##
        
        dsts+=( "${dst:?}" )
    done
    done
    done

    >&2 printf -- \
        $'%s\n' \
        "${dsts[@]:?}" \
        ##
}

go-JS-Deps() {
    exec "${self:?}" python \
    exec "${root:?}/fg.py" \
        -x "${root:?}/fg.sh" \
        -i "${root:?}/JS-Deps.fgsl" \
        -f element "${root:?}/JS-Deps,kind=edge,name=index,type=2u32.bin" \
        -f X "${root:?}/JS-Deps,kind=node,name=x,type=f32.bin" \
        -f Y "${root:?}/JS-Deps,kind=node,name=y,type=f32.bin" \
        -f Date "${root:?}/JS-Deps,kind=node,name=date,type=u32.bin" \
        -f Devs "${root:?}/JS-Deps,kind=node,name=nmaintainers,type=u32.bin" \
        -f Vuln "${root:?}/JS-Deps,kind=node,name=cve,type=u32.bin" \
        -e FG_OUTPUT "${root:?}/JS-Deps.jpg" \
        "$@" \
        ##
}

go-SO-Answers() {
    exec "${self:?}" python \
    exec "${root:?}/SO-Answers.sh" \
    "${root:?}/fg.py" \
        -x "${root:?}/fg.sh" \
        -i "${root:?}/SO-Answers.fgsl" \
        -e FG_OUTPUT "${root:?}/SO-Answers.jpg" \
        "$@" \
        ##
}

go-exec() {
    exec "$@"
}


#---

test -f "${root:?}/env.sh" && source "${_:?}"
go-"$@"
