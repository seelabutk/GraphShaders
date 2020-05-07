#!/usr/bin/gawk -f

BEGIN {
	FS = OFS = "\t";
	split("", times);
	split("", lines);
	split("", nlines);
}

{
	task = substr($1, 1, 36);
	level = substr($1, 37);
	time = $2;
	name = $3;
	value = $4;

	if (!(task in times)) times[task] = time;
	else if (time < times[task]) times[task] = time;

	lines[task, nlines[task]++] = $0;
}


END {
	PROCINFO["sorted_in"] = "@val_num_asc";
	for (task in times) {
		n = nlines[task];
		for (i=0; i<n; ++i) {
			print(lines[task, i]);
		}
	}
}
