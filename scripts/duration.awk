#!/usr/bin/gawk -f

BEGIN {
	FS = OFS = "\t";
}

{
	task = substr($1, 1, 36);
	split($1, ary, "/");
	pathprefix = "";
	for (i=2; i<length(ary); ++i) {
		pathprefix = pathprefix "/" ary[i];
	}
	pathsuffix = ary[length(ary)];
	match($2, /^([0-9]+)\.([0-9]+)$/, ary);
	sec = ary[1];
	usec = ary[2];
	name = $3;
	value = $4;

	print $0;

	if (name == "@started") {
		s_sec[task,pathprefix] = sec;
		s_usec[task,pathprefix] = usec;
	} else if (name == "@finished") {
		diffusec = usec - s_usec[task, pathprefix];
		diffsec = sec - s_sec[task, pathprefix];
		while (diffusec < 0) { diffsec--; diffusec += 1000000 }
		while (diffusec > 1000000) { diffsec++; diffusec -= 1000000 }
		printf("%s%s/%d"OFS"%d.%06d"OFS"%s"OFS"%d.%06d\n", task, pathprefix, pathsuffix+1, sec, usec, "@duration", diffsec, diffusec);
	}
}
