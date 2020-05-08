name="layout-tool"

run(){
        docker run --rm -it --name=${name} -d -v $(pwd)/scripts:/scripts -v $(pwd)/layouts:/mnt -v $(pwd)/processed:/out ${name} /bin/bash
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