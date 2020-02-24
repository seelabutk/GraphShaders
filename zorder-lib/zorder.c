/*
 * $Id: zorder.c,v 1.4 2015/03/12 15:57:35 wes Exp $
 * Version: $Name: Apr24-2015 $
 * $Revision: 1.4 $
 * $Log: zorder.c,v $
 * Revision 1.4  2015/03/12 15:57:35  wes
 * Wes edits per DC's comments
 *
 * Revision 1.3  2015/02/18 21:00:55  wes
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



#include <zorder.h>

/*
 * misc utility routines for internal use only:
 int zoNearestPowerOfTwo(int n)
 int zoRoundUpPowerOfTwo (off_t n);
 int zoNumBits(size_t t);
    
 *
 */
 
/*
 * ----------------------------------------------------
 * @Name zoNearestPowerOfTwo
 @pstart
 int zoNearestPowerOfTwo (int n)
 @pend

 @astart
 int n - an integer value.
 @aend

 @dstart

 This routine computes the integer that is the closest power of two to
 the input integer "n". The algorithm works only for non-negative
 input.

 This routine will come in handy if you have to scale an image so it's
 size is an even power of two.

 @dend
 * ----------------------------------------------------
 */
static int
zoNearestPowerOfTwo (int n)
{
    int nbits, j;

    j = n;
    for (nbits = 0; j > 0;)
    {
	j = j >> 1;
	nbits++;
    }

    if (nbits != 0)
	nbits--;

    if ((1<<nbits) == n)
	return(n);
    else
    {
	int a, b, c;

	a = (1 << (nbits + 1));	/* 2^j which is greater than n */
	b = a - n;
	c = a - (a >> 1);
	c = c>>1;
	if (b > c)
	    return(1 << nbits);
	else
	    return(1 << (nbits + 1));
    }
}

/* 
 * zoRoundUpPowerOfTwo(off_t n)
 * 
 * return an int that is an even power of two, where 2**i >= n
 */
static int
zoRoundUpPowerOfTwo(off_t n)
{
    float f;
    int r,t;

    f = (float)n;
    f = log2f(f);
    f = ceilf(f);
    t = (int)f;

    r = 1;
    while (t > 0)
    {
	r = r << 1;
	t--;
    }
    
    return r;
}

/*
 * zoNumBits(size_t t)
 *
 * Given an input valut, t, return an int that says how many binary
 * digits of precision are needed for that t. For example, if t=8,
 * then zoNumBits() would return 4; if t=7, it would return 3, etc.
 *
 * (it does not count the number of on bits in t)
 */
static size_t
zoNumBits(size_t t)
{
    size_t n;

    for (n=0; t>0; n++)
	t = t >> 1;

    return n;
}


