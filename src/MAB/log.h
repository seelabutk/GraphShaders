#ifndef _MAB_LOG_H_
#define _MAB_LOG_H_

#include "mp.h"

int mabLogToFile(char *filename, char *mode);

void mabLogFlush(int always_flush);

void mabLogAction(char *message, ...);

void mabLogMessage(char *name, char *message, ...);

void mabLogForward(char **info);

void mabLogContinue(const char *const info);

void mabLogEnd(char *message, ...);

void mabLogDirectly(char *line);

#define ELEVENTH_ARGUMENT(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define COUNT_ARGUMENTS(...) ELEVENTH_ARGUMENT(dummy, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define MAB_PASTE(x, y) MAB_PASTEINNER(x, y)
#define MAB_PASTEINNER(x, y) x ## y
#define mabLogMessage(...) MAB_PASTE(mabLogMessage, COUNT_ARGUMENTS(__VA_ARGS__))(__VA_ARGS__)
#define mabLogMessage0() mabLogMessage(NULL, NULL)
#define mabLogMessage1(a1) mabLogMessage(a1, NULL)
#define mabLogMessage2(a1, a2) mabLogMessage(a1, a2)
#define mabLogMessage3(a1, a2, a3) mabLogMessage(a1, a2, a3)
#define mabLogMessage4(a1, a2, a3, a4) mabLogMessage(a1, a2, a3, a4)
#define mabLogMessage5(a1, a2, a3, a4, a5) mabLogMessage(a1, a2, a3, a4, a5)
#define mabLogMessage6(a1, a2, a3, a4, a5, a6) mabLogMessage(a1, a2, a3, a4, a5, a6)
// meh.. need more

#define MAB_WRAP(...) \
	MPP_BEFORE(1, mabLogAction(__VA_ARGS__)) \
	MPP_AFTER(2, mabLogEnd(NULL))

#endif /* _MAB_LOG_H_ */
