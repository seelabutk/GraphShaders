#!/usr/bin/env bash

die() { printf $'Error: %s\n' "$*" >&2; exit 1; }
root=$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)

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
replicas=1
tty=1
declare -A urls

test -f "${root:?}/env.sh" && source "${_:?}"

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
		${cache:+--mount type=bind,src=$cache,dst=$cache} \
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

makefigs() {
	local f
	for f; do
		makefig "$f" \
		|| die "Failed to makefig $f"
	done
}

makefig() {
	id=${1:?need id}
	url=${figurls["${id:?}"]:?}

	/usr/bin/time \
		--format="${id:?},real%e,user%U,sys%S" \
			python3.8 stitch.py \
				--nthreads 1 \
				--z-inc 1 \
				--mode stitch \
				-o "tmp/${id:?}.png" \
				<<<"${url:?}" \
			|| die "stitch.py failed"
}

stress() {
    #url='http://accona.eecs.utk.edu:8365/tile/knit-graph/5/15/15/,vert,base64:CgkJdm9pZCBub2RlKGluIGZsb2F0IHgsIGluIGZsb2F0IHksIGluIGZsb2F0IGRhdGUsIGluIGZsb2F0IF9udW1NYWludCwgaW4gZmxvYXQgY3ZlLCBvdXQgZmxvYXQgdnVsbmVyYWJsZSkgewoJCQl2dWxuZXJhYmxlID0gY3ZlOwoJCQlmZ19Ob2RlUG9zaXRpb24gPSB2ZWMyKHgsIHkpOwogICAgICAgICAgICBmZ19Ob2RlRGVwdGggPSBmZ19tZWFuKHgpOwoJCX0KCQ==,frag,base64:CgkJdm9pZCBlZGdlKGluIGZsb2F0IHZ1bG5lcmFibGUpIHsKCQkJdmVjNCByZWQgPSB2ZWM0KDAuOCwgMC4xLCAwLjEsIDEuMCk7CgkJCXZlYzQgZ3JlZW4gPSB2ZWM0KDAuMCwgMS4wLCAwLjAsIDEuMCk7CgkJCXZlYzQgY29sb3IgPSBtaXgocmVkLCBncmVlbiwgdnVsbmVyYWJsZSk7CgkJCWZnX0ZyYWdDb2xvciA9ICgxLjAgLSBmZ19GcmFnRGVwdGgpICogY29sb3I7CgkJfQoJ,pDepth,10,dcIdent,590,dcIndex,base64:AQAAAAAAAAA=,dcMult,base64:AACAPw==,dcOffset,base64:AAAAAA==,dcMinMult,0.5,dcMaxMult,0.5'
	#url='http://kavir.eecs.utk.edu:8365/tile/cit-Patents/3/3/3/,vert,base64:CgkJdm9pZCBub2RlKGluIGZsb2F0IHgsIGluIGZsb2F0IHksIGluIGZsb2F0IGFwcGRhdGUsIGluIGZsb2F0IGdvdGRhdGUsIGluIGZsb2F0IG5jbGFzcywgaW4gZmxvYXQgY2NsYXNzLCBvdXQgdmVjMyBjb2xvcikgewoJCQkJCWZnX05vZGVQb3NpdGlvbiA9IHZlYzIoeCwgeSk7CgkJCQkJZmdfTm9kZURlcHRoID0gZmdfbWluKGFwcGRhdGUpOwoJCQkJCWNvbG9yID0gdUNhdDZbaW50KDUuMCpjY2xhc3MpXTsKCQkJfQoJ,frag,base64:CgkJdm9pZCBlZGdlKGluIHZlYzMgY29sb3IpIHsKCQkJZmdfRnJhZ0NvbG9yID0gdmVjNChmZ19GcmFnRGVwdGggKiBjb2xvciwgMS4wKTsKCQl9Cgk=,pDepth,10,dcIdent,1,dcIndex,base64:AwAAAAAAAAA=,dcMult,base64:AACAPw==,dcOffset,base64:AAAAAA==,dcMinMult,1,dcMaxMult,0'
	url='http://kavir.eecs.utk.edu:9334/tile/SO-Answers-edgetime/1/0/0/,vert,base64:dm9pZCBub2RlKGluIHVuaXQgWCwgaW4gdW5pdCBZLCBpbiB1bml0IEZpcnN0Q29udHJpYnV0aW9uLCBpbiB1bml0IExhc3RDb250cmlidXRpb24sIGluIHVuaXQgSW5EZWdyZWUsIGluIHVuaXQgT3V0RGVncmVlLCBvdXQgZmxhdCBmbG9hdCBmYywgb3V0IGZsYXQgZmxvYXQgbGMsIG91dCBmbGF0IGZsb2F0IGlkLCBvdXQgZmxhdCBmbG9hdCBvZCkgewogICAgZmdfTm9kZVBvc2l0aW9uID0gdmVjMihYLCBZKTsKICAgIGZnX05vZGVEZXB0aCA9IGZnX21pbigtRmlyc3RDb250cmlidXRpb24pOwogICAgZmMgPSBGaXJzdENvbnRyaWJ1dGlvbjsKICAgIGxjID0gTGFzdENvbnRyaWJ1dGlvbjsKICAgIGlkID0gSW5EZWdyZWU7CiAgICBvZCA9IE91dERlZ3JlZTsKfQ==,frag,base64:CmNvbnN0IHZlYzMgUGFpcmVkNENsYXNzW10gPSB2ZWMzW10oCiAgICB2ZWMzKDE2Ni4sIDIwNi4sIDIyNy4pIC8gMjU1LiwKICAgIHZlYzMoIDMxLiwgMTIwLiwgMTgwLikgLyAyNTUuLAogICAgdmVjMygxNzguLCAyMjMuLCAxMzguKSAvIDI1NS4sCiAgICB2ZWMzKCA1MS4sIDE2MC4sICA0NC4pIC8gMjU1Lik7Cgp2b2lkIGVkZ2UoaW4gZmxhdCBmbG9hdCBmYywgaW4gZmxhdCBmbG9hdCBsYywgaW4gZmxhdCBmbG9hdCBpZCwgaW4gZmxhdCBmbG9hdCBvZCwgaW4gdW5pdCBDdXJyZW50Q29udHJpYnV0aW9uLCBpbiB1bml0IFNlbmRlckxvbmdldml0eSwgaW4gdW5pdCBSZWNlaXZlckxvbmdldml0eSkgewogICAgZmdfRWRnZUNvbG9yID0gdmVjNCgxLi8xNi4pOwogICAgaW50IGluZGV4ID0gMDsKICAgIGluZGV4ICs9IDIgKiBpbnQoaWQgPiBvZCk7IC8vIGJsdWUgKGZhbHNlKSB2cyBncmVlbiAodHJ1ZSkKICAgIGluZGV4ICs9IDEgKiBpbnQoU2VuZGVyTG9uZ2V2aXR5ID49IFJlY2VpdmVyTG9uZ2V2aXR5KTsgLy8gbGlnaHQgKGZhbHNlKSB2cyBkYXJrICh0cnVlKQogICAgZmdfRWRnZUNvbG9yLnJnYiA9IFBhaXJlZDRDbGFzc1tpbmRleF07Cn0=,pDepth,0,dcIdent,6380910371648010,dcIndex,base64:AwAAAAAAAAA=,dcMult,base64:AACAvw==,dcOffset,base64:AAAAgA==,dcMinMult,base64:MQ==,dcMaxMult,base64:MA==,doScissorTest,1'
	app=SO-Answers

	printf $'name,replicas,z_inc,nthreads,pDepth,mode,real,user,sys\n' >> stress.csv

    for pDepth in 10; do
    for replicas in 7; do
    for z_inc in 4; do
    for nthreads in 6; do
	for id in "c,replicas${replicas:?},z_inc${z_inc:?},nthreads${nthreads}"; do

	sleep 1 || break

	if [ -d "_stress/${id:?}" ]; then
		continue
	fi
    
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
		- request an image made up of 2^(${z_inc:?}+1) = $(( 2 ** (z_inc + 1) )) tiles of size 256x256 (total size = $(( 2 ** (z_inc + 1) * 256 )) pixels square)
		- base url: ${url:?}

		The server is configured to...
		- use a swarm of ${replicas:?} FG server instances
		- use the ${app:?} dataset
		- partition the data into 2^${pDepth} = $(( 2 ** pDepth )) tiles
	EOF

    replicas=$replicas create-fg

    for mode in prime3 test{31..60}; do
	case "${mode:?}" in
	(prime*) timeout=20m;;
	(test*) timeout=1m;;
	(*) timeout=10m; printf $'Unknown mode: %q\n' "${mode:?}" >&2;;
	esac

    sleep 0.5 || break

    printf $'%s,%s,%s,%s,%s,%s,' "${id:?}" "${replicas:?}" "${z_inc:?}" "${nthreads:?}" "${pDepth:?}" "${mode:?}" >> stress.csv

    /usr/bin/time \
    --format='%e,%U,%S' \
        timeout "${timeout:?}" \
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

randstress() {
	url='http://kavir.eecs.utk.edu:9334/tile/SO-Answers-edgetime/1/0/0/,vert,base64:dm9pZCBub2RlKGluIHVuaXQgWCwgaW4gdW5pdCBZLCBpbiB1bml0IEZpcnN0Q29udHJpYnV0aW9uLCBpbiB1bml0IExhc3RDb250cmlidXRpb24sIGluIHVuaXQgSW5EZWdyZWUsIGluIHVuaXQgT3V0RGVncmVlLCBvdXQgZmxhdCBmbG9hdCBmYywgb3V0IGZsYXQgZmxvYXQgbGMsIG91dCBmbGF0IGZsb2F0IGlkLCBvdXQgZmxhdCBmbG9hdCBvZCkgewogICAgZmdfTm9kZVBvc2l0aW9uID0gdmVjMihYLCBZKTsKICAgIGZnX05vZGVEZXB0aCA9IGZnX21pbigtRmlyc3RDb250cmlidXRpb24pOwogICAgZmMgPSBGaXJzdENvbnRyaWJ1dGlvbjsKICAgIGxjID0gTGFzdENvbnRyaWJ1dGlvbjsKICAgIGlkID0gSW5EZWdyZWU7CiAgICBvZCA9IE91dERlZ3JlZTsKfQ==,frag,base64:CmNvbnN0IHZlYzMgUGFpcmVkNENsYXNzW10gPSB2ZWMzW10oCiAgICB2ZWMzKDE2Ni4sIDIwNi4sIDIyNy4pIC8gMjU1LiwKICAgIHZlYzMoIDMxLiwgMTIwLiwgMTgwLikgLyAyNTUuLAogICAgdmVjMygxNzguLCAyMjMuLCAxMzguKSAvIDI1NS4sCiAgICB2ZWMzKCA1MS4sIDE2MC4sICA0NC4pIC8gMjU1Lik7Cgp2b2lkIGVkZ2UoaW4gZmxhdCBmbG9hdCBmYywgaW4gZmxhdCBmbG9hdCBsYywgaW4gZmxhdCBmbG9hdCBpZCwgaW4gZmxhdCBmbG9hdCBvZCwgaW4gdW5pdCBDdXJyZW50Q29udHJpYnV0aW9uLCBpbiB1bml0IFNlbmRlckxvbmdldml0eSwgaW4gdW5pdCBSZWNlaXZlckxvbmdldml0eSkgewogICAgZmdfRWRnZUNvbG9yID0gdmVjNCgxLi8xNi4pOwogICAgaW50IGluZGV4ID0gMDsKICAgIGluZGV4ICs9IDIgKiBpbnQoaWQgPiBvZCk7IC8vIGJsdWUgKGZhbHNlKSB2cyBncmVlbiAodHJ1ZSkKICAgIGluZGV4ICs9IDEgKiBpbnQoU2VuZGVyTG9uZ2V2aXR5ID49IFJlY2VpdmVyTG9uZ2V2aXR5KTsgLy8gbGlnaHQgKGZhbHNlKSB2cyBkYXJrICh0cnVlKQogICAgZmdfRWRnZUNvbG9yLnJnYiA9IFBhaXJlZDRDbGFzc1tpbmRleF07Cn0=,pDepth,0,dcIdent,6380910371648010,dcIndex,base64:AwAAAAAAAAA=,dcMult,base64:AACAvw==,dcOffset,base64:AAAAgA==,dcMinMult,base64:MQ==,dcMaxMult,base64:MA==,doScissorTest,1'
	app='SO-Answers'

	printf $'name,replicas,nthreads,pDepth,count,mode,real,user,sys\n' >> randstress.csv

    for pDepth in 10; do
    for replicas in {1..8}; do
    for nthreads in 6 120; do
	for id in "e;replicas${replicas:?};nthreads${nthreads}"; do

	if [ -d "_stress/${id:?}" ]; then
		continue
	fi
    
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
		- randomly generate requests at different zoom levels
		- base url: ${url:?}

		The server is configured to...
		- use a swarm of ${replicas:?} FG server instances
		- use the ${app:?} dataset
		- partition the data into 2^${pDepth} = $(( 2 ** pDepth )) tiles
	EOF

    replicas=${replicas:?} create-fg

	for mode in prime1 test{1..30}; do
	
	case "${mode:?}" in
	(prime*) count=16;;
	(test*) count=1024;;
	esac
	
	case "${mode:?}" in
	(prime*) timeout=20m;;
	(test*) timeout=120s;;
	esac

    sleep 0.5 || break

    printf $'%s,%s,%s,%s,%s,%s,' "${id:?}" "${replicas:?}" "${nthreads:?}" "${pDepth:?}" "${count:?}" "${mode:?}" >> randstress.csv

    /usr/bin/time \
    --format='%e,%U,%S' \
        timeout "${timeout:?}" \
            python3.8 randstitch.py \
            --nthreads ${nthreads:?} \
			--count "${count:?}" \
			--output "_stress/${id:?}/randstitch.vega.json" \
            <<<"${url:?},pDepth,${pDepth:?}" \
    2>>randstress.csv

    done

    destroy
    rm logs
    mv logs.bak logs

    done
    done
    done
    done
}

randstitch() {

	python3.8 randstitch.py \
		--count 16 \
		--nthreads 6 \
		<<<"${url:?}"
}

makeanim() {
	files=()

	for id in SO-Answers-8 SO-Answers-7; do
	for url in "${figurls[$id]}"; do
	for lo in {0..99}; do
	printf -v hi $'%03d' "$((lo+1))"
	printf -v lo $'%03d' "${lo:?}"

	oldfrag=${url##*,frag,base64:}
	oldfrag=${oldfrag%%,*}
	newfrag=$(base64 -d <<<"${oldfrag:?}")
	newfrag=${newfrag/\#define LO 0.33/#define LO ${lo:0:1}.${lo:1}}
	newfrag=${newfrag/\#define HI 0.34/#define HI ${hi:0:1}.${hi:1}}
	newfrag=$(base64 -w 0 <<<"${newfrag:?}" | tr -d $'\n')

	newurl=${url/,frag,base64:${oldfrag:?},/,frag,base64:${newfrag:?},}

	file=${id:?},lo=${lo:?},hi=${hi:?}.png
	files+=( "${file:?}" )
	if [ -e "${file:?}" ]; then
		continue
	fi

	/usr/bin/time \
		--format="${id:?},real%e,user%U,sys%S" \
			python3.8 stitch.py \
				--nthreads 1 \
				--z-inc 1 \
				--mode stitch \
				-o "${file:?}" \
				<<<"${newurl:?}" \
			|| die "stitch.py failed"

	done
	done

	file=${id:?}.mp4
	if [ -e "${file:?}" ]; then
		continue
	fi

	"${HOME:?}/opt/magick/go.sh" docker exec ffmpeg \
		-framerate 10 \
		-pattern_type glob \
		-i "${id:?},lo=*,hi=*.png" \
		-c:v libx264 \
		-pix_fmt yuv420p \
		"${file:?}"

	done
}

makepatentanim() {
	files=()

	for id in NBER-Patents-8; do
	for url in "${figurls[$id]}"; do
	for Z in 2; do
	for I in {0..3}; do
	for J in {0..30}; do

	oldfrag=${url##*,frag,base64:}
	oldfrag=${oldfrag%%,*}
	newfrag=$(base64 -d <<<"${oldfrag:?}")
	newfrag=${newfrag/\#define I 0/#define I ${I:?}}
	newfrag=${newfrag/\#define J 0/#define J ${J:?}}
	newfrag=$(base64 -w 0 <<<"${newfrag:?}" | tr -d $'\n')

	newurl=${url/,frag,base64:${oldfrag:?},/,frag,base64:${newfrag:?},}

	printf -v J $'%02d' "${J:?}"

	file=${id:?},I=${I:?},J=${J:?},Z=${Z:?}.png
	files+=( "${file:?}" )
	if [ -e "${file:?}" ]; then
		continue
	fi

	/usr/bin/time \
		--format="${id:?},real%e,user%U,sys%S" \
			python3.8 stitch.py \
				--nthreads 1 \
				--z-inc ${Z:?} \
				--mode stitch \
				-o "${file:?}" \
				<<<"${newurl:?}" \
			|| die "stitch.py failed"

	done

	file=${id:?},I=${I:?},Z=${Z:?}.mp4
	if [ -e "${file:?}" ]; then
		continue
	fi

	"${HOME:?}/opt/magick/go.sh" docker exec ffmpeg \
		-framerate 10 \
		-pattern_type glob \
		-i "${id:?},I=${I:?},J=*,Z=${Z:?}.png" \
		-c:v libx264 \
		-pix_fmt yuv420p \
		"${file:?}"

	done
	done
	done
	done
}

"$@"
