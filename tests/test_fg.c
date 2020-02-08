#include <tap.h>
#include <assert.h>
#include "common.h"
#include <fg.h>


void
_test_fg__common(float x0, float y0, float x1, float y1, float d, int num) {
	int rs[32];
	int cs[32];
	int n;
	
	void emit(int r, int c) {
		assert(n < sizeof(rs)/sizeof(*rs));
		rs[n] = r;
		cs[n] = c;
		n++;
	}
	
	n = 0;
	raytrace(x0, y0, x1, y1, d, emit);

	if (num >= 0) cmp_ok(n, "==", num, "should have correct number of tiles");
	cmp_ok(rs[0], "==", (int)x0, "first x should be x0");
	cmp_ok(cs[0], "==", (int)y0, "first y should be y0");
	cmp_ok(rs[n-1], "==", (int)x1, "last x should be x1");
	cmp_ok(cs[n-1], "==", (int)y1, "last y should be y1");
	
	for (int i=1; i<n; ++i) {
		int row_changed =
		         ((rs[i-1] == rs[i] - 1) || (rs[i-1] == rs[i] + 1))
		      && (cs[i-1] == cs[i]);
		int col_changed = 
		         ((cs[i-1] == cs[i] - 1) || (cs[i-1] == cs[i] + 1))
		      && (rs[i-1] == rs[i]);

		if ((col_changed && !row_changed) || (!col_changed && row_changed)) {
			continue;
		}
		
		if (col_changed && row_changed)
			fail("column and row both changed");
		
		if (!col_changed && !row_changed)
			fail("neither column nor row changed");
		
		for (int j=0; j<n; ++j) {
			diag(j==i ? "%d: [%d, %d]\n" : "%d: %d, %d", j, rs[j], cs[j]);
		}
		
		goto found_disconnect;
	}
	pass("all parts of the path are connected");
found_disconnect:;

	char filename[32];
	snprintf(filename, sizeof(filename), "temp_%s", __func__);
	FILE *f = fopen(filename, "w");
	assert(f);
	fprintf(f, "start: (%f %f) (%f %f) %f\n", x0, y0, x1, y1, d);
	fprintf(f, "trace:\n");
	for (int i=0; i<n; ++i) {
		fprintf(f, "  (%d %d)\n", rs[i], cs[i]);
	}
	fclose(f);
}


void
test_fg__raytrace_1(void) {
	_test_fg__common(1.0, 1.0, 8.0, 8.0, 1.0, 15);
}


void
test_fg__raytrace_2(void) {
	_test_fg__common(1.0, 1.0, 8.0, 7.0, 1.0, 14);
}


void
test_fg__raytrace_3(void) {
	_test_fg__common(1.2, 1.3, 7.7, 9.1, 1.0, 15);
}


void
test_fg__raytrace_4(void) {
	_test_fg__common(8, 8, 1, 1, 1.0, 15);
}


void
test_fg__raytrace_5(void) {
	_test_fg__common(8.1, 1.4, 1.6, 8.1, 1.0, 15);
}


void
test_fg__les_miserables(void) {
	struct graph graph;
	
	int rc = load("data/les-miserables/nodes.csv", "data/les-miserables/edges.csv", &graph);
	cmp_ok(rc, "==", 0, "load should succeed");
	
	cmp_ok(graph.ncount, "==", 77, "should have read all the nodes");
	cmp_ok(graph.ecount, "==", 254, "should have read all the edges");
	
	fcmp_ok(graph.nx[0], "==", 303.5739416973415, "should have correct first x value");
	fcmp_ok(graph.ny[0], "==", 361.25374922736694, "should have correct first y value");
	is(graph.nid[0], "Myriel", "should have correct first id");
	
	cmp_ok(graph.es[0], "==", 1, "should have correct first source");
	cmp_ok(graph.et[0], "==", 0, "should have correct first target");
	
	fcmp_ok(graph.nx[graph.ncount-1], "==", 633.10784, "should have correct last x value");
	fcmp_ok(graph.ny[graph.ncount-1], "==", 387.04956, "should have correct last y value");
	is(graph.nid[graph.ncount-1], "Mme.Hucheloup", "should have correct last id");
	
	cmp_ok(graph.es[graph.ecount-1], "==", 76, "should have correct last source");
	cmp_ok(graph.et[graph.ecount-1], "==", 58, "should have correct last target");
	
	rescale(&graph, 0.0, 0.0, 10.0, 10.0);
	
	fcmp_ok(graph.nx[0], "==", 1.08942, "should have correct rescaled first x value");
	fcmp_ok(graph.ny[0], "==", 7.37831, "should have correct rescaled first y value");
	
	fcmp_ok(graph.nx[graph.ncount-1], "==", 9.0668, "should have correct rescaled second x value");
	fcmp_ok(graph.ny[graph.ncount-1], "==", 8.00665, "should have correct rescaled second y value");
}


int
main(int argc, char **argv) {
	plan(NO_PLAN);
	T(test_fg__raytrace_1);
	T(test_fg__raytrace_2);
	T(test_fg__raytrace_3);
	T(test_fg__raytrace_4);
	T(test_fg__raytrace_5);
	T(test_fg__les_miserables);
	done_testing();
}
