in=${1:-data/knit-graph/nodes.dat}
dtype=${2:-f4}

od() { command od -v -A n "$@"; }
log() { printf $'%s = %s\n' "$1" "${!1}"; }

i=0
j=0

N=8
count=$(<$in od -t u8 -j $j -N $N)
log count
((j+=N))

for ((k=0; k<count; ++k)); do
	N=8
	nsize=$(<$in od -t u8 -j $j -N $N)
	log nsize
	((j+=N))

	N=8
	ntype=$(<$in od -t u8 -j $j -N $N)
	log ntype
	((j+=N))

	N=8
	ncount=$(<$in od -t u8 -j $j -N $N)
	log ncount
	((j+=N))

	N=8
	nrange=$(<$in od -t $dtype -j $j -N $N)
	log nrange
	((j+=N))

	((i=j+nsize))

	N=16
	ndata=$(<$in od -t $dtype -j $j -N $N)
	log ndata
	((j+=N))

	((j=i))
done
