case "${HOSTNAME:-unset}" in
(accona|sinai|kavir|gobi|thar|sahara)
    go-python-exec() {
        PATH=/home/pprovins/opt/python-3.8.1/bin${PATH:+:${PATH:?}} \
        exec "$@"
    }

    docker_run+=(
        --mount="type=bind,src=/mnt/seenas2/data,dst=/mnt/seenas2/data"
    )
    ;;

esac

if [ "${USER:-}" = "thobson2" ]; then
    #port=9334
    port=8080
else
    port=8223
fi
# replicas=1
# PATH=${PATH:+${PATH:?}:}/home/pprovins/opt/python-3.8.1/bin
# cache=/mnt/seenas2/data${root:?}/cache