/*
 * zoInitOrderIndexing
 * Call this routine to perform initialization before doing any data
 * conversion or data access.
 *
 * Input:
 * iSize, jSize, kSize: these are the dimensions of the input
 * mesh that you'll be working with in subsequent routines.
 *
 * LayoutEnum - either ZORDER_LAYOUT or AORDER_LAYOUT, which will set
 * the layout method. Note that this enum is used only by the routine
 * getData(), which will internally look at the enum to figure out
 * whether to call the array-order or z-order memory access
 * routines. See the getData() routine for more information.
 *
 * input a pointer to an uninitialized ZOrderStruct. This
 * initialization routine will fill it in with a  bunch of stuff that
 * will be used by later conversion and indexing routines.
 *
 * Returns: a valid ZOrderStruct if successful, NULL otherwise.
*/
ZOrderStruct *
zoInitZOrderIndexing(size_t iSize,
		     size_t jSize,
		     size_t kSize,
		     ZOLayoutEnum layoutMethod)
{
    ZOrderStruct *zo=NULL;
    size_t i;
    size_t t;
    int n_iBits, n_jBits, n_kBits;

    if ((zo = (ZOrderStruct *)calloc(1, sizeof(ZOrderStruct))) == NULL)
	return NULL;

    /* set the layout method. note: this method can be set (changed)
       later, its primary purpose is to serve as a conditional when
       doing data indexing/access */
    zo->layoutMethod = layoutMethod;

    /* build the array order indexing tables first. */
    zo->src_iSize = iSize;
    zo->src_jSize = jSize;
    zo->src_kSize = kSize;

    /* start with the offsets for the j-direction */
    if ((zo->jOffsets = (off_t *)malloc(sizeof(size_t)*jSize)) == NULL)
    {
	free((void *)zo);
    	return NULL;
    }

    zo->jOffsets[0]=0;
    t=iSize;
    for (i=1;i<jSize;i++, t+=iSize)
	zo->jOffsets[i]=t;

    /* then do the offsets for the k-direction */
    if ((zo->kOffsets = (off_t *)malloc(sizeof(size_t)*kSize)) == NULL)
    {
	free((void *)zo->jOffsets);
	free((void *)zo);
	return NULL;
    }
    
    zo->kOffsets[0]=0;
    t=iSize*jSize;
    for (i=1;i<kSize;i++, t+=(iSize*jSize))
	zo->kOffsets[i]=t;

    /* now, build the z-order indexing tables */
    
#if POWER_OF_TWO 
    /* ok, so here's the real "issue" with z-order indexing: it
       basically requires that the input data be an even power of two
       in size.  since this is rarely the case (unless you are OpenGL
       and consuming textures), what we do here is to figure out what
       the next biggest power of two size is for each of the input
       dimensions.  */
    zo->zo_iSize = zoRoundUpPowerOfTwo(iSize);
    zo->zo_jSize = zoRoundUpPowerOfTwo(jSize);
    zo->zo_kSize = zoRoundUpPowerOfTwo(kSize);
#else
    bad-code-path;
#endif
    
#if (ZORDER_TRACE & ZORDER_TRACE_DEBUG)
    fprintf(stderr,"zoInitZOrderIndexing: size of z-order array is %ld %ld %ld\n", zo->zo_iSize, zo->zo_jSize, zo->zo_kSize);
#endif
    
    /* next, build the zorder bit masks for each of i, j, k */
    /* malloc space for bitmasks. */
    zo->zIbits = (off_t *)calloc(zo->zo_iSize, sizeof(off_t));
    zo->zJbits = (off_t *)calloc(zo->zo_jSize, sizeof(off_t));
    zo->zKbits = (off_t *)calloc(zo->zo_kSize, sizeof(off_t));

    /*
     * note/limitation: the maximum size of the z-order index is
     * determined by the size of an off_t. On 32-bit machines, it
     * is a maximum of 30 bits, or 10 bits for each of i,j,k (max
     * array dimensions 1024^3). On 64-bit machines, it is a
     * maximum of 21 bits (max array dimension of 2^21 for any of
     * (i,j,k). 
     */

    /* compute the number bits we need to process */
    n_iBits = zoNumBits(zo->zo_iSize);
    n_jBits = zoNumBits(zo->zo_jSize);
    n_kBits = zoNumBits(zo->zo_kSize);

#if (ZORDER_TRACE & ZORDER_TRACE_DEBUG)
    fprintf(stderr, "iBits, jBits, kBits = %d %d %d \n", n_iBits, n_jBits, n_kBits);
#endif

    /* now, build the bitmasks */
    {
	size_t i, j;
	off_t s, d, mask;

	/* do the k bits first */
	for (i=0;i<zo->src_kSize;i++)
	{
	    d = 0;
	    mask = 0x4; 
	    s = i;
	    for (j=0;j<n_kBits;j++)
	    {
		if (s & 0x1)
		    d |= mask;
		s = s >> 1;
		mask = mask << 3;
	    }
	    zo->zKbits[i] = d;
	}

	/* then the j bits */
	for (i=0;i<zo->src_jSize;i++)
	{
	    d = 0;
	    mask = 0x2;
	    s = i;
	    for (j=0;j<n_jBits;j++)
	    {
		if (s & 0x1)
		    d |= mask;
		s = s >> 1;
		mask = mask << 3;
	    }
	    zo->zJbits[i] = d;
	}

	/* then the i bits */
	for (i=0;i<zo->src_iSize;i++)
	{
	    d = 0;
	    mask = 0x1;
	    s = i;
	    for (j=0;j<n_iBits;j++)
	    {
		if (s & 0x1)
		    d |= mask;
		s = s >> 1;
		mask = mask << 3;
	    }
	    zo->zIbits[i] = d;
	}
    }

    /* what is the total memory footprint, in terms of number of
       elements, needed to hold the z-order layout for this iSize,
       jSize, kSize? */
    zo->zo_totalSize = zoGetIndexZOrder(zo, zo->src_iSize-1, zo->src_jSize-1, zo->src_kSize-1)+1;

    {
	/* 1/29/2015 todo: do some work to figure out which bit
	   ordering is most 
	   compact in terms of memory use. When the iSize, jSize,
	   kSize are equal, the order doesn't matter. However, if one
	   of them is larger than the others, then we want the largest
	   one to be the low-order bits of the z-order bitmask. Not
	   doing this computation can result in a lot of extra memory
	   being used unnecessarily.

	   the work that will happen:
	   1. Figure out which of the three is the largest
	   2. set it to be the low order bits
	   3. the order of the other two doesn't matter, because the
	   overall size of the memory footprint will be determined by
	   the largest of the three
	   4. use this information to set the way the 3 bitfields are
	   combined during indexing, and redo the zo_totalSize
	   computation, which, above, makes assumptions about the how
	   the bitfields are combined.
	*/
	
    }
    
    return zo;
}

