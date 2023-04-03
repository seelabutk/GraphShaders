# vim: set ts=8 sw=8 noet syntax=make:

SHELL := bash

CSV2BIN := ./csv2bin.py


#--- SO-Answers

SRC := data
DST := data

SRC_NODE := $(SRC)/SO-Answers.node.csv
SRC_EDGE := $(SRC)/SO-Answers.edge.csv
DST_NODE_X := $(DST)/SO-Answers.node.x.f32.bin
DST_NODE_Y := $(DST)/SO-Answers.node.y.f32.bin
DST_EDGE_ELEMENT := $(DST)/SO-Answers.edge.element.2u32.bin
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
		element "$(DST_EDGE_ELEMENT)" \
		When "$(DST_EDGE_WHEN)"; \
	printf -- \
		$$'    %s\n' \
		'##'; \
	exec >&-; \
	chmod +x "$@"


SO-Answers.sh: $(DST_NODE_X)
$(DST_NODE_X): $(SRC_NODE)
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
$(DST_NODE_Y): $(SRC_NODE)
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
$(DST_EDGE_ELEMENTS): $(SRC_EDGE)
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
$(DST_EDGE_WHEN): $(SRC_EDGE)
	$(CSV2BIN) \
		--input="$<" \
		--column=when \
		--transform=int \
		--format=I \
		--output="$@" \
		##
