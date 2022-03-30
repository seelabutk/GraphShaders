#!/usr/bin/env bash

die() { printf $'Error: %s\n' "$*" >&2; exit 1; }

tag=fg_$USER:latest
name=fg_$USER
target=
data='/mnt/seenas2/data/snap'
cache=
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
tty=1
declare -A urls

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
		${interactive:+-i} \
		${tty:+-t} \
		${script:+-a stdin -a stdout -a stderr --sig-proxy=true} \
		${ipc:+--ipc=$ipc} \
		${net:+--net=$net} \
		${user:+-u $(id -u):$(id -g)} \
		${cwd:+-v $PWD:$PWD -w $PWD} \
		${port:+-p $port:$port} \
		${data:+-v $data:$data} \
		${cache:+-v $cache:$cache} \
		${runtime:+--runtime $runtime} \
		${cap_add:+--cap-add=$cap_add} \
		${xauth:+-e DISPLAY -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /etc/sudoers.d:/etc/sudoers.d:ro -v $xauth:$xauth -e XAUTHORITY=$xauth} \
		${entrypoint:+--entrypoint $entrypoint} \
		--detach-keys="ctrl-q,ctrl-q" \
		$tag \
		"$@"
}

inspect() {
	entrypoint='/bin/bash -i' run "$@"
}

