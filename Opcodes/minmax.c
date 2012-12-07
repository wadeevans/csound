/*
    minmax.c

    A Csound opcode library implementing ugens
    for finding minimum and maximum values.

    Anthony Kozar
    April 5, 2005

    Jan 26, 2006:  Added non-accumulating and abs value variations.
    Mar 06, 2006:  Converted to Csound 5 API.

    Copyright (C) 2005-6  Anthony M. Kozar Jr.

    This file is part of Csound.

    The Csound Library is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Csound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

// #include "csdl.h"
#include "csoundCore.h"
#include "interlocks.h"
#include <math.h>

typedef struct {
    OPDS    h;
    MYFLT   *accum, *ain;
} MINMAXACCUM;

typedef struct {
    OPDS    h;
    MYFLT   *xout, *xin1, *xin2toN[VARGMAX];
} MINMAX;

/* Which implementation is faster ?? */
static int MaxAccumulate(CSOUND *csound, MINMAXACCUM *p)
{
    MYFLT   cur;
    int     n, nsmps = CS_KSMPS;
    MYFLT   *out = p->accum;
    MYFLT   *in = p->ain;

    for (n=0; n<nsmps; n++) {
      cur = in[n];
      if (UNLIKELY(cur > out[n]))
        out[n] = cur;
    }

    return OK;
}

static int MinAccumulate(CSOUND *csound, MINMAXACCUM *p)
{
    MYFLT   cur;
    int     n, nsmps = CS_KSMPS;
    MYFLT   *out = p->accum;
    MYFLT   *in = p->ain;

    for (n=0; n<nsmps; n++) {
      cur = in[n];
      if (UNLIKELY(cur < out[n]))
        out[n] = cur;
    }

    return OK;
}

/* Absolute value versions of the above */
static int MaxAbsAccumulate(CSOUND *csound, MINMAXACCUM *p)
{
    int     n, nsmps = CS_KSMPS;
    MYFLT   *out = p->accum;
    MYFLT   *in = p->ain;
    MYFLT   inabs;

    for (n=0; n<nsmps; n++) {
      inabs = FABS(in[n]);
      if (UNLIKELY(inabs > out[n]))
        out[n] = inabs;
    }

    return OK;
}

static int MinAbsAccumulate(CSOUND *csound, MINMAXACCUM *p)
{
    int     n, nsmps = CS_KSMPS;
    MYFLT   *out = p->accum;
    MYFLT   *in = p->ain;
    MYFLT   inabs;

    for (n=0; n<nsmps; n++) {
      inabs = FABS(in[n]);
      if (UNLIKELY(inabs < out[n]))
        out[n] = inabs;
    }

    return OK;
}

/* Multiple input min and max opcodes */
static int Max_arate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     n, nsmps = CS_KSMPS;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   max, temp;

    for (n=0; n<nsmps; n++) {
      max = in1[n];
      for (i = 0; i < nargs; ++i) {
        temp = in2[i][n];
        if (UNLIKELY(temp > max))
          max = temp;
      }
      out[n] = max;
    }

    return OK;
}

static int Min_arate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     n, nsmps = CS_KSMPS;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   min, temp;

    for (n=0; n<nsmps; n++) {
      min = in1[n];
      for (i = 0; i < nargs; ++i) {
        temp = in2[i][n];
        if (UNLIKELY(temp < min))
          min = temp;
      }
      out[n] = min;
    }

    return OK;
}

/* Absolute value versions of multiple input opcodes */
static int MaxAbs_arate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     n, nsmps = CS_KSMPS;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   max, temp;

    for (n=0; n<nsmps; n++) {
      max = FABS(in1[n]);
      for (i = 0; i < nargs; ++i) {
        temp = FABS(in2[i][n]);
        if (UNLIKELY(temp > max))
          max = temp;
      }
      out[n] = max;
    }

    return OK;
}

static int MinAbs_arate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     n, nsmps = CS_KSMPS;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   min, temp;

    for (n=0; n<nsmps; n++) {
      min = FABS(in1[n]);
      for (i = 0; i < nargs; ++i) {
        temp = FABS(in2[i][n]);
        if (UNLIKELY(temp < min))
          min = temp;
      }
      out[n] = min;
    }

    return OK;
}

/* k-rate multiple input min and max opcodes */
static int Max_krate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   max, temp;

    max = *in1;
    for (i = 0; i < nargs; ++i) {
      temp = in2[i][0];
      if (UNLIKELY(temp > max))
        max = temp;
    }
    *out = max;

    return OK;
}

static int Min_krate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   min, temp;

    min = *in1;
    for (i = 0; i < nargs; ++i) {
      temp = in2[i][0];
      if (UNLIKELY(temp < min))
        min = temp;
    }
    *out = min;

    return OK;
}

/* Absolute value versions of k-rate opcodes */
static int MaxAbs_krate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   max, temp;

    max = FABS(*in1);
    for (i = 0; i < nargs; ++i) {
      temp = FABS(in2[i][0]);
      if (UNLIKELY(temp > max))
        max = temp;
    }
    *out = max;

    return OK;
}

static int MinAbs_krate(CSOUND *csound, MINMAX *p)
{
    int     i;
    int     nargs = ((int) p->INOCOUNT) - 1;
    MYFLT   *out = p->xout;
    MYFLT   *in1 = p->xin1;
    MYFLT   **in2 = p->xin2toN;
    MYFLT   min, temp;

    min = FABS(*in1);
    for (i = 0; i < nargs; ++i) {
      temp = FABS(in2[i][0]);
      if (UNLIKELY(temp < min))
        min = temp;
    }
    *out = min;

    return OK;
}

/* code for linking dynamic libraries under Csound 5 */

#define S(x)    sizeof(x)

static OENTRY minmax_localops[] = {
    {"maxaccum", S(MINMAXACCUM), 4, "", "aa", NULL, NULL, (SUBR) MaxAccumulate},
    {"minaccum", S(MINMAXACCUM), 4, "", "aa", NULL, NULL, (SUBR) MinAccumulate},
    {"maxabsaccum", S(MINMAXACCUM), 4, "", "aa", NULL, NULL,
     (SUBR) MaxAbsAccumulate},
    {"minabsaccum", S(MINMAXACCUM), 4, "", "aa", NULL, NULL,
     (SUBR) MinAbsAccumulate},
    {"max", 0xffff},
    {"min", 0xffff},
    {"maxabs", 0xffff},
    {"minabs", 0xffff},
    {"max.a", S(MINMAX), 4, "a", "ay", NULL, NULL, (SUBR) Max_arate},
    {"min.a", S(MINMAX), 4, "a", "ay", NULL, NULL, (SUBR) Min_arate},
    {"maxabs.a", S(MINMAX), 4, "a", "ay", NULL, NULL, (SUBR) MaxAbs_arate},
    {"minabs.a", S(MINMAX), 4, "a", "ay", NULL, NULL, (SUBR) MinAbs_arate},
    {"max.k", S(MINMAX), 2, "k", "kz", NULL, (SUBR) Max_krate, NULL},
    {"min.k", S(MINMAX), 2, "k", "kz", NULL, (SUBR) Min_krate, NULL},
    {"maxabs.k", S(MINMAX), 2, "k", "kz", NULL, (SUBR) MaxAbs_krate, NULL},
    {"minabs.k", S(MINMAX), 2, "k", "kz", NULL, (SUBR) MinAbs_krate, NULL}
};

LINKAGE_BUILTIN(minmax_localops)
