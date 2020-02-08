#ifndef _COMMON_H_
#define _COMMON_H_


#include <math.h>


#define T(func) run_test(argc, argv, #func, func)


void
run_test(int argc, char **argv, char *test_name, void (*test)(void));



int fcmp_ok_at_loc(const char *file, int line, float range, float a,
                   const char *op, float b, const char *fmt, ...);


#define CAT(a, ...) PRIMITIVE_CAT(a, __VA_ARGS__)
#define PRIMITIVE_CAT(a, ...) a ## __VA_ARGS__
#define COMPL(b) PRIMITIVE_CAT(COMPL_, b)
#define COMPL_0 1
#define COMPL_1 0
#define BITAND(x) PRIMITIVE_CAT(BITAND_, x)
#define BITAND_0(y) 0
#define BITAND_1(y) y
#define IIF(c) PRIMITIVE_CAT(IIF_, c)
#define IIF_0(t, ...) __VA_ARGS__
#define IIF_1(t, ...) t
#define CHECK_2(x, n, ...) n
#define CHECK_3(x, y, z, ...) z
#define CHECK(...) CHECK_2(__VA_ARGS__, 0,)
#define PROBE(x) x, 1,
#define IS_PAREN(x) CHECK(IS_PAREN_PROBE x)
#define IS_PAREN_PROBE(...) PROBE(~)
#define NOT(x) CHECK(PRIMITIVE_CAT(NOT_, x))
#define NOT_0 PROBE(~)
#define BOOL(x) COMPL(NOT(x))
#define IF(c) IIF(BOOL(c))
#define EMPTY()
#define DEFER(id) id EMPTY()
#define EVAL(...)  EVAL1(EVAL1(EVAL1(__VA_ARGS__)))
#define EVAL1(...) EVAL2(EVAL2(EVAL2(__VA_ARGS__)))
#define EVAL2(...) EVAL3(EVAL3(EVAL3(__VA_ARGS__)))
#define EVAL3(...) EVAL4(EVAL4(EVAL4(__VA_ARGS__)))
#define EVAL4(...) EVAL5(EVAL5(EVAL5(__VA_ARGS__)))
#define EVAL5(...) __VA_ARGS__


#define FCMP_DEFAULT_RANGE 0.1
#define RANGE(x) ~, x
#undef FCMP
#define FCMP_DEFINED() EVAL(DEFER(CHECK_3)(FCMP, 1, 0, ))
#define FCMP_RANGE() IF(FCMP_DEFINED())(EVAL(DEFER(CHECK_2)(FCMP,)), FCMP_DEFAULT_RANGE)

#define fcmp_ok(...)     fcmp_ok_at_loc(__FILE__, __LINE__, FCMP_RANGE(), __VA_ARGS__, NULL)


#endif
