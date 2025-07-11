/*********************************************************************
This file is part of the argtable2 library.
Copyright (C) 1998-2001,2003-2011 Stewart Heitmann
sheitmann@users.sourceforge.net

The argtable2 library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

This software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.
**********************************************************************/

/* config.h must be included before anything else */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* #ifdef HAVE_STDLIB_H */
#include <stdlib.h>
/* #endif */

#include "argtable2.h"
#include <limits.h>

#undef EOVERFLOW
/* local error codes */
enum {EMINCOUNT=1,EMAXCOUNT,EBADINT,EOVERFLOW};

static void resetfn(struct arg_int *parent)
    {
    /*printf("%s:resetfn(%p)\n",__FILE__,parent);*/
    parent->count=0;
    }

/* strtol0x() is like strtol() except that the numeric string is    */
/* expected to be prefixed by "0X" where X is a user supplied char. */
/* The string may optionally be prefixed by white space and + or -  */
/* as in +0X123 or -0X123.                                          */
/* Once the prefix has been scanned, the remainder of the numeric   */
/* string is converted using strtol() with the given base.          */
/* eg: to parse hex str="-0X12324", specify X='X' and base=16.      */
/* eg: to parse oct str="+0o12324", specify X='O' and base=8.       */
/* eg: to parse bin str="-0B01010", specify X='B' and base=2.       */
/* Failure of conversion is indicated by result where *endptr==str. */
static long int strtol0X(const char* str, const char **endptr, char X, int base)
    {
    long int val;               /* stores result */
    int s=1;                    /* sign is +1 or -1 */
    const char *ptr=str;        /* ptr to current position in str */

    /* skip leading whitespace */
    while (isspace(*ptr))
        ptr++;
    /* printf("1) %s\n",ptr); */

    /* scan optional sign character */
    switch (*ptr)
        {
        case '+':
            ptr++;
            s=1;
            break;
        case '-':
            ptr++;
            s=-1;
            break;
        default:
            s=1;
            break;    
        }
    /* printf("2) %s\n",ptr); */

    /* '0X' prefix */
    if ((*ptr++)!='0')
        { 
        /* printf("failed to detect '0'\n"); */
        *endptr=str;
        return 0;
        }
   /* printf("3) %s\n",ptr); */
   if (toupper(*ptr++)!=toupper(X))
        {
        /* printf("failed to detect '%c'\n",X); */
        *endptr=str;
        return 0;
        }
    /* printf("4) %s\n",ptr); */

    /* attempt conversion on remainder of string using strtol() */
    val = strtol(ptr,(char**)endptr,base);
    if (*endptr==ptr)
        {
        /* conversion failed */
        *endptr=str;
        return 0;
        }

    /* success */
    return s*val;
    }


/* Returns 1 if str matches suffix (case insensitive).    */
/* Str may contain trailing whitespace, but nothing else. */
static int detectsuffix(const char *str, const char *suffix)
    {
    /* scan pairwise through strings until mismatch detected */
    while( toupper(*str) == toupper(*suffix) )
        {
        /* printf("'%c' '%c'\n", *str, *suffix); */

        /* return 1 (success) if match persists until the string terminator */
        if (*str=='\0')
           return 1; 

        /* next chars */
        str++;
        suffix++;
        }
    /* printf("'%c' '%c' mismatch\n", *str, *suffix); */

    /* return 0 (fail) if the matching did not consume the entire suffix */
    if (*suffix!=0)
        return 0;   /* failed to consume entire suffix */

    /* skip any remaining whitespace in str */
    while (isspace(*str))
        str++;

    /* return 1 (success) if we have reached end of str else return 0 (fail) */
    return (*str=='\0') ? 1 : 0;
    }


