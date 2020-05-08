#! /bin/bash

process_csv(){
  file=${1:?need file}
  out=${2:?need out}
  java -Djava.awt.headless=true -Xms8g -Xmx64g -cp /forceatlas2.jar:/gephi-toolkit-0.9.2-all.jar kco.forceatlas2.Main \
    --input ${file} \
    --output ${out} \
    --2d \
    --format csv \
    --nsteps 10000
}

"$@"