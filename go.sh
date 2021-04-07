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
	if [ $# -eq 0 ]; then
		cat logs/*.log | ./scripts/sort.sh | ./scripts/reorder.sh
	else
		tail -q "$@" logs/*.log
	fi | ./scripts/duration.awk | ./scripts/pretty.sh
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

use() {
	local dataset
	dataset=${1:?need dataset}

	ln -sf ${dataset:?}.index.html static/index.html
}

stress() {
    url='http://accona.eecs.utk.edu:8223/tile/cit-Patents/3/3/3/,vert,base64:I3ZlcnNpb24gMzAwIGVzCnByZWNpc2lvbiBtZWRpdW1wIGZsb2F0OwoKaW4gZmxvYXQgYU5vZGUxLCBhTm9kZTIsIGFOb2RlMywgYU5vZGU0LCBhTm9kZTUsIGFOb2RlNiwgYU5vZGU3LCBhTm9kZTg7CnVuaWZvcm0gZmxvYXQgdU5vZGVNaW4xLCB1Tm9kZU1pbjIsIHVOb2RlTWluMywgdU5vZGVNaW40LCB1Tm9kZU1pbjUsIHVOb2RlTWluNiwgdU5vZGVNaW43LCB1Tm9kZU1pbjg7CnVuaWZvcm0gZmxvYXQgdU5vZGVNYXgxLCB1Tm9kZU1heDIsIHVOb2RlTWF4MywgdU5vZGVNYXg0LCB1Tm9kZU1heDUsIHVOb2RlTWF4NiwgdU5vZGVNYXg3LCB1Tm9kZU1heDg7Cgp1bmlmb3JtIGZsb2F0IHVUcmFuc2xhdGVYLCB1VHJhbnNsYXRlWSwgdVNjYWxlOwoKb3V0IHZlYzQgZmdfRnJhZ0Nvb3JkOwpvdXQgZmxvYXQgZmdfQXR0cmlidXRlOwpvdXQgdmVjMyBmZ19BdHRyaWJ1dGVGVjM7CgojZGVmaW5lIG1haW4oKSBUaGVVc2VyTWFpbihvdXQgdmVjNCBmZ19Qb3NpdGlvbiwgb3V0IGZsb2F0IGZnX0F0dHJpYnV0ZSkKCgkJCSNkZWZpbmUgYVggYU5vZGUxCgkJCSNkZWZpbmUgdVhNaW4gdU5vZGVNaW4xCgkJCSNkZWZpbmUgdVhNYXggdU5vZGVNYXgxCgkJCgoJCQkjZGVmaW5lIGFZIGFOb2RlMgoJCQkjZGVmaW5lIHVZTWluIHVOb2RlTWluMgoJCQkjZGVmaW5lIHVZTWF4IHVOb2RlTWF4MgoJCQoKCQkJI2RlZmluZSBhQXBwRGF0ZSBhTm9kZTMKCQkJI2RlZmluZSB1QXBwRGF0ZU1pbiB1Tm9kZU1pbjMKCQkJI2RlZmluZSB1QXBwRGF0ZU1heCB1Tm9kZU1heDMKCQkKCgkJCSNkZWZpbmUgYUdvdERhdGUgYU5vZGU0CgkJCSNkZWZpbmUgdUdvdERhdGVNaW4gdU5vZGVNaW40CgkJCSNkZWZpbmUgdUdvdERhdGVNYXggdU5vZGVNYXg0CgkJCgoJCQkjZGVmaW5lIGFOQ2xhc3MgYU5vZGU1CgkJCSNkZWZpbmUgdU5DbGFzc01pbiB1Tm9kZU1pbjUKCQkJI2RlZmluZSB1TkNsYXNzTWF4IHVOb2RlTWF4NQoJCQoKCQkJI2RlZmluZSBhQ0NsYXNzIGFOb2RlNgoJCQkjZGVmaW5lIHVDQ2xhc3NNaW4gdU5vZGVNaW42CgkJCSNkZWZpbmUgdUNDbGFzc01heCB1Tm9kZU1heDYKCQkKCQl2b2lkIG1haW4oKSB7CgkJCWZsb2F0IHggPSAoYVggLSB1WE1pbikgLyAodVhNYXggLSB1WE1pbik7CgkJCWZsb2F0IHkgPSAoYVkgLSB1WU1pbikgLyAodVlNYXggLSB1WU1pbik7CgoJCQlmbG9hdCBhdHRyaWIgPSAoYUFwcERhdGUgLSB1QXBwRGF0ZU1pbikgLyAodUFwcERhdGVNYXggLSB1QXBwRGF0ZU1pbik7CgoJCQl2ZWMzIGNvbG9yc1s2XSA9IHZlYzNbNl0oCgkJCQl2ZWMzKDAuODYsIDAuMzcxMiwgMC4zMzk5OSksCgkJCQl2ZWMzKDAuODI4NzksIDAuODYsIDAuMzM5OTkpLAoJCQkJdmVjMygwLjMzOTk5LCAwLjg2LCAwLjM3MTIpLAoJCQkJdmVjMygwLjMzOTk5LCAwLjgyODc5LCAwLjg2KSwKCQkJCXZlYzMoMC4zNzEyLCAwLjMzOTk5LCAwLjg2KSwKCQkJCXZlYzMoMC44NiwgMC4zMzk5OSwgMC44Mjg3OSkKCQkJKTsKCQkJZmdfQXR0cmlidXRlRlYzID0gY29sb3JzW2ludChhQ0NsYXNzIC0gMS4wKV07CgoJCQlmZ19Qb3NpdGlvbiA9IHZlYzQoeCwgeSwgYXR0cmliLCAxLjApOwoJCQlmZ19BdHRyaWJ1dGUgPSAxLjAgLSBhdHRyaWI7CgkJfQoJCiN1bmRlZiBtYWluCgp2b2lkIG1haW4oKSB7Cgl2ZWM0IHBvczsKCWZsb2F0IGF0dHJpYjsKCglUaGVVc2VyTWFpbihwb3MsIGF0dHJpYik7Cgl2ZWMyIHNjYWxlZCA9IHVTY2FsZSAqIHBvcy54eSArIHZlYzIodVRyYW5zbGF0ZVgsIHVUcmFuc2xhdGVZKTsKCglmZ19GcmFnQ29vcmQgPSBwb3M7CglmZ19BdHRyaWJ1dGUgPSBhdHRyaWI7CglnbF9Qb3NpdGlvbiA9IHZlYzQoMi4wICogc2NhbGVkIC0gMS4wLCBwb3MuencpOwp9Cg==,frag,base64:I3ZlcnNpb24gMzAwIGVzCnByZWNpc2lvbiBtZWRpdW1wIGZsb2F0OwoKaW4gdmVjNCBmZ19GcmFnQ29vcmQ7CmluIGZsb2F0IGZnX0F0dHJpYnV0ZTsKaW4gdmVjMyBmZ19BdHRyaWJ1dGVGVjM7CgpvdXQgdmVjNCBmZ19GcmFnQ29sb3I7CgojZGVmaW5lIG1haW4oKSBUaGVVc2VyTWFpbihvdXQgdmVjNCBmZ19GcmFnQ29sb3IpCgoJCXZvaWQgbWFpbigpIHsKCQkJZmdfRnJhZ0NvbG9yID0gdmVjNChmZ19BdHRyaWJ1dGUgKiBmZ19BdHRyaWJ1dGVGVjMsIDEuMCk7CgkJfQoJCiN1bmRlZiBtYWluCgp2b2lkIG1haW4oKSB7Cgl2ZWM0IGNvbG9yOwoKCVRoZVVzZXJNYWluKGNvbG9yKTsKCglmZ19GcmFnQ29sb3IgPSBjb2xvcjsKfQo=,dcIdent,666,dcIndex,base64:AwAAAAAAAAA=,dcMult,base64:AACAPw==,dcOffset,base64:AAAAAA==,dcMinMult,0,dcMaxMult,1'

    printf $'name,replicas,z_inc,nthreads,pDepth,mode,real,user,sys\n' >>stress.csv

    for pDepth in 4 0 10; do
    for replicas in 1 2 3 4 5 6 7 8; do
    for z_inc in 0 1 2 3 4; do
    for nthreads in 6 12; do
    for id in "replicas=${replicas:?};z_inc=${z_inc:?};nthreads=${nthreads:?}"; do

    destroy
    build
    mv logs logs.bak
    mkdir -p "_stress/${id:?}"
    ln -sf "_stress/${id:?}" logs
    replicas=$replicas create-fg

    for mode in prime test1 test2 test3; do

    printf $'%s,%s,%s,%s,%s,' "${id:?}" "${replicas:?}" "${z_inc:?}" "${pDepth:?}" "${mode:?}" >>stress.csv

    /usr/bin/time \
    --format='%e,%U,%S' \
        timeout 10m \
            python3.8 stitch.py \
            --nthreads ${nthreads:?} \
            --z-inc ${z_inc:?} \
            --mode noop \
            -o /dev/null \
            <<<"${url:?},pDepth,${pDepth:?}" \
    2>>stress.csv

    done

    destroy
    rm logs
    mv logs.bak logs

    done
    done
    done
    done
    done
}

"$@"
