/*
 * $Id: zo-sanity3.c,v 1.4 2015/03/12 15:57:34 wes Exp $
 * Version: $Name: Apr24-2015 $
 * $Revision: 1.4 $
 * $Log: zo-sanity3.c,v $
 * Revision 1.4  2015/03/12 15:57:34  wes
 * Wes edits per DC's comments
 *
 * Revision 1.3  2015/02/18 21:00:51  wes
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

/*
 * zo-sanity3 test program
 *
 * The purpose of this program is to accept as input user parameters
 * specificying the size of a 3d array, then for each (i,j,k)
 * location, print out the corresponding z-order index.
 *
 * The need for this program came about in studying the amount of
 * extra memory required by the z-order in-memory representation.
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
    off_t i, j, k;

    parseArgs(ac, av, &iSize, &jSize, &kSize);

    fprintf(stderr,"%s user-requested array size: %d %d %d \n", av[0], iSize, jSize, kSize);

    zo = zoInitZOrderIndexing(iSize, jSize, kSize, ZORDER_LAYOUT);

    fprintf(stderr,"i\tj\tk\tz-indx\n");
    for (k=0;k<kSize;k++)
	for (j=0;j<jSize;j++)
	    for (i=0;i<iSize;i++)
		fprintf(stderr,"%lld\t%lld\t%lld\t%lld\n",i,j,k,zoGetIndexZOrder(zo, i, j, k));

    return 0;
}
      
