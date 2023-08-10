#--- Seelab Machines

case "${HOSTNAME:-unset}" in (kavir)

# Kavir has a GPU: let's use it
docker_run+=(
    --runtime=nvidia
)

;; esac  # case "${HOSTNAME}" in


case "${HOSTNAME:-unset}" in (accona|sinai|kavir|gobi|thar|sahara)


# Some data is kept on the NAS, so make the whole NAS accessible.

docker_run+=(
    --mount="type=bind,src=/mnt/seenas2/data,dst=/mnt/seenas2/data"
)

# We need some utilities to help package up data for others.

go-Package-Data-Directories() {
    if [ $# -eq 0 ]; then
        set -- JS-Deps SO-Answers NBER-Patents
    fi

    for arg; do
        tar \
            --create \
            --file "${root:?}/${arg:?}.data.tar.gz" \
            --gzip \
            --verbose \
            --directory "${root:?}/examples/${arg:?}" \
            --dereference \
            data \
            ##
    done
}



;; esac  # case "${HOSTNAME}" in
