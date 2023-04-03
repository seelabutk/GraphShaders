#pragma gs attribute(float X[N])
#pragma gs attribute(float Y[N])
#pragma gs attribute(uint AppYear[N])
#pragma gs attribute(uint GotYear[N])
#pragma gs attribute(uint Cat1[N])
#pragma gs attribute(uint Cat2[N])

#pragma gs shader(positional)
void main() {
    float x = X[gs_NodeIndex];
    float y = Y[gs_NodeIndex];
    gs_NodePosition = vec3(x, y, 0);
}

#pragma gs shader(relational)
void main() {
}

#pragma gs shader(appearance)
void main() {
    #pragma gs define(SRC_CAT1 2)
    #pragma gs define(TGT_CAT1 4)
    #pragma gs define(LO 1980)
    #pragma gs define(HI 1989)

    #pragma gs scratch(uint Seen[E])
    bool first = 0 == atomicAdd(Seen[gs_EdgeIndex], 1);

    bool d = false;

    #pragma gs scratch(atomic_uint count)
    if (first) atomicCounterAdd(count, 1);

    if (Cat1[gs_SourceIndex] != SRC_CAT1) {
        d = true;
        #pragma gs scratch(atomic_uint wrong_source_cat)
        if (first) atomicCounterAdd(wrong_source_cat, 1);
    }

    if (Cat1[gs_TargetIndex] != TGT_CAT1) {
        d = true;
        #pragma gs scratch(atomic_uint wrong_target_cat)
        if (first) atomicCounterAdd(wrong_target_cat, 1);
    }

    if (AppYear[gs_SourceIndex] < LO) {
        d = true;
        #pragma gs scratch(atomic_uint app_year_too_lo)
        if (first) atomicCounterAdd(app_year_too_lo, 1);
    }

    if (AppYear[gs_SourceIndex] > HI) {
        d = true;
        #pragma gs scratch(atomic_uint app_year_too_hi)
        if (first) atomicCounterAdd(app_year_too_hi, 1);
    }

    if (d) {
        #pragma gs scratch(atomic_uint discarded)
        if (first) atomicCounterAdd(discarded, 1);
        discard;
    }

    #pragma gs scratch(atomic_uint kept)
    if (first) atomicCounterAdd(kept, 1);

    uint gotyear = GotYear[gs_SourceIndex];
    uint appyear = AppYear[gs_SourceIndex];
    uint lag = gotyear - appyear;

    #pragma gs define(CUTOFF1 2)
    #pragma gs define(CUTOFF2 4)

    gs_FragColor = vec4(0.1);
    gs_FragColor.r = int(lag < CUTOFF1);
    gs_FragColor.g = int(CUTOFF1 <= lag && lag < CUTOFF2);
    gs_FragColor.b = int(CUTOFF2 <= lag);
}