ZOStatusEnum
zoConvertArrayOrderToZOrder(void *src,
			    void **dst,
			    size_t sizeOfThing,
			    const ZOrderStruct *zo)
/* rcReorderToZorder3D(void **t, */
/* 		    int sizeOfThing, */
/* 		    int xSize, */
/* 		    int ySize, */
/* 		    int zSize) */
{
    void *s, *d;
    size_t i, j, k;
    off_t sIndx, dIndx;

    /* create a buffer for the new z-order array */
    {
	size_t nelem;
	nelem = zo->zo_totalSize;
    
#if (ZORDER_TRACE & ZORDER_TRACE_DEBUG)
	{
	    size_t origSize =  zo->src_iSize * zo->src_jSize * zo->src_kSize;
	    double inflationFactor = (double)nelem / (double)origSize;
	    fprintf(stderr,"zoConvertArrayOrderToZOrder():\nOrig array elements:\t%ld\nZOrder Array elements:\t%ld\nInflation factor:\t%g\n",origSize,nelem,inflationFactor);
	}
#endif

	/*	nelem = zo->zo_iSize * zo->zo_jSize * zo->zo_kSize; */
	/* need to insert assertion/check for bogus values */
	if ((d = calloc(nelem, sizeOfThing)) == NULL)
	    return ZORDER_ERROR;
    }
    
    /* now, do the reordering. */
    
    s = src;
    sIndx = 0;

    for (k=0;k<zo->src_kSize;k++)
    {
	for (j=0;j<zo->src_jSize;j++)
	{
	    for (i=0;i<zo->src_iSize;i++)
	    {
		/*
		 * foreach element of the source array, compute its
		 * z-order index, then assign from src[i,j,k] to
		 * dst[zorder_index] 
		*/  
		dIndx = zoGetIndexZOrder(zo, i, j, k);
		memcpy(d+dIndx*sizeOfThing, s+sIndx*sizeOfThing, sizeOfThing);
		sIndx++;
	    }
	}
    }

    *dst = d;

    return ZORDER_OK; 			/* need to assign return value */
}