script() {
	interactive=1 tty= script=1 run "$@"
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
		FG_LOGFILE=/dev/stderr \
		gdb \
			-ex='set confirm on' \
			-ex='set pagination off' \
			-ex=r \
			-ex=q \
			--args \
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

partition() {
	for app in COM-Orkut; do
	for url in "${urls[${app:?}]}"; do

	/usr/bin/time \
		--format="prime,app,${app:?},real,%e,user,%U,sys,%S" \
		curl \
			--silent \
			--location \
			--show-error \
			--output /dev/null \
			"${url:?},pDepth,0,doRepartition,1"

	for pDepth in 6; do

	/usr/bin/time \
		--format="test,app,${app:?},pDepth,${pDepth:?},real,%e,user,%U,sys,%S" \
		curl \
			--silent \
			--location \
			--show-error \
			--output /dev/null \
			"${url:?},pDepth,${pDepth:?},doRepartition,1" \
	|| return 1

	done
	done
	done
}

# use() {
# 	local dataset
# 	dataset=${1:?need dataset}
#
# 	ln -sf ${dataset:?}.index.html static/index.html
# }

stitch() {
	for replicas in 6; do

    destroy
    build
    create-fg

	for app in JS-Deps SO-Answers NBER-Patents; do
	for url in "${urls[${app:?}]}"; do
	for pDepth in {0..10..2}; do
	for nthreads in 6; do
	for zinc in 4; do
	for resolution in $(( 2 ** ( zinc + 1 ) * 256 )); do
	for id in "${app:?},nthreads${nthreads:?},pDepth${pDepth:?},replicas${replicas:?},zinc${zinc:?}"; do

	/usr/bin/time \
		--format="${id:?},real%e,user%U,sys%S" \
			python3.8 stitch.py \
				--nthreads ${nthreads:?} \
				--z-inc ${zinc:?} \
				--mode stitch \
				-o "tmp/${id:?}.${resolution:?}x${resolution:?}.png" \
				<<<"${url:?},pDepth,${pDepth:?}" \
			|| die "stitch.py failed"

	done
	done
	done
	done
	done
	done
	done

    destroy

	done
}

stress() {
    #url='http://accona.eecs.utk.edu:8365/tile/knit-graph/5/15/15/,vert,base64:CgkJdm9pZCBub2RlKGluIGZsb2F0IHgsIGluIGZsb2F0IHksIGluIGZsb2F0IGRhdGUsIGluIGZsb2F0IF9udW1NYWludCwgaW4gZmxvYXQgY3ZlLCBvdXQgZmxvYXQgdnVsbmVyYWJsZSkgewoJCQl2dWxuZXJhYmxlID0gY3ZlOwoJCQlmZ19Ob2RlUG9zaXRpb24gPSB2ZWMyKHgsIHkpOwogICAgICAgICAgICBmZ19Ob2RlRGVwdGggPSBmZ19tZWFuKHgpOwoJCX0KCQ==,frag,base64:CgkJdm9pZCBlZGdlKGluIGZsb2F0IHZ1bG5lcmFibGUpIHsKCQkJdmVjNCByZWQgPSB2ZWM0KDAuOCwgMC4xLCAwLjEsIDEuMCk7CgkJCXZlYzQgZ3JlZW4gPSB2ZWM0KDAuMCwgMS4wLCAwLjAsIDEuMCk7CgkJCXZlYzQgY29sb3IgPSBtaXgocmVkLCBncmVlbiwgdnVsbmVyYWJsZSk7CgkJCWZnX0ZyYWdDb2xvciA9ICgxLjAgLSBmZ19GcmFnRGVwdGgpICogY29sb3I7CgkJfQoJ,pDepth,10,dcIdent,590,dcIndex,base64:AQAAAAAAAAA=,dcMult,base64:AACAPw==,dcOffset,base64:AAAAAA==,dcMinMult,0.5,dcMaxMult,0.5'
	url='http://kavir.eecs.utk.edu:8365/tile/cit-Patents/3/3/3/,vert,base64:CgkJdm9pZCBub2RlKGluIGZsb2F0IHgsIGluIGZsb2F0IHksIGluIGZsb2F0IGFwcGRhdGUsIGluIGZsb2F0IGdvdGRhdGUsIGluIGZsb2F0IG5jbGFzcywgaW4gZmxvYXQgY2NsYXNzLCBvdXQgdmVjMyBjb2xvcikgewoJCQkJCWZnX05vZGVQb3NpdGlvbiA9IHZlYzIoeCwgeSk7CgkJCQkJZmdfTm9kZURlcHRoID0gZmdfbWluKGFwcGRhdGUpOwoJCQkJCWNvbG9yID0gdUNhdDZbaW50KDUuMCpjY2xhc3MpXTsKCQkJfQoJ,frag,base64:CgkJdm9pZCBlZGdlKGluIHZlYzMgY29sb3IpIHsKCQkJZmdfRnJhZ0NvbG9yID0gdmVjNChmZ19GcmFnRGVwdGggKiBjb2xvciwgMS4wKTsKCQl9Cgk=,pDepth,10,dcIdent,1,dcIndex,base64:AwAAAAAAAAA=,dcMult,base64:AACAPw==,dcOffset,base64:AAAAAA==,dcMinMult,1,dcMaxMult,0'

	printf $'name,replicas,z_inc,nthreads,pDepth,mode,real,user,sys\n' >> stress.csv

    for pDepth in 4 0 10; do
    for replicas in 1 2 3 4 5 6 7 8; do
    for z_inc in 0 1 2 3 4; do
    for nthreads in 6 12; do
	for id in "replicas${replicas:?},z_inc${z_inc:?},nthreads${nthreads}"; do
    
	destroy
    build

    mv logs logs.bak
    mkdir -p "_stress/${id:?}"
    ln -sf "_stress/${id:?}" logs
	cat <<-EOF >"_stress/${id:?}/README"
		The logs in this directory aim to determine how FG's performance is
		under the following conditions.
		
		The client is configured to...
		- use a set of ${nthreads:?} simultaneous rendering requests
		- request an image made up of 2^(${z_inc:?}+1) = $(( 2 ** (z_inc + 1) )) tiles of size 256x256 (total size = $(( 2 ** (z_inc + 1) * 256) )) pixels square)

		The server is configured to...
		- use a swarm of ${replicas:?} FG server instances
		- use the ${app:?} dataset
		- partition the data into 2^${pDepth} = $(( 2 ** pDepth )) tiles
	EOF

    replicas=$replicas create-fg

    for mode in prime test1 test2 test3; do

    sleep 0.5 || break

    printf $'%s,%s,%s,%s,%s,%s,' "${id:?}" "${replicas:?}" "${z_inc:?}" "${nthreads:?}" "${pDepth:?}" "${mode:?}" >> stress.csv

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
