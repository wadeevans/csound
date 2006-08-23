/*  pvscent.c:
    Calculation of spectral centroid as Beauchamp

    (c) John ffitch, 2005
	(c) Alan OCinneide, 2005

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

#include "csdl.h"
#include "pstream.h"

typedef struct {
        OPDS    h;
        MYFLT   *ans;
        PVSDAT  *fin;
        unsigned long   lastframe;
} PVSCENT;

static int pvscentset(CSOUND *csound, PVSCENT *p)
{
     p->lastframe = 0;
     *p->ans = FL(0.0);
     if (!(p->fin->format==PVS_AMP_FREQ) || (p->fin->format==PVS_AMP_PHASE))
       return
         csound->InitError(csound,
                 Str("pvscent: format must be amp-phase or amp-freq.\n"));
     return OK;
}

static int pvscent(CSOUND *csound, PVSCENT *p)
{
    long i,N = p->fin->N;
    MYFLT c = FL(0.0);
    MYFLT d = FL(0.0);
    MYFLT j, binsize = FL(0.5)*csound->esr/(MYFLT)N;
    float *fin = (float *) p->fin->frame.auxp;
    if (p->lastframe < p->fin->framecount) {
      for (i=0,j=FL(0.5)*binsize; i<N+2; i+=2, j += binsize) {
        c += fin[i]*j;         /* This ignores phase */
        d += fin[i];
      }
      *p->ans = (d==FL(0.0) ? FL(0.0) : c/d);
      p->lastframe = p->fin->framecount;
    }
    return OK;
}

/* PVSPITCH opcode by Ala OCinneide */

typedef struct _pvspitch
{
	// OPDS data structure
	OPDS	h;

	// Output
	MYFLT	*kfreq;
	MYFLT	*kamp;

	// Inputs
	PVSDAT	*fin;
	MYFLT	*ithreshold;

	// Internal arrays
	AUXCH peakfreq;
	AUXCH inharmonic;

	unsigned long lastframe;

} pvspitch;



#define FALSE 0
#define TRUE (!FALSE)

#define RoundNum(Number)  (int)MYFLT2LRND(Number) 


#define Remainder(Numerator, Denominator)  Numerator/Denominator - (int) (Numerator/Denominator)
   


int pvspitch_init(CSOUND *csound, pvspitch *p)
{
	// Initialise frame count to zero.
	p->lastframe = 0;

	if (p->peakfreq.auxp==NULL)
	{
		csound->AuxAlloc(csound, sizeof(MYFLT)*(p->fin->N+2)/4, &p->peakfreq);
	}

	if (p->inharmonic.auxp==NULL)
	{
		csound->AuxAlloc(csound, sizeof(MYFLT)*(p->fin->N+2)/4, &p->inharmonic);
	}

	if (p->fin->format!=PVS_AMP_FREQ)
	{
		csound->InitError(csound, "PV Frames must be in AMP_FREQ format!\n");
		return NOTOK;
	}

	return OK;
}

int pvspitch_process(CSOUND *csound, pvspitch *p)
{
	// Initialised inputs
	float *Frame		= (float *) p->fin->frame.auxp;
	MYFLT *PeakFreq		= (MYFLT *) p->peakfreq.auxp;
	MYFLT *inharmonic	= (MYFLT *) p->inharmonic.auxp;
	MYFLT Threshold		= (MYFLT) *p->ithreshold;
	int fftsize			= (int) p->fin->N;
	int numBins			= fftsize/2 + 1;	
	
	MYFLT f0Cand, Frac, Freq = (MYFLT) 0;
	int i, j,  P1, P2, maxPartial;
	MYFLT lowHearThreshold	= (MYFLT) 20;
	MYFLT Amp				= FL(0.0);
	int Partial			    =  0;
	int	numPeaks			= 0;
	int maxAdj				= 3;
	int Adj					= FALSE;
	int PrevNotAdj			= FALSE;

	// Un-normalise the threshold value
	Threshold *= csound->e0dbfs;
	
	// If a new frame is ready...
	if (p->lastframe < p->fin->framecount)	
	{
		// Finds the peaks in the frame.
		for (i=1; i<(numBins-1) && numPeaks<numBins/2; i++)
		{
			// A peak is defined as being above the threshold and
			// greater than both its neighbours...
			if (Frame[2*i] > Threshold &&
			    Frame[2*i] > Frame[2*(i-1)] &&
			    Frame[2*i] > Frame[2*(i+1)])
			{
				PeakFreq[numPeaks]=Frame[2*i+1];
				numPeaks++;
				i++; // Impossible to have two peaks in a row, skip over the next.
			}
			
			Amp += Frame[2*i];
		}
		
		Amp += Frame[0];
		Amp += Frame[2*numBins];
		Amp *= 0.5;
		
		if (numPeaks==0)
		{
			// If no peaks found return 0.
			Partial = 0;
		}
		else
		{
			// Threshold of hearing is 20 Hz, so no need to look beyond there for the fundamental.
			maxPartial = (int) (PeakFreq[0]/lowHearThreshold);

			// Calculates the inharmonicity for each fundamental candidate
			for (i=0; i<maxPartial; i++)
			{
				inharmonic[i] = (MYFLT) 0;
				f0Cand = PeakFreq[0]/(i+1);

				for (j=1; j<numPeaks; j++)
				{
					Frac = Remainder(PeakFreq[j], f0Cand);
					Frac = (Frac > 0.5 ? 1 - Frac : Frac);

					Frac /= PeakFreq[j];
					
					inharmonic[i]+=Frac;
				}
				
				// Test for the adjacency of partials...
				for (j=0; j<numPeaks-1; j++)
				{
					P1 = RoundNum(PeakFreq[j]/f0Cand);
					P2 = RoundNum(PeakFreq[j+1]/f0Cand);
								
					if (P2-P1<maxAdj && P2-P1!=0)
					{
						Adj = TRUE;
						break;
					}
					else Adj = FALSE;
				}

				// Search for the fundamental with the least inharmonicity
				if (i==0 || (i>0 && inharmonic[i]<inharmonic[Partial-1]) || (i>0 && PrevNotAdj && Adj))
				{
					// The best candidate so far...
					if (Adj)
					{
						Partial = i+1;
						PrevNotAdj = FALSE;
					}
					else if (i==0)
					{
						Partial = i+1;
						PrevNotAdj = TRUE;
					}
					else PrevNotAdj = TRUE;

				}
			}
		}
		
		// Output the appropriate frequency values.		
		if (Partial!=0)
		{
			f0Cand = PeakFreq[0]/Partial;
			// Average frequency between partials
			for (i=0; i<numPeaks; i++)
			{
				Freq += PeakFreq[i] / RoundNum(PeakFreq[i]/f0Cand);
			}	
			Freq /= numPeaks;		
			*p->kfreq = Freq;
		}
		else
		{
			*p->kfreq = (MYFLT) 0;
		}
		
		*p->kamp  = Amp;
		
		// Update the frame count
		p->lastframe = p->fin->framecount;
	}

	return OK;
}















#define S(x)    sizeof(x)

static OENTRY localops[] = {
   {"pvscent", S(PVSCENT), 3, "k", "f", (SUBR)pvscentset, (SUBR)pvscent, NULL},
   {"pvspitch", S(pvspitch), 3, "kk", "fk", (SUBR)pvspitch_init, (SUBR)pvspitch_process, NULL}
};

int pvscent_init_(CSOUND *csound)
{
    return csound->AppendOpcodes(csound, &(localops[0]),
                                 (int) (sizeof(localops) / sizeof(OENTRY)));
}