static int scanfn(struct arg_int *parent, const char *argval)
    {
    int errorcode = 0;

    if (parent->count == parent->hdr.maxcount)
        {
        /* maximum number of arguments exceeded */
        errorcode = EMAXCOUNT;
        }
    else if (!argval)
        {
        /* a valid argument with no argument value was given. */
        /* This happens when an optional argument value was invoked. */
        /* leave parent arguiment value unaltered but still count the argument. */
        parent->count++;
        }
    else
        {
        long int val;
        const char *end;

        /* attempt to extract hex integer (eg: +0x123) from argval into val conversion */
        val = strtol0X(argval, &end, 'X', 16);
        if (end==argval)
            {
            /* hex failed, attempt octal conversion (eg +0o123) */
            val = strtol0X(argval, &end, 'O', 8);
            if (end==argval)
                {
                /* octal failed, attempt binary conversion (eg +0B101) */
                val = strtol0X(argval, &end, 'B', 2);
                if (end==argval)
                    {
                    /* binary failed, attempt decimal conversion with no prefix (eg 1234) */
                    val = strtol(argval, (char**)&end, 10);
                    if (end==argval)
                        {
                        /* all supported number formats failed */
                        return EBADINT;
                        }
                    }
                }
            }

        /* Safety check for integer overflow. WARNING: this check    */
        /* achieves nothing on machines where size(int)==size(long). */
        if ( val>INT_MAX || val<INT_MIN )
            errorcode = EOVERFLOW;

        /* Detect any suffixes (KB,MB,GB) and multiply argument value appropriately. */
        /* We need to be mindful of integer overflows when using such big numbers.   */
        if (detectsuffix(end,"KB"))             /* kilobytes */
            {
            if ( val>(INT_MAX/1024) || val<(INT_MIN/1024) )
                errorcode = EOVERFLOW;          /* Overflow would occur if we proceed */
            else
                val*=1024;                      /* 1KB = 1024 */
            }
        else if (detectsuffix(end,"MB"))        /* megabytes */
            {
            if ( val>(INT_MAX/1048576) || val<(INT_MIN/1048576) )
                errorcode = EOVERFLOW;          /* Overflow would occur if we proceed */
            else
                val*=1048576;                   /* 1MB = 1024*1024 */
            }
        else if (detectsuffix(end,"GB"))        /* gigabytes */
            {
            if ( val>(INT_MAX/1073741824) || val<(INT_MIN/1073741824) )
                errorcode = EOVERFLOW;          /* Overflow would occur if we proceed */
            else
                val*=1073741824;                /* 1GB = 1024*1024*1024 */
            }
        else if (!detectsuffix(end,""))  
            errorcode = EBADINT;                /* invalid suffix detected */

        /* if success then store result in parent->ival[] array */
        if (errorcode==0)
            parent->ival[parent->count++] = val;
        }

    /* printf("%s:scanfn(%p,%p) returns %d\n",__FILE__,parent,argval,errorcode); */
    return errorcode;
    }

static int checkfn(struct arg_int *parent)
    {
    int errorcode = (parent->count < parent->hdr.mincount) ? EMINCOUNT : 0;
    /*printf("%s:checkfn(%p) returns %d\n",__FILE__,parent,errorcode);*/
    return errorcode;
    }

static void errorfn(struct arg_int *parent, FILE *fp, int errorcode, const char *argval, const char *progname)
    {
    const char *shortopts = parent->hdr.shortopts;
    const char *longopts  = parent->hdr.longopts;
    const char *datatype  = parent->hdr.datatype;

    /* make argval NULL safe */
    argval = argval ? argval : "";

    fprintf(fp,"%s: ",progname);
    switch(errorcode)
        {
        case EMINCOUNT:
            fputs("missing option ",fp);
            arg_print_option(fp,shortopts,longopts,datatype,"\n");
            break;

        case EMAXCOUNT:
            fputs("excess option ",fp);
            arg_print_option(fp,shortopts,longopts,argval,"\n");
            break;

        case EBADINT:
            fprintf(fp,"invalid argument \"%s\" to option ",argval);
            arg_print_option(fp,shortopts,longopts,datatype,"\n");
            break;

        case EOVERFLOW:
            fputs("integer overflow at option ",fp);
            arg_print_option(fp,shortopts,longopts,datatype," ");
            fprintf(fp,"(%s is too large)\n",argval);
            break;
        }
    }


struct arg_int* arg_int0(const char* shortopts,
                         const char* longopts,
                         const char *datatype,
                         const char *glossary)
    {
    return arg_intn(shortopts,longopts,datatype,0,1,glossary);
    }

struct arg_int* arg_int1(const char* shortopts,
                         const char* longopts,
                         const char *datatype,
                         const char *glossary)
    {
    return arg_intn(shortopts,longopts,datatype,1,1,glossary);
    }


struct arg_int* arg_intn(const char* shortopts,
                         const char* longopts,
                         const char *datatype,
                         int mincount,
                         int maxcount,
                         const char *glossary)
    {
    size_t nbytes;
    struct arg_int *result;

	/* foolproof things by ensuring maxcount is not less than mincount */
	maxcount = (maxcount<mincount) ? mincount : maxcount;

    nbytes = sizeof(struct arg_int)     /* storage for struct arg_int */
           + maxcount * sizeof(int);    /* storage for ival[maxcount] array */

    result = (struct arg_int*)malloc(nbytes);
    if (result)
        {
        /* init the arg_hdr struct */
        result->hdr.flag      = ARG_HASVALUE;
        result->hdr.shortopts = shortopts;
        result->hdr.longopts  = longopts;
        result->hdr.datatype  = datatype ? datatype : "<int>";
        result->hdr.glossary  = glossary;
        result->hdr.mincount  = mincount;
        result->hdr.maxcount  = maxcount;
        result->hdr.parent    = result;
        result->hdr.resetfn   = (arg_resetfn*)resetfn;
        result->hdr.scanfn    = (arg_scanfn*)scanfn;
        result->hdr.checkfn   = (arg_checkfn*)checkfn;
        result->hdr.errorfn   = (arg_errorfn*)errorfn;

        /* store the ival[maxcount] array immediately after the arg_int struct */
        result->ival  = (int*)(result+1);
        result->count = 0;
        }
    /*printf("arg_intn() returns %p\n",result);*/
    return result;
    }
