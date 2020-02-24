/*
 * $Id: zo-sanity1.c,v 1.4 2015/03/12 15:57:34 wes Exp $
 * Version: $Name: Apr24-2015 $
 * $Revision: 1.4 $
 * $Log: zo-sanity1.c,v $
 * Revision 1.4  2015/03/12 15:57:34  wes
 * Wes edits per DC's comments
 *
 * Revision 1.3  2015/02/18 21:00:51  wes
 * Wes additions after LBNL Tech Transfer clearance to release
 *
 * Revision 1.2  2015/01/30 19:39:23  wes
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

/*
*
* this sanity check program only seems to verify that the memcpy stuff
* is working property during the array conversion.
*
*/

/*
 * zo-sanity1 test program
 *
 * The purpose of this program is to test out the process of
 * converting from array order to z-order layout. The means for doing
 * this test is to first create some in-memory data in array order
 * layout containing known values, convert it to z-order layout, then
 * to compare the contents by iterating over each (i,j,k) location and
 * comparing their values.
*/
#include <stdio.h>
#include <string.h>
#include <zorder.h>

void
usage(char *progName)
{
    fprintf(stderr,"usage: %s -i iSize -j jSize -k kSize \n", progName);
}

void
parseArgs(int ac,
	  char *av[],
	  int *iSize,
	  int *jSize,
	  int *kSize)
{
    int i=1;
    ac--;

    *iSize=*jSize=*kSize= -1;
    
    while (ac > 0)
    {
	if (strcmp(av[i],"-i") == 0)
	{
	    i++;
	    ac--;
	    *iSize = atoi(av[i]);
	}
	else if (strcmp(av[i],"-j") == 0)
	{
	    i++;
	    ac--;
	    *jSize = atoi(av[i]);
	}
	else if (strcmp(av[i],"-k") == 0)
	{
	    i++;
	    ac--;
	    *kSize = atoi(av[i]);
	}
	else
	{
	    usage(av[0]);
	    exit(-1);
	}
	ac--;
	i++;
    }

    if ((*iSize == -1) || (*jSize == -1) || (*kSize == -1))
    {
	usage(av[0]);
	exit(-1);
    }
}

int
main (int ac,
      char *av[])
{
    int iSize, jSize, kSize;
    ZOrderStruct *zo;

    parseArgs(ac, av, &iSize, &jSize, &kSize);

    fprintf(stderr,"%s user-requested array size: %d %d %d \n", av[0], iSize, jSize, kSize);

    zo = zoInitZOrderIndexing(iSize, jSize, kSize, ZORDER_LAYOUT);

    fprintf(stderr," all done initializing. \n");

    {
	/* first, try doing a shuffle with ints */
	int *a=NULL, *z=NULL;
	int i, j, k;
	off_t ai_indx, zo_indx;
	a = (int *)malloc(sizeof(int)*iSize*jSize*kSize);

	/* 
	   load up the src array with known values: each a[i] contains
	   its array-order index.
	*/
	ai_indx = 0;
	for (k=0;k<kSize;k++)
	    for (j=0;j<jSize;j++)
		for (i=0;i<iSize;i++,ai_indx++)
		    a[ai_indx]=ai_indx;

	/* create a new array, which is the z-ordered version of the
	   original */   
	zoConvertArrayOrderToZOrder((void *)a, (void **)&z, sizeof(int), zo);

	/* now, iterate over each entry of the index-order array, and
	   grab the z-order entry, and compare them. All this does is
	   verify that the memcpy stuff works when doing the
	   conversion from index to array order: it doesn't really
	   verify that the stuff in the z-order array is really a
	   z-ordered version of the array order index since we're
	   using the same routine to compute a z-order index here as
	   is used inside the zoConvertArrayOrderToZorder() call. */

	ai_indx = 0;
	for (k=0;k<kSize;k++)
	    for (j=0;j<jSize;j++)
		for (i=0;i<iSize;i++,ai_indx++)
		{
		    /* the following line is a call directly to the
		       method that grabs the z-order index  */
		    /* zo_indx = zoGetIndexZOrder(zo, i, j, k); */

		    /* invoke the method for getting either an a-order
		       or z-order index */
		    zo_indx = zoGetIndex(zo, i, j, k);
		    if (a[ai_indx] != z[zo_indx])
		    {
			fprintf(stderr," sanity check fails on ints. \n");
			exit(-1);
		    }
		}
	   
	fprintf(stderr," sanity check passes for ints. \n");
	free ((void *)a);
	free ((void *)z);
    }
    
    {
	/* now try doing a shuffle with doubles */
	double *a=NULL, *z=NULL;
	int i, j, k;
	off_t ai_indx, zo_indx;
	a = (double *)malloc(sizeof(double)*iSize*jSize*kSize);

	/* 
	   load up the src array with known values: each a[i] contains
	   its array-order index.
	*/
	ai_indx = 0;
	for (k=0;k<kSize;k++)
	    for (j=0;j<jSize;j++)
		for (i=0;i<iSize;i++,ai_indx++)
		    a[ai_indx]=ai_indx;

	/* create a new array, which is the z-ordered version of the
	   original */   
	zoConvertArrayOrderToZOrder((void *)a, (void **)&z, sizeof(double), zo);

	/* now, iterate over each entry of the index-order array, and
	   grab the z-order entry, and compare them. All this does is
	   verify that the memcpy stuff works when doing the
	   conversion from index to array order: it doesn't really
	   verify that the stuff in the z-order array is really a
	   z-ordered version of the array order index since we're
	   using the same routine to compute a z-order index here as
	   is used inside the zoConvertArrayOrderToZorder() call. */

	ai_indx = 0;
	for (k=0;k<kSize;k++)
	    for (j=0;j<jSize;j++)
		for (i=0;i<iSize;i++,ai_indx++)
		{
		    zo_indx = zoGetIndex(zo, i, j, k);
		    /*	zo_indx = zoGetIndexZOrder(zo, i, j, k); */
		    if (a[ai_indx] != z[zo_indx])
		    {
			fprintf(stderr," sanity check fails on doubles. \n");
			exit(-1);
		    }
		}
	   
	fprintf(stderr," sanity check passes for doubles. \n");
	free ((void *)a);
	free ((void *)z);
    }

    return 0;
}
      
