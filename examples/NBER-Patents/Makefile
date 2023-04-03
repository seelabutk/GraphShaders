# vim: set ts=8 sw=8 noet syntax=make:

SHELL := bash

CSV2BIN := ./csv2bin.py


#--- NBER-Patents

SRC := data
DST := data

SRC_NODE := $(SRC)/NBER-Patents.node.csv
SRC_EDGE := $(SRC)/NBER-Patents.edge.csv

DST_NODE_X := $(DST)/NBER-Patents.node.x.f32.bin
DST_NODE_Y := $(DST)/NBER-Patents.node.y.f32.bin
DST_NODE_APPYEAR := $(DST)/NBER-Patents.node.appyear.u32.bin
DST_NODE_GOTYEAR := $(DST)/NBER-Patents.node.gotyear.u32.bin
DST_NODE_CAT1 := $(DST)/NBER-Patents.node.cat1.u32.bin
DST_NODE_CAT2 := $(DST)/NBER-Patents.node.cat2.u32.bin

DST_EDGE_ELEMENT := $(DST)/NBER-Patents.edge.element.2u32.bin

.PHONY: NBER-Patents

NBER-Patents: | NBER-Patents.sh
NBER-Patents.sh:
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
		GotYear "$(DST_NODE_GOTYEAR)" \
		AppYear "$(DST_NODE_APPYEAR)" \
		Cat1 "$(DST_NODE_CAT1)" \
		Cat2 "$(DST_NODE_CAT2)" \
		element "$(DST_EDGE_ELEMENTS)"; \
	printf -- \
		$$'    %s\n' \
		'##'; \
	exec >&-; \
	chmod +x "$@"


NBER-Patents.sh: $(DST_NODE_X)
$(DST_NODE_X): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=x \
		--transform=float \
		--format=f \
		--output="$@" \
		--domain=-101164.03,106701.17 \
		--transform=normalize \
		##

NBER-Patents.sh: $(DST_NODE_Y)
$(DST_NODE_Y): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=y \
		--transform=float \
		--format=f \
		--output="$@" \
		--domain=-128590.445,95094.445 \
		--transform=normalize \
		##

NBER-Patents.sh: $(DST_NODE_APPYEAR)
$(DST_NODE_APPYEAR): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=appyear \
		--transform=int \
		--format=I \
		--output="$@" \
		--domain=1901,2006 \
		##

NBER-Patents.sh: $(DST_NODE_GOTYEAR)
$(DST_NODE_GOTYEAR): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=gyear \
		--transform=int \
		--format=I \
		--output="$@" \
		##

NBER-Patents.sh: $(DST_NODE_CAT1)
$(DST_NODE_CAT1): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=cat_ccl \
		--transform=float \
		--transform=int \
		--format=I \
		--output="$@" \
		--domain=1,6 \
		##

NBER-Patents.sh: $(DST_NODE_CAT2)
$(DST_NODE_CAT2): $(SRC_NODE)
	$(CSV2BIN) \
		--input="$<" \
		--column=nclass \
		--transform=int \
		--format=I \
		--output="$@" \
		--domain=1,800 \
		##

NBER-Patents.sh: $(DST_EDGE_ELEMENTS)
$(DST_EDGE_ELEMENTS): $(SRC_EDGE)
	$(CSV2BIN) \
		--input="$<" \
		--column=citing \
		--column=cited \
		--transform=int \
		--transform=-1 \
		--format=II \
		--output="$@" \
		--domain=0,3149406 \
		##
