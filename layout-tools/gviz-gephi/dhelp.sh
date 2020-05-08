name="gviz-gephi"
scriptpath=`realpath ./scripts`
datapath=`realpath ../layouts`
outpath=`realpath ../processed`

run(){
        docker run --rm -it --name=${name} -d -v ${scriptpath}:/scripts -v ${datapath}:/mnt -v ${outpath}/${name}:/out ${name} /bin/bash
        #docker run --rm -it --name=${name} -d ${name} /bin/bash
}

stop() {
        docker stop ${name}
}

build() {
        docker build . -t ${name}
}

connect() {
        docker exec -it ${name} /bin/bash
}

save() {
        docker save ${name} > ${name}.tar
}

"$@"