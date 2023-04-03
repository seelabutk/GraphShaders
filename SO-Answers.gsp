#pragma gs attribute(float X[N])
#pragma gs attribute(float Y[N])
#pragma gs attribute(uint When[E])

#define AUG_01_2008 1217563200
#define MAR_07_2016 1457326800

#define JAN_01_2011 1293858000
#define JAN_01_2012 1325394000
#define JAN_01_2013 1357016400
#define JAN_01_2014 1388552400
#define JAN_01_2015 1420088400
#define JAN_01_2016 1451624400

#pragma gs define(LAYOUT 1)

#pragma gs shader(positional)
void main() {
    float x = X[gs_NodeIndex];
    float y = Y[gs_NodeIndex];

    gs_NodePosition = vec3(x, y, 0.);
}


#pragma gs shader(relational)
void main() {
    const uint when = When[gs_EdgeIndex];

    #pragma gs scratch(uint FirstContribution[N])

    atomicCompSwap(FirstContribution[gs_SourceIndex], 0, when);
    atomicMin(FirstContribution[gs_SourceIndex], when);

    atomicCompSwap(FirstContribution[gs_TargetIndex], 0, when);
    atomicMin(FirstContribution[gs_TargetIndex], when);

    #pragma gs scratch(uint LastContribution[N])

    atomicCompSwap(LastContribution[gs_SourceIndex], 0, when);
    atomicMax(LastContribution[gs_SourceIndex], when);

    atomicCompSwap(LastContribution[gs_TargetIndex], 0, when);
    atomicMax(LastContribution[gs_TargetIndex], when);

    #pragma gs scratch(uint CommentsReceived[N])
    #pragma gs scratch(atomic_uint MaxCommentsReceived)
    uint cr = 1 + atomicAdd(CommentsReceived[gs_TargetIndex], 1);
    atomicCounterMax(MaxCommentsReceived, cr);

    #pragma gs scratch(uint CommentsPosted[N])
    #pragma gs scratch(atomic_uint MaxCommentsPosted)
    uint cp = 1 + atomicAdd(CommentsPosted[gs_TargetIndex], 1);
    atomicCounterMax(MaxCommentsPosted, cp);

    if (LAYOUT == 2) {
        gs_SourcePosition.x = float(FirstContribution[gs_SourceIndex] - AUG_01_2008) / float(MAR_07_2016 - AUG_01_2008);
        gs_SourcePosition.y = 1. - float(LastContribution[gs_SourceIndex] - AUG_01_2008) / float(MAR_07_2016 - AUG_01_2008);

        gs_TargetPosition.x = float(FirstContribution[gs_TargetIndex] - AUG_01_2008) / float(MAR_07_2016 - AUG_01_2008);
        gs_TargetPosition.y = 1. - float(LastContribution[gs_TargetIndex] - AUG_01_2008) / float(MAR_07_2016 - AUG_01_2008);
    }
}

#pragma gs shader(appearance)
void main() {
    bool b1 = CommentsPosted[gs_SourceIndex] >= CommentsReceived[gs_SourceIndex] / 32;
    
    const uint sourceSpan = LastContribution[gs_SourceIndex] - FirstContribution[gs_SourceIndex];
    const uint targetSpan = LastContribution[gs_TargetIndex] - FirstContribution[gs_TargetIndex];
    bool b2 = targetSpan >= sourceSpan;

    gs_FragColor = vec4(0.1);
    gs_FragColor.r = float(b1 || b2);
    gs_FragColor.g = float(b1 && b2);
}