/*
 * Call this routine to obtain the index of some (i,j,k)
 * element in an array, which may be laid out using either a-order or
 * z-order methods. Internal to this routine, we look at the
 * layoutEnum field of the input ZOrderStruct, and then invoke the
 * specific routine for either a-order or z-order.
 *
 * Input:
 * zo - a pointer to a ZOrderStruct (initialized with
 * zoInitOrderIndexing)
 * i,j,k - off_t's, these are i,j,k index locations
 *
 * Output/returns:
 * -1 on error, or the 1-D index of some (i,j,k) location in a 3d
 * array. 
 *
 * Notes: use a value of zero for k if computing the index into a 2D
 * array, and values of zero for j and k if computing the index into a
 * 1D array.
 *
 */
#if !(ZORDER_INDEXING_MACROS)
off_t
zoGetIndex(const ZOrderStruct *zo,
	   off_t i,
	   off_t j,
	   off_t k)
{
    off_t rstat = -1;
    
    if (zo->layoutMethod == ZORDER_LAYOUT)
	rstat = zoGetIndexZOrder(zo, i, j, k);
    else if (zo->layoutMethod == AORDER_LAYOUT)
	rstat = zoGetIndexArrayOrder(zo, i, j, k);

    return rstat;
}
#endif /* !(ZORDER_INDEXING_MACROS) */


/*
 * zoGetIndexZOrder
 *
 * Call this routine to obtain the z-order index of some (i,j,k)
 * element in an array.
 *
 * Input:
 * zo - a pointer to a ZOrderStruct (initialized with
 * zoInitOrderIndexing)
 * i,j,k - off_t's, these are i,j,k index locations
 *
 * Output/returns:
 * -1 on error, or the 1-D index along a z-order curve of some (i,j,k)
 * location in a 3d array.
 *
 * Notes: use a value of zero for k if computing the index into a 2D
 * array, and values of zero for j and k if computing the index into a
 * 1D array.
 *
*/
#if !(ZORDER_INDEXING_MACROS)
off_t
zoGetIndexZOrder(const ZOrderStruct *zo,
		 off_t i,
		 off_t j,
		 off_t k)
{
    off_t rval=0;

    /* sanity checks first? */
    rval = zo->zKbits[k] | zo->zJbits[j] | zo->zIbits[i];
    /*    rval = zo->zIbits[i] | zo->zJbits[j] | zo->zKbits[k]; */

#if 0
    {
	size_t lim = zo->zo_totalSize;
	if (rval >= lim)
	    fprintf(stderr,"zoGetIndexOrder error: input (%lld, %lld, %lld) returned z-order index, %lld, is outside of the malloc'd memory region, %ld\n", i, j, k, rval, lim);
    }
#endif
    return rval;
}
#endif

/*
 * zoGetIndexArrayOrder
 *
 * Call this routine to obtain the array-order index of some (i,j,k)
 * element in an array.
 *
 * Input:
 * zo - a pointer to a ZOrderStruct (initialized with
 * zoInitOrderIndexing)
 * i,j,k - off_t's, these are i,j,k index locations
 *
 * Output/returns:
 * -1 on error, or the 1-D index of some (i,j,k) location in a 3d array.
 *
 * Notes: use a value of zero for k if computing the index into a 2D
 * array, and values of zero for j and k if computing the index into a
 * 1D array.
 *
*/
#if !(ZORDER_INDEXING_MACROS)
off_t
zoGetIndexArrayOrder(const ZOrderStruct *zo,
		     off_t i,
		     off_t j,
		     off_t k)
{
    off_t rval=0;

    /* sanity checks first? */
    rval = i + zo->jOffsets[j] + zo->kOffsets[k];
    return rval;
}
#endif



ZOStatusEnum
zoSetLayoutMethod(ZOrderStruct *zo,
		  ZOLayoutEnum layoutMethod)
{
    if (layoutMethod != ZORDER_LAYOUT || layoutMethod != AORDER_LAYOUT)
	return ZORDER_ERROR;
    
    zo->layoutMethod = layoutMethod;
    return ZORDER_OK;
}


ZOLayoutEnum
zoGetLayoutMethod(const ZOrderStruct *zo)
{
    return zo->layoutMethod;
}


/* eof */
