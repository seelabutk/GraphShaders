#!/usr/bin/env bash

tag=fg_$USER:latest
name=fg_$USER
target=
data='/mnt/seenas2/data/snap'
registry= #accona.eecs.utk.edu:5000
xauth=
entrypoint=
ipc=
net=
user=1
cwd=1
interactive=1
script=
port=8889
constraint=
runtime=
network=fg_$USER
cap_add=SYS_PTRACE
replicas=8

[ -f env.sh ] && . env.sh

build() {
	docker build \
		${target:+--target $target} \
		-t $tag .
}

run() {	
	if [ -n "$xauth" ]; then
		rm -f $xauth
		xauth nlist $DISPLAY | sed -e 's/^..../ffff/' | xauth -f $xauth nmerge -
	fi
	docker run --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
		--rm \
		${interactive:+-it} \
		${script:+-a stdin -a stdout -a stderr --sig-proxy=true} \
		${ipc:+--ipc=$ipc} \
		${net:+--net=$net} \
		${user:+-u $(id -u):$(id -g)} \
		${cwd:+-v $PWD:$PWD -w $PWD} \
		${port:+-p $port:$port} \
		${data:+-v $data:$data} \
		${runtime:+--runtime $runtime} \
		${cap_add:+--cap-add=$cap_add} \
		${xauth:+-e DISPLAY -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /etc/sudoers.d:/etc/sudoers.d:ro -v $xauth:$xauth -e XAUTHORITY=$xauth} \
		${entrypoint:+--entrypoint $entrypoint} \
		$tag \
		"$@"
}

inspect() {
	entrypoint='/bin/bash -i' run "$@"
}

script() {
	interactive= script=1 run "$@"
}

network() {
	docker network create \
		--driver overlay \
		${port:+-p $port:$port} \
		${network:?}
}

push() {
	docker tag $tag ${registry:?}/$tag
	docker push ${registry:?}/$tag
}

create() {
	docker service create \
		${name:+--name $name} \
		${net:+--network=$net} \
		${cwd:+--mount type=bind,src=$PWD,dst=$PWD -w $PWD} \
		${data:+--mount type=bind,src=$data,dst=$data} \
		${port:+-p $port:$port} \
		${constraint:+--constraint=$constraint} \
		${replicas:+--replicas=$replicas} \
		${registry:+$registry/}$tag \
		"$@"
}

destroy() {
	docker service rm $name
}

logs() {
	docker service logs $name "$@"
}

fg() {
	run env \
		FG_PORT=$port \
		FG_SERVICE=0 \
		gdb -ex=r --args \
		/app/build/server \
		"$@"
}

create-fg() {
	create env \
		FG_SERVICE=1 \
		FG_PORT=$port \
		/app/build/server \
		"$@"
}

"$@"
