# vim: set ts=8 sw=8 noet syntax=make:

SHELL := bash

CSV2BIN := ./csv2bin.py


#--- SO-Answers

SRC := data/SO-Answers-edgetime
DST := graph/SO-Answers

DST_NODE_X := $(DST)/SO-Answers.node.x.f32.bin
DST_NODE_Y := $(DST)/SO-Answers.node.y.f32.bin
DST_EDGE_ELEMENTS := $(DST)/SO-Answers.edge.elements.2u32.bin
DST_EDGE_WHEN := $(DST)/SO-Answers.edge.when.u32.bin

.PHONY: SO-Answers

SO-Answers: | $(DST)
$(DST):
	mkdir -p "$@"

SO-Answers: | SO-Answers.sh
SO-Answers.sh:
	exec >"$@"; \
	printf -- \
		$$'#!/usr/bin/env bash\n'; \
	printf -- \
		$$'exec \\\n'; \
	printf -- \
		$$'    %s \\\n' \
		'"$$@"'; \
	printf -- \
		$$'    -f %q %q \\\n' \
		X "$(DST_NODE_X)" \
		Y "$(DST_NODE_Y)" \
		element "$(DST_EDGE_ELEMENTS)" \
		When "$(DST_EDGE_WHEN)"; \
	printf -- \
		$$'    %s\n' \
		'##'; \
	exec >&-; \
	chmod +x "$@"


SO-Answers.sh: $(DST_NODE_X)
$(DST_NODE_X): $(SRC)/nodes.csv
	$(CSV2BIN) \
		--input="$<" \
		--column=x \
		--transform=float \
		--format=f \
		--output="$@" \
		--domain=-506573.0,500330.0 \
		--transform=normalize \
		##

SO-Answers.sh: $(DST_NODE_Y)
$(DST_NODE_Y): $(SRC)/nodes.csv
	$(CSV2BIN) \
		--input="$<" \
		--column=y \
		--transform=float \
		--format=f \
		--output="$@" \
		--domain=-492756.0,494892.0 \
		--transform=normalize \
		##

SO-Answers.sh: $(DST_EDGE_ELEMENTS)
$(DST_EDGE_ELEMENTS): SO-Answers.edges.sort_by_when.csv
	$(CSV2BIN) \
		--input="$<" \
		--column=src \
		--column=tgt \
		--transform=int \
		--transform=-1 \
		--format=II \
		--output="$@" \
		##

SO-Answers.sh: $(DST_EDGE_WHEN)
$(DST_EDGE_WHEN): $(SRC)/edges.csv
	$(CSV2BIN) \
		--input="$<" \
		--column=when \
		--transform=int \
		--format=I \
		--output="$@" \
		##





#--- JS-Deps


# SRC := data/JS-Deps-filtered
# DST := graph/JS-Deps

# DST_NODE_X := $(DST)/JS-Deps.node.x.f32.bin
# DST_NODE_Y := $(DST)/JS-Deps.node.y.f32.bin
# DST_EDGE_ELEMENTS := $(DST)/JS-Deps.edge.elements.2u32.bin
# DST_EDGE_WHEN := $(DST)/JS-Deps.edge.when.u32.bin

# .PHONY: JS-Deps

# JS-Deps: | $(DST)
# $(DST):
# 	mkdir -p "$@"

# JS-Deps: | JS-Deps.sh
# JS-Deps.sh:
# 	exec >"$@"; \
# 	printf -- \
# 		$$'#!/usr/bin/env bash\n'; \
# 	printf -- \
# 		$$'exec \\\n'; \
# 	printf -- \
# 		$$'    %s \\\n' \
# 		'"$$@"'; \
# 	printf -- \
# 		$$'    -f %q %q \\\n' \
# 		X "$(DST_NODE_X)" \
# 		Y "$(DST_NODE_Y)" \
# 		element "$(DST_EDGE_ELEMENTS)" \
# 		When "$(DST_EDGE_WHEN)"; \
# 	printf -- \
# 		$$'    %s\n' \
# 		'##'; \
# 	exec >&-; \
# 	chmod +x "$@"


# JS-Deps.sh: $(DST_NODE_X)
# $(DST_NODE_X): $(SRC)/nodes.csv
# 	$(CSV2BIN) \
# 		--input="$<" \
# 		--column=x \
# 		--transform=float \
# 		--format=f \
# 		--output="$@" \
# 		--domain=-1666.310422874415,4145.933813716894 \
# 		--transform=normalize \
# 		##

# JS-Deps.sh: $(DST_NODE_Y)
# $(DST_NODE_Y): $(SRC)/nodes.csv
# 	$(CSV2BIN) \
# 		--input="$<" \
# 		--column=y \
# 		--transform=float \
# 		--format=f \
# 		--output="$@" \
# 		--domain=-2541.5300207136556,2247.2635371167767 \
# 		--transform=normalize \
# 		##

# JS-Deps.sh: $(DST_EDGE_ELEMENTS)
# $(DST_EDGE_ELEMENTS): $(SRC)/edges.csv
# 	$(CSV2BIN) \
# 		--input="$<" \
# 		--column=src \
# 		--column=tgt \
# 		--transform=int \
# 		--transform=-1 \
# 		--format=II \
# 		--output="$@" \
# 		##

# JS-Deps.sh: $(DST_EDGE_WHEN)
# $(DST_EDGE_WHEN): $(SRC)/edges.csv
# 	$(CSV2BIN) \
# 		--input="$<" \
# 		--column=when \
# 		--transform=int \
# 		--format=I \
# 		--output="$@" \
# 		##







