/*
 * $Id: zorder.h,v 1.4 2015/03/12 15:57:38 wes Exp $
 * Version: $Name: Apr24-2015 $
 * $Revision: 1.4 $
 * $Log: zorder.h,v $
 * Revision 1.4  2015/03/12 15:57:38  wes
 * Wes edits per DC's comments
 *
 * Revision 1.4  2020/02/24 13:00:00  thobson2
 * Add support for 2D Z-Order curves and for disabling AORDER
 * entirely.
 *
 * Revision 1.3  2015/02/18 21:00:56  wes
 * Wes additions after LBNL Tech Transfer clearance to release
 *
 * Revision 1.2  2015/01/30 19:39:24  wes
 * Wes cleanups and tweaks
 *
 * Revision 1.1.1.1  2013/10/14 22:43:32  wes
 * Initial entry
 *
 *
 * This code is 100% hand crafted by a human being in the USA.
 * E. Wes Bethel, LBNL
 */


/*
 * Library API for Z-Order Memory Layout (zorder-lib), Copyright (c) 
 * 2015, The Regents of the University of California, through Lawrence 
 * Berkeley National Laboratory (subject to receipt of any required 
 * approvals from the U.S. Dept. of Energy).  All rights reserved.  
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are 
 * met:
 *
 * (1) Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer.  
 * (2) Redistributions in binary form must reproduce the above copyright 
 * notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.  
 * (3) Neither the name of the University of California, Lawrence 
 * Berkeley National Laboratory, U.S. Dept. of Energy nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You are under no obligation whatsoever to provide any bug fixes, 
 * patches, or upgrades to the features, functionality or performance of 
 * the source code ("Enhancements") to anyone; however, if you choose to 
 * make your Enhancements available either publicly, or directly to 
 * Lawrence Berkeley National Laboratory, without imposing a separate 
 * written license agreement for such Enhancements, then you hereby grant 
 * the following license: a  non-exclusive, royalty-free perpetual 
 * license to install, use, modify, prepare derivative works, incorporate 
 * into other computer software, distribute, and sublicense such 
 * enhancements or derivative works thereof, in binary and source code
 * form.  
 *
 * This work is supported by the U. S. Department of Energy under contract 
 * number DE-AC03-76SF00098 between the U. S. Department of Energy and the 
 * University of California.
 */



#ifndef __zorder_h
#define __zorder_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>


/* status defines */
typedef enum
{
    ZORDER_OK = 0x0,
    ZORDER_ERROR = 0x1
} ZOStatusEnum;

/* macro options */
/* set ZORDER_INDEXING_MACROS to 1 to use macro versions of
   index computation */  
/* #define ZORDER_INDEXING_MACROS 0   */
#define ZORDER_INDEXING_MACROS 1   

#define ZORDER_ENABLE_NONE 0x0
#define ZORDER_ENABLE_ZORDER 0x1
#define ZORDER_ENABLE_AORDER 0x2

/* #define ZORDER_ENABLE (ZORDER_ENABLE_ZORDER | ZORDER_ENABLE_AORDER) */
#define ZORDER_ENABLE ZORDER_ENABLE_ZORDER

/* various tracing options */
#define ZORDER_TRACE_NONE 0 	/* use this for silent operation */
#define ZORDER_TRACE_DEBUG 0x1

#define ZORDER_TRACE ZORDER_TRACE_DEBUG
/* #define ZORDER_TRACE ZORDER_TRACE_NONE */


typedef enum
{
    ZORDER_LAYOUT = 0x1,
    AORDER_LAYOUT = 0x2
} ZOLayoutEnum;

/* 
 * 27 jun 2013 
 * a long-standing limitation of the z-order method is the requirement that the
 * grid dimensions be an even power of two in size, similar to OpenGL
 * texture size limitations.  
 *
 * the #define below will execute code paths that either implement,
 * or work around, this power-of-two size limitation. when
 * POWER_OF_TWO is set to 1, then this code will pad out the z-order
 * array size so that it is an even power of two in each
 * dimension. When set to 0, it will implement a fix to work around
 * that limitation.
 *
 * As of the time of this writing (27 jun 2013), that fix does not yet
 * exist, so don't set it 0.
 */
#define POWER_OF_TWO 1

typedef struct
{
    /* enum for the type of layout being used note: this method can be
       set (changed) later, its primary purpose is to serve as a
       conditional when doing data indexing/access */
    ZOLayoutEnum layoutMethod;
    
    /* array-order indexing variables and tables */
    /* dims of the array-order mesh. these need not be powers of two */
    size_t src_iSize, src_jSize, src_kSize;

    /* dims of the z-order mesh. these will be set to be powers of two
       during the initialization phase */ 
    size_t zo_iSize, zo_jSize, zo_kSize;

    /* 
     * 1/29/2015. if this were C++ code, the remaining fields would be
     * private class variables.
     */ 

    /* 10/14/2013. The size of the z-order array isn't exactly an
       even-power-of-two in size.   */
    size_t zo_totalSize;
    
    /*
     * when the init routine is called, we precompute some tables that
     * are used later to accelerate construction of both array- and
     * z-order indexing. The app would not normally access these
     * tables directly.
     */
    off_t *jOffsets, *kOffsets;

    off_t *zIbits, *zJbits, *zKbits;
    /* z-order indexing variables and tables */
#if 0
    int    iShift, jShift, kShift; /* 3/7/2015 - these are not
				      presently used, but will be in a
				      future version of the code  */
#endif
    
} ZOrderStruct;

#ifdef __cplusplus
extern "C" {
#endif
    
    /* initialize */
    ZOrderStruct *zoInitZOrderIndexing(size_t iSize, size_t jSize, size_t kSize, ZOLayoutEnum);

    /* get/set the indexing method */
    ZOStatusEnum zoSetLayoutMethod(ZOrderStruct *zo, ZOLayoutEnum layoutMethod);
    ZOLayoutEnum zoGetLayoutMethod(const ZOrderStruct *zo);

    /* Data conversion routines */
    ZOStatusEnum zoConvertArrayOrderToZOrder(void *src, void **dst, size_t sizeOfThing, const ZOrderStruct *zo);
    

    /* data access routines. */

    
#if ZORDER_INDEXING_MACROS
#define zoGetIndexZOrder(zo, i, j, k) (((zo)->zo_kSize > 0 ? (zo)->zKbits[(k)] : 0) | \
                                       ((zo)->zo_jSize > 0 ? (zo)->zJbits[(j)] : 0) | \
                                       ((zo)->zo_iSize > 0 ? (zo)->zIbits[(i)] : 0))
#define zoGetIndexArrayOrder(zo, i, j, k) ((i) + (zo)->jOffsets[(j)] + (zo)->kOffsets[(k)])
#define zoGetIndex(zo, i, j, k) ((zo->layoutMethod == ZORDER_LAYOUT) ? (zoGetIndexZOrder(zo, i, j, k)) : (zoGetIndexArrayOrder(zo, i, j, k)))
#else
    off_t zoGetIndex(const ZOrderStruct *zo, off_t i, off_t j, off_t k);
    off_t zoGetIndexZOrder(const ZOrderStruct *zo, off_t i, off_t j, off_t k);
    off_t zoGetIndexArrayOrder(const ZOrderStruct *zo, off_t i, off_t j, off_t k);
#endif


#ifdef __cplusplus
}
#endif

#endif
