#!/usr/bin/env bash

uniq --group=append --check-chars=36 | \
awk '

BEGIN {
	FS = "\t";
}

NF > 1 {
	task = substr($1, 1, 36);
	level = substr($1, 37);
	time = $2;
	name = $3;
	value = $4;

	if (index(name, " ")) sep = " ";
	else if (index(name, "@")) sep = " ";
	else sep = "=";
	print strftime("%Y-%m-%d %H:%M:%S", time), level, name sep value;
	next;
}

1
'
#match($0, /^([^@]+)@([^:]+):(.+)$/, ary) { print strftime("%Y-%m-%d %H:%M:%S", ary[2])" "ary[1]" "ary[3]; next } 1'
