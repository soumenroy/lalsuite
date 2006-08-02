/********************************************************************************************/
/*      zellepolka - the pulsar coincidence analysis code for einstein at home postprocess  */
/*                                                                                          */
/*      Xavier Siemens,  Bruce Allen,  Bernd Machenschalk,  Yousuke Itoh,  Holger Pletsch   */
/*                                                                                          */
/*                                                                                          */
/*                                  UWM - April  2005                                       */
/*                             Based on uberpolka written by                                */
/*                        Xavier Siemens, Bruce Allen, Bernd Machenschalk                   */
/********************************************************************************************/

/*! 
   @file
   @brief the pulsar coincidence analysis code for einstein at home post-process --- counting
    number of events in cells construcetd in parameters space.

<li>Inputs and outputs
<ul>   
<li> This code takes one single file generated from the EaH zipped result files by a python code combiner_v2.py in 
lalapps/src/pulsar/FDS_isolated.
<li> This code outputs one output file whose name is specified by the user, five files with default names, and 
some information on stderr.
<ol>
<li> The one output file contains all the information (f,a,d,f1dot,ncandidate,sig) in all the cells. This can be huge.
<li> One file outputs time variation of 2F of some significant outliers.
<li> One file outputs cell information of some significant outliers
<li> One file outputs time variation of 2F of some coincident outliers.
<li> One file outputs cell information of some coincident outliers
<li> One file outputs the cell info of the maximum coincident event over each Frequency cell but all over the sky.
<li> Outputs summary table on stderr. This table shows how many cells have how many counts. 
     (e.g. 2 cells have 10 coincidences.)
</ol>
</ul>

<li>Algorithm
<ol>
<li> First construct a grid in four dimensional parameters space (frequency, right ascension, declination and f1dot).
   We take an adaptive grid in the sky location according to the metric used in the search
   (the actual implementation uses a Gaussian declination-model, with maximum around the equator),
   a uniform grid in the frequency and spin-down.
   We can change the grid spacing of the each parameter, and shift the grid as a whole for 
  each parameter.  <br> 
<li>  Then we count how many candidate events are in each cell.
  Even if a file has more than one event in a cell, we say we have one event from that file, 
  and take the largest F statistic event among the events in the cell from that file.
  Therefore, the greatest number counts in a cell is the number of data-stretches from
  which the result files have originated.
</ol>



  @author Xavier Siemens,  Bruce Allen,  Bernd Machenschalk,  Yousuke Itoh , Holger Pletsch
  $Id$ 
*/



/* ----------------------------------------------------------------------------- */
/* defines */
#ifndef FALSE
#define FALSE (1==0)
#endif
#ifndef TRUE
#define TRUE  (1==1)
#endif

#define DONE_MARKER "%DONE\n"
/* maximum depth of a linked structure. */
#define LINKEDSTR_MAX_DEPTH 1024 



/* ----------------------------------------------------------------------------- */
/* file includes */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "getopt.h"
#include <math.h>


#include <unistd.h>



/*
   To use unzip, you need to have unzip-5.5x from, say,  the InfoZip webpage, 
   and readzipfile_util.h and .c. from yousuke.
   ( not needed any more, will be done by combiner_v2.py ) 
#define USE_UNZIP  
*/




#ifdef USE_UNZIP
#include "unzip.h"
#include "readzipfile_util.h"
#endif

#ifdef HAVE_GLOB_H
#include <glob.h>
#endif


#include <lal/LALDatatypes.h>
#include <lal/LALMalloc.h>
#include <lal/LALConstants.h>
#include <lal/LALStatusMacros.h>
#include <lal/ConfigFile.h>
#include <lal/UserInput.h>

#include <lalapps.h>


/* this is defined in C99 and *should* be in math.h.  Long term
   protect this with a HAVE_FINITE */
#ifdef _MSC_VER
#include <float.h>
#define finite _finite
#else
int finite(double);
#endif





/* ----------------------------------------------------------------------------- */
/* some error codes and messages */
#define POLKAC_ENULL            1
#define POLKAC_ENONULL          2
#define POLKAC_ESYS             3
#define POLKAC_EINVALIDFSTATS   4
#define POLKAC_EMEM             5
#define POLKAC_ENORMAL          6
#define POLKAC_EUNZIP           7
#define POLKAC_EGLOB            8

#define POLKAC_MSGENULL         "Arguments contained an unexpected null pointer"
#define POLKAC_MSGENONULL       "Input pointer was not NULL"
#define POLKAC_MSGESYS          "System call failed (probably file IO"
#define POLKAC_MSGEINVALIDFSTATS "Invalid Fstats file"
#define POLKAC_MSGEMEM          "Sorry, ran out of memory... bye."
#define POLKAC_MSGEUNZIP          "Cannot use unzip."
#define POLKAC_MSGEGLOB          "Cannot use glob."


#define POLKA_EXIT_OK 0
#define POLKA_EXIT_ERR     31
#define POLKA_EXIT_READCND  32
#define POLKA_EXIT_FCTEST   33
#define POLKA_EXIT_OUTFAIL  34


/* ----------------------------------------------------------------------------- */
/* structures */
/*!
Configuration variables have three categories. 

Cell parameters: Define dimensions of cells and center of the cells.
@param Deltaf     REAL8 Size of coincidence window in Hz (= Size of cell in frequency)
@param DeltaAlpha REAL8 Size of coincidence window in radians (at the equator delta=0)
@param DeltaDelta REAL8 Size of coincidence window in radians (at the equator delta=0)
@param DeltaF1dot REAL8 Size of coincidence window of spindown d/dt f (= Size of cell in f1dot)
@param Shiftf     REAL8 Parallel shift of frequency of cell 
@param ShiftAlpha REAL8 Parallel shift of Alpha of cell
@param ShiftDelta REAL8 Parallel shift of Delta of cell 
@param ShiftF1dot REAL8 Parallel shift of F1dot of cell
@param Kappa      REAL8 Tuning parameter for declination window

Input: Control the way to read a file or files in a directory
@param *FstatsFile CHAR Names of Fstat files to be read in.
@param *InputDir   CHAR Directory name of input files 
@param *BaseName   CHAR Base name of input files 
@param **Filelist  CHAR Array of filenames to load Fstats file from 
@param NFiles;     UINT4 Number of input files read
@param TwoFthr;    REAL8 Threshold for 2F values

Output: Control the way to return results
@param OutputFile  CHAR Names of output file
@param Nthr        INT4 Show exective results of cells with numbers of coincidence above Nthr.
@param Sthr        REAL4 Show exective results of cells with significance above Sthr.
@param AutoOut     BOOLEAN If set, output the info of the most significant and the most coincident event.
*/

typedef struct PolkaConfigVarsTag 
{
  REAL8 TwoFthr;     /*  Threshold for TwoF values */
  REAL8 Deltaf;      /*  Size of coincidence window in Hz */
  REAL8 DeltaF1dot;  /*  Size of coincidence window of spindown */
  REAL8 DeltaAlpha;  /*  Size of coincidence window in radians (at equator) */
  REAL8 DeltaDelta;  /*  Size of coincidence window in radians (at equator) */
  REAL8 Kappa;       /*  Tuning parameter for declination window */
  REAL8 Shiftf;      /*  Parallel shift of frequency of cell */
  REAL8 ShiftAlpha;  /*  Parallel shift of Alpha of cell */
  REAL8 ShiftDelta;  /*  Parallel shift of Delta of cell */
  REAL8 ShiftF1dot;  /*  Parallel shift of F1dot spindown of cell */
  
  REAL8 fMIN;
  REAL8 F1dotMIN;
  REAL8 AlphaMIN;
  REAL8 DeltaMIN;
  INT4 FileIDMIN;
  
  CHAR *FstatsFile;  /*  Names of Fstat files to be read in */
  CHAR *OutputFile;  /*  Names of output file */
  CHAR *InputDir;    /*  Directory name of input files */
  CHAR *BaseName;    /*  Base name of input files */
  CHAR **Filelist;   /*  Array of filenames to load Fstats file from */
  UINT4 NFiles;      /*  Number of input files read */
  INT4 Nthr;         /*  Show exective results of cells with numbers of coincidence above Nthr. */
  REAL4 Sthr;        /*  Show exective results of cells with significance above Sthr. */
  BOOLEAN AutoOut;
  BOOLEAN UseUnzip;
  
} PolkaConfigVars;


/* This structure contains the indices corresponding to the 
coarse frequency and sky bins */
/*!
CandidateList

@param iCand         Candidate id: unique within this program.  
@param f             Frequency of the candidate 
@param Alpha         right ascension of the candidate
@param Delta         declination  of the candidate 
@param F1dot         spindown (d/dt f) of the candidate
@param TwoF          2F of this candidate event
@param FileID        File ID to specify from which file the candidate under consideration originaly comes. 
@param iFreq         Frequency index of this candidate event
@param iDelta        Declination index. This can be negative. 
@param iAlpha        Right ascension index of this candidate event
@param iF1dot        Spindwon index of this candidate event
*/

typedef struct CandidateListTag
{
  REAL8 f;           /*  Frequency of the candidate */
  REAL8 Alpha;       /*  right ascension of the candidate */
  REAL8 Delta;       /*  declination  of the candidate */
  REAL8 F1dot;       /*  spindown (d/dt f) of the candidate */
  REAL8 TwoF;        /*  Maximum value of F for the cluster */
  UINT4 iCand;       /*  Candidate id: unique with in this program.  */
  INT2 FileID;       /*  File ID to specify from which file the candidate under consideration originaly comes. */
  INT2 iFreq;        /*  Frequency index */
  INT2 iDelta;       /*  Declination index. This can be negative. */
  INT2 iAlpha;       /*  Right ascension index */
  INT2 iF1dot;       /*  Spindown index */
 
} CandidateList;     /*   Fstat lines */ 


/*!
Liked list containing one INT4 data..

@param data INT4
@param next int4_linked_list*
*/
struct int4_linked_list {
  INT4 data;
  struct int4_linked_list *next;
}; 


/*!
Structure containg data in cells

@param Freq          REAL8 Frequency index of the cell 
@param Alpha         REAL8 Right ascension index of the cell
@param Delta         REAL8 Declination index of the cell
@param F1dot         REAL8 Spindown index of the cell 
@param iFreq         INT4 Frequency index of this candidate event
@param iDelta        INT4 Declination index. This can be negative. 
@param iAlpha        INT4 Right ascension index of this candidate event
@param iF1dot        INT4 Spindown index of this candidate event
@param significance  REAL8 minus log of joint false alarm of the candidates in this cell
@param nCand;        INT4 number of the events in this cell
@param CandID        int4_linked_list* linked structure that has candidate id-s of the candidates in this cell
*/
typedef struct CellDataTag
{
  REAL4 Freq;          /*  Frequency index of the cell */
  REAL4 Alpha;         /*  Right ascension index of the cell */
  REAL4 Delta;         /*  Declination index of the cell */
  REAL4 F1dot;          /*  Spindown index of the cell */
  REAL4 significance;  /*  minus log of joint false alarm of the candidates in this cell. */
  UINT4 nCand;          /*  number of the events in this cell. */
  INT2 iFreq;          /*  Frequency index of this candidate event */
  INT2 iDelta;         /*  Declination index of this candidate event */
  INT2 iAlpha;         /*  Right ascension index of this candidate event */
  INT2 iF1dot;         /*  Spindown index of this candidate event */
 
  struct int4_linked_list *CandID;  /* linked structure that has candidate id-s of the candidates in this cell. */
} CellData;


/* ----------------------------------------------------------------------------- */
/* Function declarelations */
void ReadCommandLineArgs( LALStatus *, INT4 argc, CHAR *argv[], PolkaConfigVars *CLA ); 
void GetFilesListInThisDir(LALStatus *, const CHAR *directory, const CHAR *basename, CHAR ***filelist, UINT4 *nfiles );
void ReadCandidateFiles( LALStatus *, CandidateList **Clist, PolkaConfigVars *CLA, INT4 *datalen );
void ReadOneCandidateFile( LALStatus *, CandidateList **CList, const CHAR *fname, PolkaConfigVars *CLA, INT4 *datalen, const REAL8 myFthr );
/*void ReadOneCandidateFile( LALStatus *, CandidateList **CList, const CHAR *fname, INT4 *datalen, INT4 *candilenFthr, const REAL8 myFthr ); */
void ReadOneCandidateFileV2( LALStatus *lalStatus, CandidateList **CList, const CHAR *fname, INT4 *candlen );

#ifdef USE_UNZIP
void ReadCandidateListFromZipFile (LALStatus *, CandidateList **CList, CHAR *fname, INT4 *candlen, const INT4 *FileID);
#endif

void PrepareCells( LALStatus *, CellData **cell, const UINT4 CLength );
void RePrepareCells( LALStatus *, CellData **cell, const UINT4 CLength , const UINT4 iposition);

int compareNumOfCoincidences(const void *a, const void *b);
int compareCandidates(const void *ip, const void *jp);
int compareSignificances(const void *a, const void *b);
int compareFrequencyCell(const void *a, const void *b);
int compareINT4arrays(const INT4 *idata1, const INT4 *idata2, size_t s); /* compare two INT4 arrays of size s.*/
int compareREAL8arrays(const REAL8 *rdata1, const REAL8 *rdata2, size_t s); /* compare two REAL8 arrays of size s.*/
void add_int4_data(LALStatus *, struct int4_linked_list **list_ptr, const INT4 *data);
void delete_int4_linked_list( LALStatus *, struct int4_linked_list *list_ptr);

void get_info_of_the_cell( LALStatus *, CellData *cd, const CandidateList *CList);

void PrintResult( LALStatus *, const PolkaConfigVars *CLA, CellData *cell, const INT4 *ncell, CandidateList *CList );
void print_Fstat_of_the_cell( LALStatus *, FILE *fp, const CellData *cd, const CandidateList *CList, const INT4 icell_start, 
			      const INT4 icell_end, const REAL8 sig_thr, const REAL8 ncand_thr );
void print_info_of_the_cell( LALStatus *lalStatus, FILE *fp, const CellData *cd, const INT4 icell_start, 
			     const INT4 icell_end, const REAL8 sig_thr, const REAL8 ncand_thr);
void print_cand_of_most_coin_cell( LALStatus *lalStatus, CellData *cd, const CandidateList *CList);

void FreeMemory(LALStatus *, PolkaConfigVars *CLA, CellData *cell, CandidateList *CList, const UINT4 datalen);
void FreeConfigVars(LALStatus *, PolkaConfigVars *CLA );




/* ----------------------------------------------------------------------------- */
/* Global Variables */
/*! @param global_status LALStatus Used to initialize LALStatus lalStatus. */
LALStatus global_status;
/*! @param lalDebugLevel INT4 Control debugging behaviours. Defined in lalapps.h */
extern INT4 lalDebugLevel;
/*! @param vrbflg        INT4 Control debugging messages. Defined in lalapps.h */
extern INT4 vrbflg;



RCSID ("$Id$");



/* ------------------------------------------------------------------------------------------*/
/* Code starts here.                                                                         */
/* ------------------------------------------------------------------------------------------*/
/* ########################################################################################## */
/*!
  Main function

  @param[in] argc   INT4
  @param[in] argv[] CHAR*
  @return    return 0 on normal exit.  
*/
int main(INT4 argc,CHAR *argv[]) 
{
  LALStatus *lalStatus = &global_status;
  INT4 CLength=0;
  INT4 CLength16=0;
  INT4 sizecells=16384;
  INT4 cc1, cc2, cc3, cc4;
  
  CandidateList *SortedC = NULL;

  CandidateList *SortedC16 = NULL;

  CellData *cell = NULL;
  INT4 icell, icand, ncell, icand16;

  PolkaConfigVars PCV;
  REAL8 DeltaDeltaFlex;

  PCV.fMIN=100000;
  PCV.F1dotMIN=100000;
  PCV.AlphaMIN=100000;
  PCV.DeltaMIN=100000;
  PCV.FileIDMIN=100000;

  lalDebugLevel = 0 ;  
  vrbflg = 1;   /* verbose error-messages */

  /* Get the debuglevel from command line arg, then set laldebuglevel. */
  LAL_CALL (LALGetDebugLevel(lalStatus, argc, argv, 'v'), lalStatus);

  /* Reads command line arguments */
  LAL_CALL( ReadCommandLineArgs( lalStatus, argc,argv, &PCV ), lalStatus); 

  /* Reads in candidare files, set CLength */
  LAL_CALL( ReadCandidateFiles(lalStatus, &SortedC, &PCV, &CLength), lalStatus);

  CLength16 = 16 * CLength;

  /* Prepare cells. */
  LAL_CALL( PrepareCells( lalStatus, &cell, sizecells ), lalStatus);  

  /* Allocate memory for 16 x Candidate List */ 
  SortedC16 = (CandidateList *)LALMalloc (CLength16*sizeof(CandidateList));

  if ( !SortedC16 ) 
    { 
      LALPrintError ("Could not allocate memory for 16 candidate list.\n\n");
      ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
    }
      
  /* printf("%f %g %g %g %d\n",PCV.fMIN,PCV.F1dotMIN,PCV.AlphaMIN,  PCV.DeltaMIN, PCV.FileIDMIN);*/

  /* --------------------------------------------------------------------------------*/      
  /* initialization */
  /* Initialise arrays of sorted candidates. */

  /* flexible declination window */
  DeltaDeltaFlex = 0;
  
  icand16 = 0;
  for (icand = 0; icand < CLength; icand++)
    {
      for (cc1 = 0; cc1 < 2; cc1++)
	{
	  for (cc2 = 0; cc2 < 2; cc2++)
	    {
	      for (cc3 = 0; cc3 < 2; cc3++)
		{
		  for (cc4 = 0; cc4 < 2; cc4++)
		    {		      
		      SortedC16[icand16].f = SortedC[icand].f;
		      SortedC16[icand16].Delta = SortedC[icand].Delta;
		      SortedC16[icand16].Alpha = SortedC[icand].Alpha;
		      SortedC16[icand16].F1dot = SortedC[icand].F1dot;
		      SortedC16[icand16].TwoF = SortedC[icand].TwoF;
		      SortedC16[icand16].FileID = SortedC[icand].FileID;
		      
		      /*SortedC[icand].iFreq=(INT4) ((SortedC[icand].f/(PCV.Deltaf)) + PCV.Shiftf  );*/
		      SortedC16[icand16].iFreq=(INT4) (2*((SortedC[icand].f-PCV.fMIN)/(PCV.Deltaf)) + cc1  );
		      
		      /* This was used for an isotropic sky-grid */
		      /*SortedC[icand].iDelta=(INT4)(SortedC[icand].Delta/(PCV.DeltaDelta)  + PCV.ShiftDelta ); */
		      
		      /* This is used for the anisotropic sky-grid produced by a metric. */
		      DeltaDeltaFlex = PCV.DeltaAlpha + PCV.DeltaDelta * exp( -(PCV.Kappa)*(SortedC[icand].Delta)*(SortedC[icand].Delta) ); 
		      /* SortedC[icand].iDelta=(INT4)((SortedC[icand].Delta/(DeltaDeltaFlex))  + PCV.ShiftDelta ); */
		      SortedC16[icand16].iDelta=(INT4)(2*(SortedC[icand].Delta/(DeltaDeltaFlex))  + cc2 ); 
		      /* SortedC[icand].iAlpha=(INT4)((SortedC[icand].Alpha*cos(SortedC[icand].Delta)/(PCV.DeltaAlpha))  + PCV.ShiftAlpha  ); */
		      SortedC16[icand16].iAlpha=(INT4)(2*(SortedC[icand].Alpha*cos(SortedC[icand].Delta)/(PCV.DeltaAlpha))  + cc3 );
		      /* SortedC[icand].iF1dot=(INT4)((SortedC[icand].F1dot/(PCV.DeltaF1dot))  + PCV.ShiftF1dot ); */
		      SortedC16[icand16].iF1dot=(INT4)(2*((SortedC[icand].F1dot-PCV.F1dotMIN)/(PCV.DeltaF1dot))  + cc4 );	  
		     
		      SortedC[icand].iCand = icand;
		      SortedC16[icand16].iCand = icand16; /* Keep the original ordering before sort to refer the orignal data later. */
		
		      icand16++;
		    }
		}
	    }
	}
    }

  /* free old candidate list */
  if( SortedC != NULL ) LALFree(SortedC);

  /* sort arrays of candidates */
  qsort(SortedC16, (size_t)CLength16, sizeof(CandidateList), compareCandidates);
  

  /* Initialise the first cell by the first candidate. */
  icell = 0;
  icand16 = 0;
  cell[icell].iFreq = SortedC16[icand16].iFreq;
  cell[icell].iDelta = SortedC16[icand16].iDelta;
  cell[icell].iAlpha = SortedC16[icand16].iAlpha;
  cell[icell].iF1dot = SortedC16[icand16].iF1dot;
  cell[icell].CandID->data = icand16; 
  cell[icell].nCand = 1;

  /* ------------------------------------------------------------------------------*/      
  /* main loop over candidates  */
  icell = 0;
  for (icand16 = 1; icand16 < CLength16; icand16++)
    {
    /* Skip candidate events with 2F values below the threshold of TwoFthr. */
    if ( SortedC16[icand16].TwoF > PCV.TwoFthr ) 
      {

	if( SortedC16[icand16].iFreq  == cell[icell].iFreq  && 
	    SortedC16[icand16].iDelta == cell[icell].iDelta &&
	    SortedC16[icand16].iAlpha == cell[icell].iAlpha &&
	    SortedC16[icand16].iF1dot == cell[icell].iF1dot ) 
	  { 
	    /* This candidate is in this cell. */
	    INT4 lastFileIDinThisCell = SortedC16[cell[icell].CandID->data].FileID;
	    if( SortedC16[icand16].FileID != lastFileIDinThisCell ) 
	      {
		/* This candidate has a different file id from the candidates in this cell. */
		    LAL_CALL( add_int4_data( lalStatus, &(cell[icell].CandID), &(icand16) ), lalStatus );
		    cell[icell].nCand += 1;
	      }
	    else  
	      { 
		/* This candidate has the same file id to one of candidates in this cell. */ 
		/* 	       Because the array is already sorted in the DECREASING ORDER OF 2F,  */
		/* 		 we do nothing here. */
	      }  /*if( SortedC16[icand16].FileID != lastFileIDinThisCell ) */
	  } /*  if( SortedC16[icand16].iFreq  == cell[icell].iFreq  && .. ) */ 
	else 
	  {	  
	    /* This candidate is outside of this cell. */
	    
	    icell++;
	    /* Re-Allocate memory */
	    if( icell >=  sizecells ){
	      sizecells = sizecells + 16384;
	      LAL_CALL( RePrepareCells(lalStatus, &cell, sizecells, icell), lalStatus);  
	    }
	    cell[icell].iFreq = SortedC16[icand16].iFreq;
	    cell[icell].iDelta = SortedC16[icand16].iDelta;
	    cell[icell].iAlpha = SortedC16[icand16].iAlpha;
	    cell[icell].iF1dot = SortedC16[icand16].iF1dot;
	    cell[icell].CandID->data = icand16;
	    cell[icell].nCand = 1;
	  } /*  if( SortedC16[icand].iFreq  == cell[icell].iFreq  && .. ) */ 

      } /* if ( SortedC16[icand].TwoF > PCV.TwoFthr ) */

    } /* for (icand=1; icand < CLength; icand++): loop over candidate list */      

  /* ---------------------------------------------------------------------------------------- */      


  /* Get the information in each cell. */
  ncell=icell+1; /* number of the cells in which more than or at least one candidate exists. */
  for(icell=0;icell<ncell;icell++) {
    LAL_CALL( get_info_of_the_cell( lalStatus, &cell[icell], SortedC16), lalStatus);
  }  
  fprintf(stderr,"Number of populated cells: %d \t Length of SortedC16: %d\n", ncell, CLength16);

  /* -----------------------------------------------------------------------------------------*/      
  /* Output results */
  LAL_CALL( PrintResult( lalStatus, &PCV, cell, &ncell, SortedC16),lalStatus );

  /* -----------------------------------------------------------------------------------------*/      
  /* Clean-up */
  LAL_CALL( FreeMemory(lalStatus, &PCV, cell, SortedC16, sizecells), lalStatus);
  
  LALCheckMemoryLeaks(); 

  return(POLKA_EXIT_OK);
 
} /* main() */



/* ########################################################################################## */
/* ------------------------------------------------------------------------------*/      
/* Initialize the code: allocate memory, set initial values.                     */
/* ------------------------------------------------------------------------------*/      
/*!
  Allocate memory for the cells.
  This function initialize the celldata variable.

  @param[in,out] lalStatus LALStatus*
  @param[out]    cell      CellData** CellData structure to be initialized
  @param[in]     CLength   UINT4      Number of the cells
*/
void PrepareCells( LALStatus *lalStatus, CellData **cell, const UINT4 CLength )
{
  UINT4 icell, ncell;
  INT4 errflg = 0;

  INITSTATUS( lalStatus, "InitCode", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  ASSERT( *cell == NULL, lalStatus, POLKAC_ENONULL, POLKAC_MSGENONULL);

  *cell = (CellData *) LALCalloc( CLength, sizeof(CellData) );
  if( *cell == NULL ) {
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }

  for(icell=0;icell<CLength;icell++) {
    (*cell)[icell].CandID = NULL;
    (*cell)[icell].CandID = (struct int4_linked_list *) LALCalloc( 1, sizeof(struct int4_linked_list) );
    if( (*cell)[icell].CandID == NULL ) {
      errflg = 1;
      break;
    }
    (*cell)[icell].CandID->next = NULL;
    (*cell)[icell].iFreq = 0;
    (*cell)[icell].iDelta = 0;
    (*cell)[icell].iAlpha = 0;
    (*cell)[icell].iF1dot = 0;
    (*cell)[icell].nCand = 0;
    (*cell)[icell].Freq = 0.0;
    (*cell)[icell].Delta = 0.0;
    (*cell)[icell].Alpha = 0.0;
    (*cell)[icell].F1dot = 0.0;
    (*cell)[icell].significance = 0;
  }


  if( errflg != 0 ) {
    ncell = icell;
    for(icell=0;icell<ncell;icell++) {
      LALFree( (*cell)[icell].CandID );
    }
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }


  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* PrepareCells() */



/* ########################################################################################## */
/* ------------------------------------------------------------------------------*/      
/* Re-allocate memory, set initial values.                     */
/* ------------------------------------------------------------------------------*/      
/*!
  Re-Allocate memory for the cells and initialize the additional celldata variables.

  @param[in,out] lalStatus LALStatus*
  @param[out]    cell      CellData** CellData structure to be initialized
  @param[in]     CLength   UINT4      Number of the cells
*/
 
void RePrepareCells( LALStatus *lalStatus, CellData **cell, const UINT4 CLength , const UINT4 iposition)
{
  UINT4 icell, ncell;
  INT4 errflg = 0;
  
  INITSTATUS( lalStatus, "RePrepareCells", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  /*  ASSERT( *cell == NULL, lalStatus, POLKAC_ENONULL, POLKAC_MSGENONULL); */

  CellData *tmp;
  tmp = (CellData *)LALRealloc (*cell, ( CLength * sizeof(CellData)) );
  if ( !tmp ) 
    { 
      LALPrintError("Could not re-allocate memory for cells \n\n");
      ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
    }
  *cell = tmp;

  if( *cell == NULL ) {
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }

  for(icell=iposition;icell<CLength;icell++) {
    (*cell)[icell].CandID = NULL;
    (*cell)[icell].CandID = (struct int4_linked_list *) LALCalloc( 1, sizeof(struct int4_linked_list) );
    if( (*cell)[icell].CandID == NULL ) {
      errflg = 1;
      break;
    }
    (*cell)[icell].CandID->next = NULL;
    (*cell)[icell].iFreq = 0;
    (*cell)[icell].iDelta = 0;
    (*cell)[icell].iAlpha = 0;
    (*cell)[icell].iF1dot = 0;
    (*cell)[icell].nCand = 0;
    (*cell)[icell].Freq = 0.0;
    (*cell)[icell].Delta = 0.0;
    (*cell)[icell].Alpha = 0.0;
    (*cell)[icell].F1dot = 0.0;
    (*cell)[icell].significance = 0;
  }


  if( errflg != 0 ) {
    ncell = icell;
    for(icell=iposition;icell<ncell;icell++) {
      LALFree( (*cell)[icell].CandID );
    }
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }


  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* RePrepareCells() */





/* ########################################################################################## */
/*! 
  Output results 



  @param[in,out] lalStatus LALStatus*
  @param[in]     CLA       PolkaConfigVars*
  @param[in]     cell      CellData*
  @param[in]     ncell     UINT4* Number of the cells
  @param[in]     CList     CandidateList
*/
void PrintResult(LALStatus *lalStatus, const PolkaConfigVars *CLA, CellData *cell, const INT4 *ncell, CandidateList *CList)
{
  INT4 icell;
  CHAR fnameSigTime[]="polka_significant_outlier_2FofTime"; /* Time variation of 2F of some significant outliers. */
  CHAR fnameSigCell[]="polka_significant_outlier_CellData"; /* Cell information of some significant outliers*/
  CHAR fnameCoiTime[]="polka_coincident_outlier_2FofTime";  /* Time variation of 2F of some coincident outliers. */
  CHAR fnameCoiCell[]="polka_coincident_outlier_CellData";  /* Cell information of some coincident outliers*/
  /* The cell info of the maximum coincident event over each Frequency cell but all over the sky.*/
  CHAR fnameMaxOverSky[]="polka_maxcoincident_over_each_freqcell_and_allsky"; 
  FILE *fp = NULL, *fpSigTime = NULL, *fpSigCell = NULL, *fpCoiTime = NULL, *fpCoiCell = NULL;
  INT4 *count;
  INT4 nc, nmax,idxmax = 0;
  REAL4 Sigmax = 0.0;

  INITSTATUS( lalStatus, "PrintResult", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  ASSERT( cell != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( CLA != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( CList != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);

  /* ------------------------------------------------------------- */
  /* First Sort arrays of candidates based on number of candidate. */ 
  qsort(cell, (size_t) (*ncell), sizeof(CellData), compareNumOfCoincidences);


  nmax = cell[0].nCand; /* This is the number of the maximum coincidences. */

  if( (count = (INT4 *) LALCalloc( (size_t) (nmax + 1), sizeof(INT4))) == NULL ) {
    LALPrintError("Could not allocate Memory! \n");
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }
  


  /* ------------------------------------------------------------- */
  /* Print out to the user-specified output file all the information in all the cell. 
     This file can be too huge to be tractable.*/
  if( (fp = fopen(CLA->OutputFile,"w")) == NULL ) 
    {
      LALPrintError("\n Cannot open file %s\n",CLA->OutputFile); 
      ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
    }
  /* output for all the cells */
  print_info_of_the_cell( lalStatus->statusPtr, fp, cell, 0,(*ncell),0,0);
  BEGINFAIL(lalStatus) {fclose(fp);} ENDFAIL(lalStatus);

  /* number counts and find the most significant event. */
  for(icell=0;icell<(*ncell);icell++) {
    nc=cell[icell].nCand;
    count[nc] += 1;
    if( Sigmax < cell[icell].significance) {
      Sigmax = cell[icell].significance;
      idxmax = icell;
    }
  }


  /* ------------------------------------------------------------- */
  /* output summary table. */
  if(lalDebugLevel < 3 ) {
    fprintf(stderr,"%% Most significant cell : freq [Hz]\tdec [rad]\tra [rad]  \tF1dot \t\t   #[events]\tSig" "\n");
    fprintf(stderr, "%%\t\t\t     ");
    TRY( print_info_of_the_cell( lalStatus->statusPtr, stderr, cell, idxmax,idxmax+1,0,0), lalStatus);
    fprintf(stderr,"%% Most coincident cell  : freq [Hz]\tdec [rad]\tra [rad]  \tF1dot \t\t   #[events]\tSig" "\n");
    fprintf(stderr, "%%\t\t\t     ");
    TRY( print_info_of_the_cell( lalStatus->statusPtr, stderr, cell, 0,1,0,0), lalStatus);

    nmax = cell[0].nCand;
    fprintf(stderr,"%% # of coincidences: \n");
    for(nc=0;nc<=nmax;nc++) {
      fprintf(stderr,"%7d",nc);
    }

    fprintf(stderr,"\n");
    fprintf(stderr,"%% # of cells       : \n");
    for(nc=0;nc<=nmax;nc++) { 
      fprintf(stderr, "%7d",count[nc]);
    }
    
    fprintf(stderr,"\n%%\n%% Candidates of most coincident cell : \n%% freq [Hz]\tdec [rad]\tra [rad]  \tF1dot[Hz/s]\t\t2F" "\n");
    TRY( print_cand_of_most_coin_cell( lalStatus->statusPtr, &cell[0], CList), lalStatus);

  
  }
  LALFree( count );

 
  if( CLA->AutoOut || cell[0].nCand >= CLA->Nthr ) 
    {
      if( (fpCoiCell = fopen(fnameCoiCell,"w")) == NULL || (fpCoiTime = fopen(fnameCoiTime,"w")) == NULL )
	{ 
	  LALPrintError("\n Cannot open file %s or %s\n",fnameCoiCell,fnameCoiTime); 
	  exit(POLKA_EXIT_ERR); 
	}
    }

  if( CLA->AutoOut || Sigmax > CLA->Sthr ) 
    {
      if( (fpSigCell = fopen(fnameSigCell,"w")) == NULL || (fpSigTime = fopen(fnameSigTime,"w")) == NULL )
	{ 
	  LALPrintError("\n Cannot open file %s or %s\n",fnameSigCell,fnameSigTime); 
	  exit(POLKA_EXIT_ERR); 
	}
    }



  /* ------------------------------------------------------------- */
  if( CLA->AutoOut ) 
    {  /* Output the info of the most significant and the most coincident event. */

      /* Output the info of the most coincident event. */
      /* Information of the cell. */
      print_info_of_the_cell( lalStatus->statusPtr, fpCoiCell, cell, 0,1,0,0);
      BEGINFAIL(lalStatus) {fclose(fpCoiCell);} ENDFAIL(lalStatus);
      /* Print F stat from each file contributed to this cell. */
      print_Fstat_of_the_cell( lalStatus->statusPtr, fpCoiTime, cell, CList, 0,1,0,0 );
      BEGINFAIL(lalStatus) {fclose(fpCoiTime);} ENDFAIL(lalStatus);
    
      /* Output the info of the most significant event. */
      /* Information of the cell. */
      print_info_of_the_cell( lalStatus->statusPtr, fpSigCell, cell, idxmax,idxmax+1,0,0);
      BEGINFAIL(lalStatus) {fclose(fpSigCell);} ENDFAIL(lalStatus);
      /* Print F stat from each file contributed to this cell. */
      print_Fstat_of_the_cell( lalStatus->statusPtr, fpSigTime, cell, CList, idxmax,idxmax+1,0,0 );
      BEGINFAIL(lalStatus) {fclose(fpSigTime);} ENDFAIL(lalStatus);


    } /* if( CLA->AutoOut ) */ 
  else 
    {
      /* output only on outliers larger than Nthr on number of coincidences and Sthr on significance.*/
      if( cell[0].nCand >= CLA->Nthr ) 
	{
	  
	  /* Information of the cell. */
	  print_info_of_the_cell( lalStatus->statusPtr, fpCoiCell, cell, 0, 0, 0, CLA->Nthr);
	  BEGINFAIL(lalStatus) {fclose(fpCoiCell);} ENDFAIL(lalStatus);
	  /* Print F stat from each file contributed to this cell. */
	  print_Fstat_of_the_cell( lalStatus->statusPtr, fpCoiTime, cell, CList, 0, 0, 0, CLA->Nthr );
	  BEGINFAIL(lalStatus) {fclose(fpCoiTime);} ENDFAIL(lalStatus);

	} /* if( cell[0].nCand > CLA->Nthr ) */
      
      
      /* ------------------------------------------------------------- */
      /* Second Sort arrays of candidates based on significance, if necessary. */ 
      /* output only on outliers */
      if( Sigmax > CLA->Sthr ) 
	{
	  qsort(cell, (size_t) (*ncell), sizeof(CellData), compareSignificances);

	  /* Information of the cell. */
	  print_info_of_the_cell( lalStatus->statusPtr, fpSigCell, cell, 0, 0, CLA->Sthr, 0);
	  BEGINFAIL(lalStatus) {fclose(fpSigCell);} ENDFAIL(lalStatus);
	  /* Print F stat from each file contributed to this cell. */
	  print_Fstat_of_the_cell( lalStatus->statusPtr, fpSigTime, cell, CList, 0, 0, CLA->Sthr, 0 );
	  BEGINFAIL(lalStatus) {fclose(fpSigTime);} ENDFAIL(lalStatus);

	} /* if( cell[0].significance > CLA->Sthr ) */
    } /* else of if( CLA->AutoOut ) */


  if( CLA->AutoOut || cell[0].nCand >= CLA->Nthr ) {
    fclose(fpCoiTime);
    fclose(fpCoiCell);
  }

  if( CLA->AutoOut || Sigmax > CLA->Sthr ) { 
    fclose(fpSigTime);
    fclose(fpSigCell);
  }



  /* ------------------------------------------------------------- */
  /* Output the maximum coincident event over each frequency cell and over all the sky. */
  qsort(cell, (size_t) (*ncell), sizeof(CellData), compareFrequencyCell);

  
  {
    INT4 prev_iFreq = -1;

    if( ( fp = fopen(fnameMaxOverSky,"w") ) == NULL ) {
      { 
	LALPrintError("\n Cannot open file %s or %s\n",fnameCoiCell,fnameCoiTime); 
	exit(POLKA_EXIT_ERR); 
      }
    }

    for( icell=0; icell<(*ncell); icell++ ) {
      if( cell[icell].iFreq != prev_iFreq ) {
	print_info_of_the_cell( lalStatus->statusPtr, fp, cell, icell, icell+1, 0, 0);
	BEGINFAIL(lalStatus) {fclose(fp);} ENDFAIL(lalStatus);
      }
      prev_iFreq = cell[icell].iFreq;
    }
    fclose(fp);
  }
  

  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* PrintResult() */




/* ########################################################################################## */
/*! 
  Print_info_of_the_cell

  This function basically shows the information of the outliers, where 
  those are in the parameters space and how coincident those are.
 
  Print out into FILE \b fp the infos of the cells 
  \li whose indices are between \b icell_start and \b icell_end, and 
  \li in which numbers of the events are above \b ncand_thr, and 
  \li in which significances are above \b sig_thr.

  @param[in,out] lalStatus LALStatus*
  @param[in]     fp        FILE*
  @param[in]     cd        CellData*
  @param[in]     INT4      icell_start
  @param[in]     INT4      icell_end
  @param[in]     REAL8     sig_thr
  @param[in]     REAL8     ncand_thr
*/
void print_info_of_the_cell( LALStatus *lalStatus, 
			     FILE *fp, 
			     const CellData *cd, 
			     const INT4 icell_start, 
			     const INT4 icell_end, 
			     const REAL8 sig_thr, 
			     const REAL8 ncand_thr )
{
  INT4 icell;

  INITSTATUS( lalStatus, "print_info_of_the_cell", rcsid );
  ASSERT( cd != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);

  icell = icell_start;
  while( icell < icell_end && 
	 cd[icell].significance > sig_thr && 
	 cd[icell].nCand > ncand_thr ) 
    {
      fprintf(fp,"%" LAL_REAL4_FORMAT "\t%" LAL_REAL4_FORMAT "\t%" LAL_REAL4_FORMAT "\t% g" " \t\t%" LAL_INT4_FORMAT "\t%" LAL_REAL4_FORMAT "\n", cd[icell].Freq, cd[icell].Delta, cd[icell].Alpha, cd[icell].F1dot, cd[icell].nCand, cd[icell].significance);
      icell++;
    }


  RETURN (lalStatus);
} /* void print_info_of_the_cell() */




/* ########################################################################################## */
/*!
  Free memory 

  Free Configuration variables \b CLA, CellData variable \b cell, CandidateList var \b CList.

  @param[in,out] lalStatus LALStatus* 
  @param[in]     CLA       PolkaConfigVars* configuration variables structure
  @param[in]     cell      CellData*        CellData structure
  @param[in]     CList     CandidateList*   CandidateList structure
  @param[in]     CLength   UINT4            Number of the cells
*/
void 
FreeMemory( LALStatus *lalStatus, 
	    PolkaConfigVars *CLA, 
	    CellData *cell, 
	    CandidateList *CList, 
	    const UINT4 CLength)
{
  UINT4 icell;

  INITSTATUS( lalStatus, "FreeMemory", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  FreeConfigVars( lalStatus->statusPtr, CLA );

  if( CList != NULL ) LALFree(CList);

  /* FIX (?) ME:  
     This part takes really long, when lalDebugLevel = 3. I do not know why.*/
  if( cell != NULL ) {
    for(icell=0;icell<CLength;icell++) {
      TRY( delete_int4_linked_list( lalStatus->statusPtr, cell[icell].CandID ), lalStatus );
    }
    LALFree(cell);
  }
    
  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* FreeMemory */


/* ########################################################################################## */
/*!
  Free Configuration variables \b CLA.

  @param[in,out] lalStatus LALStatus* 
  @param[in]     CLA       PolkaConfigVars* configuration variables structure
*/
void FreeConfigVars(LALStatus *lalStatus, PolkaConfigVars *CLA )
{
  UINT4 k;

  INITSTATUS( lalStatus, "FreeConfigVars", rcsid );

  if( CLA->FstatsFile != NULL ) LALFree(CLA->FstatsFile);
  if( CLA->OutputFile != NULL ) LALFree(CLA->OutputFile);
  if( CLA->InputDir != NULL ) LALFree(CLA->InputDir);
  if( CLA->BaseName != NULL ) LALFree(CLA->BaseName);


  if( (CLA->InputDir != NULL) && (CLA->BaseName != NULL) ) 
    { /* We have used glob and allocated mem for Filelist.*/
      for (k=0;k<CLA->NFiles;k++)
	{
	  if(CLA->Filelist[k] != NULL ) 
	    LALFree (CLA->Filelist[k]);
	} 
      LALFree (CLA->Filelist);
    }

  RETURN (lalStatus);
} /* FreeCOnfigVars */


/* ########################################################################################## */
/*!
  add data to linked structure 

  @param[in,out] lalStatus   LALStatus* 
  @param[in,out] list_ptr    int4_linked_list**
  @param[in]     data        INT4*
*/
void add_int4_data(LALStatus *lalStatus, struct int4_linked_list **list_ptr, const INT4 *data)
{
  struct int4_linked_list *p = NULL;

  INITSTATUS( lalStatus, "add_int4_data", rcsid );

  p = (struct int4_linked_list *) LALMalloc(sizeof(struct int4_linked_list));
  if( p == NULL ) {
    LALPrintError("Could not allocate Memory! \n");
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }
  p->data = *(data);
  p->next = *list_ptr;
  *list_ptr = p;

  RETURN (lalStatus);
} /* void add_int4_data() */


/* ########################################################################################## */
/*!
  delete a linked structure 

  @param[in,out] lalStatus   LALStatus* 
  @param[in]     list_ptr    int4_linked_list*
*/
void delete_int4_linked_list( LALStatus *lalStatus, struct int4_linked_list *list_ptr )
{
  INT4 ic;
  struct int4_linked_list *q;

  INITSTATUS( lalStatus, "delete_int4_linked_list", rcsid );

  ic = 0;
  while( list_ptr !=NULL && ic <= LINKEDSTR_MAX_DEPTH ) {  
    q = list_ptr->next;
    LALFree( list_ptr );
    list_ptr = q;
    ic++;
  }
  if( ic >  LINKEDSTR_MAX_DEPTH ) {
    LALPrintError("Maximum depth of linked structure reached!");
    exit(POLKA_EXIT_ERR);
  }

  RETURN (lalStatus);
} /* void delete_int4_linked_list() */


/* ########################################################################################## */
/*!
  get info of this cell. 

  We have indices of the candidate events contained in each cell 
  before the call of this function. This function computes the joint 
  significance, average alpha, average delta, and average frequency 
  of the events in each cell and stores them into cellData sturcture.

  @param[in,out] lalStatus   LALStatus* 
  @param[in,out] cd          CellData*
  @param[in]     CList       CandidateList*  
*/
void get_info_of_the_cell( LALStatus *lalStatus, CellData *cd, const CandidateList *CList )
{
  INT4 idx, ic;
  REAL8 lfa;
  struct int4_linked_list *p;

  INITSTATUS( lalStatus, "get_info_of_the_cellV2", rcsid );
  ASSERT( cd != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( CList != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);

  p = cd->CandID;

  ic = 0;
  while( p !=NULL && ic <= LINKEDSTR_MAX_DEPTH ) { 
    idx = p->data;
    lfa = CList[idx].TwoF/2.0 - log(1.0 + CList[idx].TwoF/2.0);
    cd->significance += lfa;
    cd->F1dot += CList[idx].F1dot;
    cd->Alpha += CList[idx].Alpha;
    cd->Delta += CList[idx].Delta;
    cd->Freq += CList[idx].f;
    p = p->next;
    ic++;
  }

  if( ic >  LINKEDSTR_MAX_DEPTH ) {
    LALPrintError("Maximum depth of linked structure reached!");
    exit(POLKA_EXIT_ERR);
  }

  cd->F1dot /= cd->nCand;
  cd->Alpha /= cd->nCand;
  cd->Delta /= cd->nCand;
  cd->Freq  /= cd->nCand;
  
  RETURN (lalStatus);
} /* void get_info_of_the_cell() */



/* ########################################################################################## */
/*!
  print candidates of most coincident cell. 

  We have indices of the candidate events contained in each cell 
  before the call of this function. This function returns all
  the candidates belonging to the most coincident cell.

  @param[in,out] lalStatus   LALStatus* 
  @param[in,out] cd          CellData*
  @param[in]     CList       CandidateList*  
*/

void print_cand_of_most_coin_cell( LALStatus *lalStatus, CellData *cd, const CandidateList *CList )
{
  INT4 idx, ic;
  struct int4_linked_list *p;

  INITSTATUS( lalStatus, "print_cand_of_most_coin_cell", rcsid );
  ASSERT( cd != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( CList != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);  

  p = cd->CandID;

  ic = 0;
  while( p !=NULL && ic <= LINKEDSTR_MAX_DEPTH ) { 
    idx = p->data;

    fprintf(stderr,"  %" LAL_REAL4_FORMAT "\t%" LAL_REAL4_FORMAT "\t%" LAL_REAL4_FORMAT "\t% g" "\t\t%g \n", 
	    CList[idx].f, CList[idx].Delta, CList[idx].Alpha, CList[idx].F1dot, CList[idx].TwoF);
    
    p = p->next;
    ic++;
  }

  if( ic >  LINKEDSTR_MAX_DEPTH ) {
    LALPrintError("Maximum depth of linked structure reached!");
    exit(POLKA_EXIT_ERR);
  }

  RETURN (lalStatus);
} /* void print_cand_of_most_coin_cell() */



/* ########################################################################################## */
/*!
  print F stat. 

  Print out into FILE \b fp the F stats of the cells 
  \li whose indices are between \b icell_start and \b icell_end, and 
  \li in which numbers of the events are above \b ncand_thr, and 
  \li in which significances are above \b sig_thr.

  This function basically shows how F statistics variers from file to file.

  @param[in,out] lalStatus   LALStatus* 
  @param[in]     fp          FILE*
  @param[in]     cd          CellData*
  @param[in]     CList       CandidateList*  
  @param[in]     icell_start INT4  Starting index of a cell
  @param[in]     icell_end   INT4  Ending index of a cell
  @param[in]     sig_thr     REAL8 Threshold on significance of the candidate events 
  above which results will be printed out.
  @param[in]     ncand_thr   REAL8 Threshold on number of the candidate events 
  above which results will be printed out.
*/
void print_Fstat_of_the_cell( LALStatus *lalStatus, 
			      FILE *fp, 
			      const CellData *cd, 
			      const CandidateList *CList, 
			      const INT4 icell_start, 
			      const INT4 icell_end, 
			      const REAL8 sig_thr, 
			      const REAL8 ncand_thr )
{
  INT4 idx, ic, icell;
  struct int4_linked_list *p;

  INITSTATUS( lalStatus, "print_Fstat_of_the_cell", rcsid );
  ASSERT( cd != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( CList != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);

  icell = icell_start;
  while( icell < icell_end && 
	 cd[icell].significance > sig_thr && 
	 cd[icell].nCand > ncand_thr ) 
    {

      p = cd[icell].CandID;
      
      ic = 0;
      while( p !=NULL && ic <= LINKEDSTR_MAX_DEPTH ) { 
	idx = p->data;
	fprintf(fp,"%" LAL_INT4_FORMAT "\t%" LAL_INT4_FORMAT "\t%" LAL_REAL4_FORMAT "\n", 
		icell, CList[idx].FileID, CList[idx].TwoF );
	p = p->next;
	ic++;
      } /*   while( p !=NULL && ic <= LINKEDSTR_MAX_DEPTH ) {  */

      if( ic >  LINKEDSTR_MAX_DEPTH ) {
	LALPrintError("Maximum depth of linked structure reached!");
	exit(POLKA_EXIT_ERR);
      }

      icell++;
    } /*   while( icell < icell_end && ...  */

  RETURN (lalStatus);
} /* void print_Fstat_of_the_cell( ) */



/* ########################################################################################## */
/*!
  Sorting function to sort cell indices in the INCREASING order of f, delta, alpha, FileID and 
  DECREASING ORDER OF a significance.

  @param[in] a CellData* to be compared. 
  @param[in] b CellData* to be compared. 
  @return If a<b, return 1, if a==b return 0, otherwise return 1. 
*/
int compareCandidates(const void *a, const void *b)
{
  const CandidateList *ip = a;
  const CandidateList *jp = b;
  int res;
  INT4 ap[5],bp[5];

  ap[0]=ip->iFreq;
  ap[1]=ip->iDelta;
  ap[2]=ip->iAlpha;
  ap[3]=ip->iF1dot;
  ap[4]=ip->FileID;
 
  bp[0]=jp->iFreq;
  bp[1]=jp->iDelta;
  bp[2]=jp->iAlpha;
  bp[3]=jp->iF1dot;
  bp[4]=jp->FileID;
 
  res = compareINT4arrays( ap,  bp, 5);
  if( res == 0 ) {
    REAL8 F1, F2;
    F1=ip->TwoF;
    F2=jp->TwoF;
    /* I put F1 and F2 inversely, because I would like to get decreasingly-ordered set. */ 
    res = compareREAL8arrays( &F2,  &F1, 1);
  } 
  return res;
} /* int compareCandidates() */


/* ########################################################################################## */
/*!
  Sorting function to sort cells indices in the INCREASING order of a frequency index, and 
  the DECREASING ORDER OF a number of events in a cell.

  Compare two cells in terms of a frequency index.
  If those are the same, then compare them in terms of a number of candidates.
  If we use qsort, we will have cells ordered as 
  cell[0], cell[1], ....
  where cell[0] has a smaller frequency index (or if it is equal to that of cell[1]
  then a larger number of the candidate events) than that of cell[1].


  @param[in] a CellData* to be compared. 
  @param[in] b CellData* to be compared. 
  @return If a<b, return 1, if a==b return 0, otherwise return 1. 
*/
int compareFrequencyCell(const void *a, const void *b)
{
  const CellData *ip = a;
  const CellData *jp = b;
  int res;
  INT4 ap[2],bp[2];

  ap[0]=ip->iFreq; /* iFreq for cand 1.*/
  bp[0]=jp->iFreq; /* iFreq for cand 2.*/

  /* I put n1 and n2 inversely, because I would like to get decreasingly-ordered set. */ 
  ap[1]=jp->nCand; /* nCand for cand 2.*/
  bp[1]=ip->nCand; /* nCand for cand 1.*/

  res = compareINT4arrays( ap,  bp, 2);

  return res;
} /* int compareSignificances() */




/* ########################################################################################## */
/*!
  Sorting function to sort cells indices in the 
  DECREASING ORDER OF a significanc and a number of candidate events in cells.

  Compare two cells in terms of the significance.
  If those are the same, then compare them in terms of the number of the candidates.
  If we use qsort, we will have cells ordered as 
  cell[0], cell[1], ....
  where cell[0] has a largher significance (or if it is equal to that of cell[1]
  then a larger number of the candidate events) than that of cell[1].


  @param[in] a CellData* to be compared. 
  @param[in] b CellData* to be compared. 
  @return If a<b, return 1, if a==b return 0, otherwise return 1. 
*/
int compareSignificances(const void *a, const void *b)
{
  const CellData *ip = a;
  const CellData *jp = b;
  int res;

  REAL8 F1, F2;
  F1=ip->significance;
  F2=jp->significance;
  /* I put F1 and F2 inversely, because I would like to get decreasingly-ordered set. */ 
  res = compareREAL8arrays( &F2,  &F1, 1);
  if( res == 0 ) {
    INT4 n1, n2;
    n1=ip->nCand;
    n2=jp->nCand;
    /* I put n1 and n2 inversely, because I would like to get decreasingly-ordered set. */ 
    res = compareINT4arrays( &n2,  &n1, 1);
  } 


  return res;
} /* int compareSignificances() */



/* ########################################################################################## */
/*!
  Sorting function to sort cells indices in the 
  DECREASING ORDER OF a number of candidate events and a significance in cells.

  Compare two cells in terms of the number of the events in the cells. 
  If those are the same, then compare them in terms of the significance.
  If we use qsort, we will have cells ordered as 
  cell[0], cell[1], ....
  where cell[0] has a largher number of the candidate events (or if it is equal to that of cell[1]
  then a larger significane) than that of cell[1].

  @param[in] a CellData* to be compared
  @param[in] b CellData* to be compared
  @return If a<b, return 1, if a==b return 0, otherwise return 1. 
*/
int compareNumOfCoincidences(const void *a, const void *b)
{
  const CellData *ip = a;
  const CellData *jp = b;
  int res;

  INT4 n1, n2;

  n1=ip->nCand;
  n2=jp->nCand;
  /* I put n1 and n2 inversely, because I would like to get decreasingly-ordered set. */ 
  res = compareINT4arrays( &n2,  &n1, 1);
  if( res == 0 ) {
    REAL8 F1, F2;
    F1=ip->significance;
    F2=jp->significance;
    /* I put F1 and F2 inversely, because I would like to get decreasingly-ordered set. */ 
    res = compareREAL8arrays( &F2,  &F1, 1);
  } 

  return res;
} /* int compareNumOfCoincidences() */


/*!
  Compare two REAL8 arrays of the same size \b n.
  First compare ap[0] and bp[0]. If ap[0] < bp[0], then 
  return -1. If ap[0] > bp[0], then return 1. If 
  ap[0] == bp[0], then compare ap[1] with bp[1]. Do the 
  same untill we reach the stage where we compare ap[n-1] 
  with bp[n-1]. If ap[n-1]==bp[n-1], then return 0.

  @param[in] ap REAL8 array to be compared
  @param[in] bp REAL8 array to be compared
  @param[in] n  Size of the array
  @return If ap<bp, return -1, if ap==bp return 0, otherwise return 1. 
*/
int 
compareREAL8arrays(const REAL8 *ap, const REAL8 *bp, size_t n) 
{
  if( (*ap) == (*bp) ) { 
    if ( n > 1 ){  
      return compareREAL8arrays( ap+1, bp+1, n-1 );
    } else {
      return 0;
    }
  }
  if ( (*ap) < (*bp) ) 
    return -1;    
  return 1;
} /* int compareREAL8arrays() */



/*!
  Compare two INT4 arrays of the same size \b n.
  First compare ap[0] and bp[0]. If ap[0] < bp[0], then 
  return -1. If ap[0] > bp[0], then return 1. If 
  ap[0] == bp[0], then compare ap[1] with bp[1]. Do the 
  same untill we reach the stage where we compare ap[n-1] 
  with bp[n-1]. If ap[n-1]==bp[n-1], then return 0.

  @param[in] ap INT4 array to be compared
  @param[in] bp INT4 array to be compared
  @param[in] n  Size of the array
  @return If ap<bp, return -1, if ap==bp return 0, otherwise return 1. 
*/
int 
compareINT4arrays(const INT4 *ap, const INT4 *bp, size_t n) 
{
  if( (*ap) == (*bp) ) { 
    if ( n > 1 ){  
      return compareINT4arrays( ap+1, bp+1, n-1 );
    } else {
      return 0;
    }
  }
  if ( (*ap) < (*bp) ) 
    return -1;    
  return 1;
} /* int compareINT4arrays() */


/* ########################################################################################## */
/*!
  Read Candidate File(s) and store the events into CandidateList str \b CList.

  If an input directory (\b CLA->InputDir ) is specified, call \b GetFilesListInThisDir() 
  to find the list of the files, and 
  then call \b ReadCandidateListFromZipFile() to fill CanidateList structure \b CList.

  If an input file (\b CLA->FstatsFile ) is specified, call ReadOneCandidateFile() to fill 
  CanidateList structure \b CList;

  @param[in,out] lalStatus LALStatus* 
  @param[out]    CList     CandidateList**  Candidate events struecture to be filled
  @param[in,out] CLA       PolkaConfigVars* Configuration variables structure
  @param[out]    clen      UINT4*           The total number of the candidate events in the files.
*/
void 
ReadCandidateFiles(LALStatus *lalStatus, 
		   CandidateList **CList, 
		   PolkaConfigVars *CLA, 
		   INT4 *clen)
{
  UINT4 kc;
  /*  UINT4 *CLenFthr = 0; */
  REAL8 percentage = 0;

  INITSTATUS( lalStatus, "ReadCandidateFiles", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  ASSERT( CLA != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( *CList == NULL, lalStatus, POLKAC_ENONULL, POLKAC_MSGENONULL);
  

  if( (CLA->InputDir != NULL) && (CLA->BaseName != NULL) ) 
    {
      CLA->Filelist = NULL;
      TRY( GetFilesListInThisDir( lalStatus->statusPtr, 
				  CLA->InputDir, 
				  CLA->BaseName, 
				  &(CLA->Filelist), 
				  &(CLA->NFiles) ), 
	   lalStatus );
      
      *clen = 0;     /* We first have to set the candidate list length zero. */
      /* *CLenFthr = 0; */
      *CList = NULL; /* We first have to nullify the list. */
      for (kc=0;kc<CLA->NFiles;kc++)
	{
	  if( lalDebugLevel > 1 ) {
	    fprintf(stderr,"%s\n",CLA->Filelist[kc]);
	  }

	  if( CLA->UseUnzip ) 
	    {
#ifdef USE_UNZIP
	      {INT4 FileID = 2*kc; /* the factor 2 because we have 2 sections in each file. */
	      TRY( ReadCandidateListFromZipFile( lalStatus->statusPtr, 
						 CList, 
						 CLA->Filelist[kc], 
						 clen, 
						 &FileID), 
		   lalStatus);
	      }
#endif
	    } /* if( CLA->UseUnzip ) */
	  else 
	    {	      
	      TRY( ReadOneCandidateFileV2( lalStatus->statusPtr, 
					   CList, 
					   CLA->Filelist[kc], 
					   clen ), 
		   lalStatus);	      
	    }
	} /*       for (kc=0;kc<CLA->NFiles;kc++) */

    } /* if( (CLA->InputDir != NULL) && (CLA->BaseName != NULL) )  */
  else if ( CLA->FstatsFile != NULL ) 
    {

      *CList = NULL;
      /* TRY( ReadOneCandidateFile(lalStatus->statusPtr, CList, CLA->FstatsFile, clen, CLenFthr, CLA->TwoFthr ), lalStatus );*/
      TRY( ReadOneCandidateFile(lalStatus->statusPtr, CList, CLA->FstatsFile, CLA, clen, CLA->TwoFthr ), lalStatus );
      /* The last file is from last file.*/
      CLA->NFiles = (*CList)[*clen-1].FileID;
    } /* if( (CLA->InputDir != NULL) && (CLA->BaseName != NULL) )  */
  else 
    { /* We should not be here. */
      LALPrintError("\nYou have to specify either input data directory or input data file.\n");
      exit(POLKA_EXIT_ERR);;
    }

  /*  percentage = ( (REAL8) *CLenFthr / *clen ) * 100.0;

  fprintf(stderr,"\n%%Number of the candidate events in this file/directory = %u.\n%% --- Threshold for 2F: %.3f\t Number of candidates kept: %u  or  %.3f%% --- \n",*clen, CLA->TwoFthr, *CLenFthr, percentage);  */

  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* ReadCandidateFiles() */





/* ########################################################################################## */
/*!
  Get the list of the files which has the base name \b basename in 
  \b directory and store the list into \b filelist. 
  Count the number of the files \b nfiles. This function checks 
  \li if HAVE_GLOB_H has been defined. If not, the function aborts.

  @param[in,out]  lalStatus LALStatus*
  @param[in]      directory CHAR*   Directory name for which files list will be made.
  @param[in]      basename  CHAR*   The base name of the files to be listed. 
  @param[out]     filelist  CHAR*** The list of the files will be stored in this strucutre.
  @param[out]     nfiles    UINT4*  The number of the files which has the basename 
  \b basename in the \b directory.
*/
void 
GetFilesListInThisDir( LALStatus *lalStatus, 
		       const CHAR *directory, 
		       const CHAR *basename, 
		       CHAR ***filelist, 
		       UINT4 *nfiles )
{
#ifdef HAVE_GLOB_H   
  CHAR *command = NULL;
  UINT4 fileno=0;
  glob_t globbuf;
#endif

  INITSTATUS (lalStatus, "GetFilesListInThisDir", rcsid);
  ATTATCHSTATUSPTR (lalStatus);
  ASSERT( directory != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( basename != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( *filelist == NULL, lalStatus, POLKAC_ENONULL, POLKAC_MSGENONULL);

#ifndef HAVE_GLOB_H   
  LALPrintError("Cannot use GetFilesListInThisDir() without glob.");
  ABORT( lalStatus, POLKAC_EGLOB, POLKAC_MSGEGLOB);
#endif

  command = (CHAR*) LALMalloc( strlen(directory) + strlen("/*") + strlen(basename) + strlen("*") + 1 );
  if( command == NULL ) {
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }

  strcpy(command, directory);
  strcat(command,"/*");
  
  strcat(command, basename);
  strcat(command,"*");

#ifdef HAVE_GLOB_H
  globbuf.gl_offs = 1;
  glob(command, GLOB_ERR, NULL, &globbuf);
  
  /* read file names -- MUST NOT FORGET TO PUT ERROR CHECKING IN HERE !!!! */
  
  if(globbuf.gl_pathc==0)
    {
      LALPrintError ("\nNo Input files in directory %s ... Exiting.\n\n", directory);
      ABORT (lalStatus, POLKAC_ESYS, POLKAC_MSGESYS);
    }

  /* prepare memory for all filenames */
  *filelist = NULL;
  if ( ( *filelist = (CHAR**)LALCalloc(globbuf.gl_pathc, sizeof(CHAR*))) == NULL) {
    ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
  }
  while ( fileno < (UINT4) globbuf.gl_pathc) 
    {
      (*filelist)[fileno] = NULL;
      if ( ((*filelist)[fileno] = (CHAR*)LALCalloc(1, strlen(globbuf.gl_pathv[fileno])+1)) == NULL) {
	ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
      }
      strcpy((*filelist)[fileno],globbuf.gl_pathv[fileno]);
      fileno++;
    }
  globfree(&globbuf);

  *nfiles = fileno; /* remember this is 1 more than the index value */
#endif

  LALFree(command);

  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
}




/* ########################################################################################## */
#ifdef USE_UNZIP
/*
TODO:
Check if *CList is either NULL or the memory of which is previously allocated by alloc() or the kind.
(how?).
*/
/*!
  Read the given zipped candidate 'Fstats'-file \b fname and append the events in the file to 
  the candidate-list \b CList. 
  This function is invoked only when \b USE_UNZIP is defined.
  The function aborts almost all the cases when the checks below failed.
  This function checks 
  \li if the file \b fname is readable. 
  \li if the number of the candidate event is smaller than the hardcoded number 8000000.
  \li if the file \b fname has the correct ending tag "%DONE".
  \li if the file has sections %1, %2 and %coincidence in this order.
  \li if the ranges of the values in the file are sensible.
  \li if the number of each row of the file is correct.
  \li if we could read all the events in the file.

  @param[in,out] lalStatus LALStatus* 
  @param[in,out] CList     CandidateList**  CandidateList str to be appended
  @param[in]     fname     CHAR* the name of the file to be read
  @param[in,out] candlen   UINT4* total number of the candidate events so far. 
  This will be updated after reading the file \b fname. 
  @param[in]     FileID    INT4* The \b FileID of the file to be read. Assign a \b FildID 
  to each event and record which file a certain event comes from.
*/
void  
ReadCandidateListFromZipFile( LALStatus *lalStatus, 
			      CandidateList **CList, 
			      CHAR *fname, 
			      INT4 *candlen, 
			      const INT4 *FileID )
{
  FILE *fp;
  const UINT4 max_num_candidates = 8000000; /* maximum tractable number of candidate events. */
  UINT4 numlines;
  INT4 nread;
  REAL8 epsilon=1e-5;
  UINT4 ic;
  INT4 length; /* length of file */
  CHAR *line, *endp; /* pointers to start and end of line */
  INT4 section = 0;    /* 0: non-POLKA, 1,2: IFO sections,
			 3: coincidence, 4: end-of-file */
  UINT4 nlines[2] = {0,0}; /* number of events for each IFO */
  const INT4 MAX_SECS = 4;

  UzpBuffer uzpbuff;

  INITSTATUS (lalStatus, "ReadCandidateListFromZipFile", rcsid);
  ATTATCHSTATUSPTR (lalStatus);
  ASSERT( fname != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);

  /* check if file exists.*/
  fp=fopen(fname,"rb");
  if (fp==NULL) 
    {
      LALPrintError("File %s doesn't exist!\n",fname);
      ABORT( lalStatus, POLKAC_ESYS, POLKAC_MSGESYS ); 
     }
  fclose(fp);

  /* Check if the memory to be allocated is not huge 
     (say, < 512MB. sizeof(CandidateList) ~ 60B. 512/60 = 8000000). */
  if( *candlen > max_num_candidates ) {
    LALPrintError("\nMaximum number of candidate events reached.\n");
    LALPrintError("\nWe have %u events while the maximum allowed number of events is %u.\n",*candlen,max_num_candidates);
    ABORT( lalStatus, POLKAC_ESYS, POLKAC_MSGESYS ); 
  }



  uzpbuff.strptr = NULL;

  /* ------------------------------------------------------------------------- */
  /*  Open and count the size of the candidates file */
  /* Read into buffer.  If this fails, we can't proceed. */
  if ( getfile( &uzpbuff, fname )  < 0 ) {
    if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
    LALPrintError("Cannot read file %s . \n",fname);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  length = uzpbuff.strlength;
  line = uzpbuff.strptr;
  if ( !line || length == 0 || *line == '\0' ) {
    if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
    LALPrintError ("Unknown format of the file  %s.\n\n", fname);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* ------------------------------------------------------------------------- */
  /* Check for correct ending tag.  If this isn't present, it is
     safest not to proceed (it greatly simplifies error trapping). */
  line += length;
  if ( ( length < 8 || strncmp( line - 8, "\n%DONE\r\n", 8 ) ) &&
       ( length < 7 || strncmp( line - 7, "\n%DONE\n", 7 ) ) ) {
    free(uzpbuff.strptr);
    LALPrintError("File %s does not end with the DONE_MARKER. \n",fname);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* ------------------------------------------------------------------------- */
  /* Start reading file data line-by-line and count the number of candidate events. */
  for ( line = uzpbuff.strptr; section < MAX_SECS;
	*endp = '\n', line = endp + 1 ) {

    /* Find end of line.  Previous endchecking assures us we will not
       fall off of the end of the file. */
    endp = line;
    while ( *endp != '\n' )
      endp++;

    /* Check for POLKA section divisions or EOF marker. */
    if ( !strncmp( line, "%1", 2 ) ) {
      section = 1;
      continue;
    } else if ( !strncmp( line, "%2", 2 ) ) {
      if( section != 1 ) { /* We should have section 1 before 2. */
	if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	LALPrintError("Unknown format file %s.",fname);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
      section = 2;
      continue;
    } else if ( !strncmp( line, "%coincidence", 12 ) ) {
      if( section != 2 ) { /* We should have section 2 before 3. */
	if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	LALPrintError("Unknown format file %s.",fname);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
      section = 3;
      break; /* We are not interested in the section 3 here. */
    }  /*   if ( !strncmp( line, "%1", 2 ) ) {*/
 

    /* Do non-POLKA checks: */
    if ( section == 0 ) 
      {
	LALPrintError("Unknown format file %s.",fname);
	if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
    /* Do POLKA IFO-section checks */
    else if ( section == 1 || section == 2 ) 
      {
	nlines[section-1] += 1;
      }
    /* Do POLKA coincidence-section checks. */
    else 
      { /* we should not be here */
	if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	LALPrintError("Unknown format file %s.",fname);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      } /*     if ( section == 0 )  */


    /* Done reading this line. */
  } /*   for ( line = uzpbuff.strptr; section < MAX_SECS; ... */
  /* ------------------------------------------------------------------------- */

  numlines = nlines[0] + nlines[1]; /* we have two sections. */

  if( numlines == 0 ) { /* This file is empty. Go to the next file.*/
    if( lalDebugLevel > 1 ) {
      LALPrintError( "No candidate events in the file %s\n\n", fname);
    }
    free(uzpbuff.strptr);
    DETATCHSTATUSPTR (lalStatus);
    RETURN (lalStatus);
  } 

  /* ------------------------------------------------------------------------- */
  /* reserve memory for fstats-file contents */
  if ( numlines > 0) 
    { 
      CandidateList *tmp;
      tmp = (CandidateList *)LALRealloc (*CList, ( *candlen + numlines )*sizeof(CandidateList));
      if ( !tmp ) 
	{ 
	  if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	  LALPrintError("Could not allocate memory for candidate file %s\n\n", fname);
	  ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
	}
      *CList = tmp;
    }


  /* ------------------------------------------------------------------------- */
  /* Start reading file data line-by-line. */
  section = 0;
  ic = (*candlen);
  for ( line = uzpbuff.strptr; section < MAX_SECS;
	*endp = '\n', line = endp + 1 ) {


    /* Find end of line.  Previous endchecking assures us we will not
       fall off of the end of the file. */
    endp = line;
    while ( *endp != '\n' )
      endp++;
    *endp = '\0'; 
    /* Replace *endp = '\n' by '\0' makes it easy to read file line by line. */

    /* Check for POLKA section divisions or EOF marker. */
    if ( !strncmp( line, "%1", 2 ) ) {
      section = 1;
      continue;
    } else if ( !strncmp( line, "%2", 2 ) ) {
      section = 2;
      continue;
    } else if ( !strncmp( line, "%coincidence", 12 ) ) {
      section = 3;
      break; /* We are not interested in the section 3 here. */
    } 
 

    if ( section == 1 || section == 2 ) 
      {
	CandidateList *cl=&(*CList)[ic];
	ic++;

	nread = sscanf (line, 
			"%lf %lf %lf %lf", 
			&(cl->f), &(cl->Alpha), &(cl->Delta), &(cl->TwoF) );
	cl->FileID = (*FileID) + section - 1; /* section can be either 1 or 2. */


	/* ------------------------------------------------------------------------- */
	/* check that values that are read in are sensible */
	if (
	    cl->FileID < 0                        ||
	    cl->f < 0.0                        ||
	    cl->TwoF < 0.0                        ||
	    cl->Alpha <         0.0 - epsilon  ||
	    cl->Alpha >   LAL_TWOPI + epsilon  ||
	    cl->Delta < -0.5*LAL_PI - epsilon  ||
	    cl->Delta >  0.5*LAL_PI + epsilon  ||
	    !finite(cl->FileID)                     ||                                                                 
	    !finite(cl->f)                     ||
	    !finite(cl->Alpha)                 ||
	    !finite(cl->Delta)                 ||
	    !finite(cl->TwoF)
	    ) {
	  LALPrintError(
			"Line %d of file %s has invalid values.\n"
			"First 255 chars are:\n"
			"%s\n"
			"1st and 4th field should be positive.\n" 
			"2nd field should lie between 0 and %1.15f.\n" 
			"3rd field should lie between %1.15f and %1.15f.\n"
			"All fields should be finite\n",
			ic+1, fname, line, (REAL8)LAL_TWOPI, (REAL8)-LAL_PI/2.0, (REAL8)LAL_PI/2.0);
	  LALFree ((*CList));
	  free( uzpbuff.strptr );
	  ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
	} /* end of the check of the range of the values.*/
      } /*     if ( section == 1 || section == 2 )  */
    /* Do POLKA coincidence-section checks. */
    else 
      { /* we should not be here */
	LALPrintError("Unknown format file %s.",fname);
	if( uzpbuff.strptr != NULL ) free(uzpbuff.strptr);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      } /* if ( section == 1 || section == 2 )  */
    
    /* Done reading this line and filling CList. */

           

    /* ------------------------------------------------------------------------- */
    /* check that we read 4 quantities with exactly the right format */
    if ( nread != 4 )
      {
	LALPrintError ("Found %d not %d values on line %d in file '%s'\n"
		       "Line in question is\n%s",
		       nread, 4, ic+1, fname, line);               
	LALFree ((*CList));
	free( uzpbuff.strptr );
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }

  } /*   for ( line = uzpbuff.strptr; section < MAX_SECS; ... ) */

  free( uzpbuff.strptr ); /* uzpbuff is allocated by malloc() in getfile(). It is user's responsibility to free this. */


  if (ic != (*candlen) + numlines ) {
    LALPrintError(
            "Read of file %s terminated after %d line but numlines=%d\n",
            fname, ic, numlines);
    LALFree((*CList));
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  if ( section != 3 ) {
    LALPrintError(
            "Read of file %s terminated not by coincidence section but %s\n",
            fname, line);
    LALFree((*CList));
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }


  (*candlen) += numlines; /* total number of candidate so far */


  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);


} /* void  ReadCandidateListFromZipFile () */
#endif /* #ifdef USE_UNZIP */



/* ########################################################################################## */
/*!
  Read one candidate-events file and fill CandidateList structure \b CList. 
  Count the number of the candidate events and fill it in \b candlen.
  The function aborts almost all the cases when the checks below failed.
  This function checks 
  \li if the file \b fname is readable. 
  \li if the file \b fname has the correct ending tag "%DONE".
  \li if the ranges of the values in the file are sensible.
  \li if the number of each row of the file is correct.
  \li if we could read all the events in the file.

  This function prints the bytecounts and the checksum of the file \b fname.

  @param[in,out] lalStatus LALStatus* 
  @param[out]    CList     CandidateList** CandidateList str to be filled in this code 
  @param[in]     fname     CHAR* the name of the file to be read
  @param[in,out]    candlen   UINT4* total number of the candidate events
*/
void  
ReadOneCandidateFileV2( LALStatus *lalStatus, 
		      CandidateList **CList, 
		      const CHAR *fname, 
		      INT4 *candlen )
{
  UINT4 i;
  UINT4 numlines;
  REAL8 epsilon=1e-5;
  CHAR line1[256];
  FILE *fp;
  INT4 nread;
  UINT4 checksum=0;
  UINT4 bytecount=0;


  INITSTATUS( lalStatus, "ReadOneCandidateFileV2", rcsid );
  ATTATCHSTATUSPTR (lalStatus);
  ASSERT( fname != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);


  /* ------ Open and count candidates file ------ */
  i=0;
  fp=fopen(fname,"rb");
  if (fp==NULL) 
    {
      LALPrintError("File %s doesn't exist!\n",fname);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
     }
  while(fgets(line1,sizeof(line1),fp)) {
    UINT4 k;
    size_t len=strlen(line1);

    /* check that each line ends with a newline char (no overflow of
       line1 or null chars read) */
    if (!len || line1[len-1] != '\n') {
      LALPrintError(
              "Line %d of file %s is too long or has no NEWLINE.  First 255 chars are:\n%s\n",
              i+1, fname, line1);
      fclose(fp);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
     }

    /* increment line counter */
    i++;

    /* maintain a running checksum and byte count */
    bytecount+=len;
    for (k=0; k<len; k++)
      checksum+=(INT4)line1[k];
  }
  numlines=i;
  /* -- close candidate file -- */
  fclose(fp);     

  if ( numlines == 0) 
    {
      LALPrintError ("ERROR: File '%s' has no lines so is not properly terminated by: %s", fname, DONE_MARKER);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }

  /* output a record of the running checksun amd byte count */
  LALPrintError( "%% %s: bytecount %" LAL_UINT4_FORMAT " checksum %" LAL_UINT4_FORMAT "\n", fname, bytecount, checksum);

  /* check validity of this Fstats-file */
  if ( strcmp(line1, DONE_MARKER ) ) 
    {
      LALPrintError ("ERROR: File '%s' is not properly terminated by: %sbut has %s instead", fname, DONE_MARKER, line1);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }
  else
    numlines --;        /* avoid stepping on DONE-marker */



  if( numlines == 0 ) { /* This file is empty. Go to the next file.*/
    if( lalDebugLevel > 1 ) {
      LALPrintError( "No candidate events in the file %s\n\n", fname);
    }
    DETATCHSTATUSPTR (lalStatus);
    RETURN (lalStatus);
  } 

#if 0 /* Do we need to check this for UINT4? */
  if ( numlines < 0  )
    {
      LALPrintError("candidate length overflow (or indeed negative) = %ud!\n",numlines);
      exit(POLKA_EXIT_ERR);
    }/* check that we have candidates. */
#endif


  /* ------------------------------------------------------------------------- */
  /* reserve memory for fstats-file contents */
  if ( numlines > 0) 
    { 
      CandidateList *tmp;
      tmp = (CandidateList *)LALRealloc (*CList, ( *candlen + numlines )*sizeof(CandidateList));
      if ( !tmp ) 
	{ 
	  LALPrintError("Could not allocate memory for candidate file %s\n\n", fname);
	  ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
	}
      *CList = tmp;
    }

  

  /* ------ Open and count candidates file ------ */
  i=0; /* append the new candidate events to the existing list. */
  fp=fopen(fname,"rb");
  if (fp==NULL) 
    {
      LALPrintError("fopen(%s) failed!\n", fname);
      LALFree ((*CList));
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }
  while(i < numlines && fgets(line1,sizeof(line1),fp))
    {
      CHAR newline='\0';
      CandidateList *cl=&(*CList)[i+(*candlen)];

      if (strlen(line1)==0 || line1[strlen(line1)-1] != '\n') {
        LALPrintError(
                "Line %d of file %s is too long or has no NEWLINE.  First 255 chars are:\n%s\n",
                i+1, fname, line1);
        LALFree ((*CList));
        fclose(fp);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
      
      nread = sscanf (line1, 
                     "%" LAL_INT4_FORMAT " %" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT 
                     " %" LAL_REAL8_FORMAT "%c", 
                     &(cl->FileID), &(cl->f), &(cl->Alpha), &(cl->Delta), &(cl->F1dot), &(cl->TwoF), &newline );

      /* check that values that are read in are sensible */
      if (
          cl->FileID < 0                     ||
          cl->f < 0.0                        ||
          cl->TwoF < 0.0                     ||
          cl->Alpha <         0.0 - epsilon  ||
          cl->Alpha >   LAL_TWOPI + epsilon  ||
          cl->Delta < -0.5*LAL_PI - epsilon  ||
          cl->Delta >  0.5*LAL_PI + epsilon  ||
	  !finite(cl->FileID)                ||                                                                 
          !finite(cl->f)                     ||
          !finite(cl->Alpha)                 ||
	  !finite(cl->F1dot)                 ||
          !finite(cl->Delta)                 ||
          !finite(cl->TwoF)
          ) {
          LALPrintError(
                  "Line %d of file %s has invalid values.\n"
                  "First 255 chars are:\n"
                  "%s\n"
                  "1st and 4th field should be positive.\n" 
                  "2nd field should lie between 0 and %1.15f.\n" 
                  "3rd field should lie between %1.15f and %1.15f.\n"
                  "All fields should be finite\n",
                  i+1, fname, line1, (double)LAL_TWOPI, (double)-LAL_PI/2.0, (double)LAL_PI/2.0);
          LALFree ((*CList));
          fclose(fp);
	  ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
           
           

      /* check that the FIRST character following the Fstat value is a
         newline.  Note deliberate LACK OF WHITE SPACE char before %c
         above */
      if (newline != '\n') {
        LALPrintError(
                "Line %d of file %s had extra chars after F value and before newline.\n"
                "First 255 chars are:\n"
                "%s\n",
                i+1, fname, line1);
        LALFree ((*CList));
        fclose(fp);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }

      /* check that we read 7 quantities with exactly the right format */
      if ( nread != 7 )
        {
          LALPrintError ("Found %d not %d values on line %d in file '%s'\n"
                         "Line in question is\n%s",
                         nread, 7, i+1, fname, line1);               
          LALFree ((*CList));
          fclose(fp);
	  ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
        }



      i++;
    } /*  end of main while loop */
  /* check that we read ALL lines! */
  if (i != numlines) {
    LALPrintError(
            "Read of file %s terminated after %d line but numlines=%d\n",
            fname, i, numlines);
    LALFree((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* read final line with %DONE\n marker */
  if (!fgets(line1, sizeof(line1), fp)) {
    LALPrintError(
            "Failed to find marker line of file %s\n",
            fname);
    LALFree((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* check for %DONE\n marker */
  if (strcmp(line1, DONE_MARKER)) {
    LALPrintError(
            "Failed to parse marker: 'final' line of file %s contained %s not %s",
            fname, line1, DONE_MARKER);
    LALFree ((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* check that we are now at the end-of-file */
  if (fgetc(fp) != EOF) {
    LALPrintError(
            "File %s did not terminate after %s",
            fname, DONE_MARKER);
    LALFree ((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* -- close candidate file -- */
  fclose(fp);     


  (*candlen) += numlines; /* total number of candidate so far */


  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);

} /* ReadOneCandidateFileV2() */





/* ########################################################################################## */
/*!
  Read one candidate-events file and fill CandidateList structure \b CList. 
  Count the number of the candidate events and fill it in \b candlen.
  The function aborts almost all the cases when the checks below failed.
  This function checks 
  \li if the file \b fname is readable. 
  \li if the file \b fname has the correct ending tag "%DONE".
  \li if the ranges of the values in the file are sensible.
  \li if the number of each row of the file is correct.
  \li if we could read all the events in the file.

  This function prints the bytecounts and the checksum of the file \b fname.

  @param[in,out] lalStatus LALStatus* 
  @param[out]    CList     CandidateList** CandidateList str to be filled in this code 
  @param[in]     fname     CHAR* the name of the file to be read
  @param[out]    candlen   UINT4* total number of the candidate events
*/
/* void  
ReadOneCandidateFile( LALStatus *lalStatus, 
		      CandidateList **CList, 
		      const CHAR *fname, INT4 *candlen, INT4 *candilenFthr,
		      const REAL8 myFthr ) */
void  
ReadOneCandidateFile( LALStatus *lalStatus, 
		      CandidateList **CList, 
		      const CHAR *fname, 
		      PolkaConfigVars *CLA,
		      INT4 *candlen, 
		      const REAL8 myFthr )
{
  UINT4 i;
  UINT4 numlines;
  REAL8 epsilon=1e-5;
  CHAR line1[256];
  FILE *fp;
  INT4 nread;
  UINT4 checksum=0;
  UINT4 bytecount=0;
  UINT4 numlinesFthr=0;


  INITSTATUS( lalStatus, "ReadOneCandidateFile", rcsid );
  ATTATCHSTATUSPTR (lalStatus);
  ASSERT( fname != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);
  ASSERT( *CList == NULL, lalStatus, POLKAC_ENONULL, POLKAC_MSGENONULL);

  /* ------ Open and count candidates file ------ */
  i=0;
  fp=fopen(fname,"rb");
  if (fp==NULL) 
    {
      LALPrintError("File %s doesn't exist!\n",fname);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
     }
  while(fgets(line1,sizeof(line1),fp)) {
    UINT4 k;
    size_t len=strlen(line1);

    /* check that each line ends with a newline char (no overflow of
       line1 or null chars read) */
    if (!len || line1[len-1] != '\n') {
      LALPrintError(
              "Line %d of file %s is too long or has no NEWLINE.  First 255 chars are:\n%s\n",
              i+1, fname, line1);
      fclose(fp);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
     }

    /* increment line counter */
    i++;
    
    /* maintain a running checksum and byte count */
    bytecount+=len;
    for (k=0; k<len; k++)
      checksum+=(INT4)line1[k];
  }
  numlines=i;
  /* -- close candidate file -- */
  fclose(fp);     

  if ( numlines == 0) 
    {
      LALPrintError ("ERROR: File '%s' has no lines so is not properly terminated by: %s", fname, DONE_MARKER);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }

  /* output a record of the running checksun amd byte count */
  LALPrintError( "%% %s: bytecount %" LAL_UINT4_FORMAT " checksum %" LAL_UINT4_FORMAT "\n", fname, bytecount, checksum);

  /* check validity of this Fstats-file */
  if ( strcmp(line1, DONE_MARKER ) ) 
    {
      LALPrintError ("ERROR: File '%s' is not properly terminated by: %sbut has %s instead", fname, DONE_MARKER, line1);
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }
  else
    numlines --;        /* avoid stepping on DONE-marker */

  *candlen=numlines;

#if 0 /* Do we need to check this? */
  if (*candlen <= 0  )
    {
      LALPrintError("candidate length = %ud!\n",*candlen);
      exit(POLKA_EXIT_ERR);;
    }/* check that we have candidates. */
#endif

  
  /* reserve memory for fstats-file contents */
  if (numlines > 0) 
    { 
      *CList = (CandidateList *)LALMalloc (numlines*sizeof(CandidateList));
      if ( !CList ) 
        { 
          LALPrintError ("Could not allocate memory for candidate file %s\n\n", fname);
	  ABORT (lalStatus, POLKAC_EMEM, POLKAC_MSGEMEM);
        }
    }

  /* ------ Open and count candidates file ------ */
  i=0;
  fp=fopen(fname,"rb");
  if (fp==NULL) 
    {
      LALPrintError("fopen(%s) failed!\n", fname);
      LALFree ((*CList));
      ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
    }
  while(i < numlines && fgets(line1,sizeof(line1),fp))
    {
      CHAR newline='\0';
      CandidateList *cl=&(*CList)[i];

      if (strlen(line1)==0 || line1[strlen(line1)-1] != '\n') {
        LALPrintError(
                "Line %d of file %s is too long or has no NEWLINE.  First 255 chars are:\n%s\n",
                i+1, fname, line1);
        LALFree ((*CList));
        fclose(fp);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
      
      nread = sscanf (line1, 
                     "%" LAL_INT4_FORMAT "%" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT " %" LAL_REAL8_FORMAT 
                     " %" LAL_REAL8_FORMAT "%c", 
                     &(cl->FileID), &(cl->f), &(cl->Alpha), &(cl->Delta), &(cl->F1dot), &(cl->TwoF), &newline );

      /* keep track of minimum values */
      if ( cl->f < CLA->fMIN){
	CLA->fMIN = cl->f;
      }
      
      if ( cl->F1dot < CLA->F1dotMIN){
	CLA->F1dotMIN = cl->F1dot;
      }
      
      if ( cl->Alpha < CLA->AlphaMIN){
	CLA->AlphaMIN = cl->Alpha;
      }
      
      if ( cl->Delta < CLA->DeltaMIN){
	CLA->DeltaMIN = cl->Delta;
      }
      
      if ( cl->FileID < CLA->FileIDMIN){
	CLA->FileIDMIN = cl->FileID;
      }
      


      /* find number of candidates that are above the 2F threshold. */
      if ( cl->TwoF > myFthr ) {
	numlinesFthr++;
      }

      /* check that values that are read in are sensible */
      if (
          cl->FileID < 0                     ||
          cl->f < 0.0                        ||
          cl->TwoF < 0.0                     ||
          cl->Alpha <         0.0 - epsilon  ||
          cl->Alpha >   LAL_TWOPI + epsilon  ||
          cl->Delta < -0.5*LAL_PI - epsilon  ||
          cl->Delta >  0.5*LAL_PI + epsilon  ||
	  !finite(cl->FileID)                ||                                                                 
          !finite(cl->f)                     ||
          !finite(cl->Alpha)                 ||
          !finite(cl->Delta)                 ||
	  !finite(cl->F1dot)                 ||
	  !finite(cl->TwoF)
          ) {
          LALPrintError(
                  "Line %d of file %s has invalid values.\n"
                  "First 255 chars are:\n"
                  "%s\n"
                  "1st and 4th field should be positive.\n" 
                  "2nd field should lie between 0 and %1.15f.\n" 
                  "3rd field should lie between %1.15f and %1.15f.\n"
                  "All fields should be finite\n",
                  i+1, fname, line1, (double)LAL_TWOPI, (double)-LAL_PI/2.0, (double)LAL_PI/2.0);
          LALFree ((*CList));
          fclose(fp);
	  ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }
           
           

      /* check that the FIRST character following the Fstat value is a
         newline.  Note deliberate LACK OF WHITE SPACE char before %c
         above */
      if (newline != '\n') {
        LALPrintError(
                "Line %d of file %s had extra chars after F value and before newline.\n"
                "First 255 chars are:\n"
                "%s\n",
                i+1, fname, line1);
        LALFree ((*CList));
        fclose(fp);
	ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
      }

      /* check that we read 7 quantities with exactly the right format */
      if ( nread != 7 )
        {
          LALPrintError ("Found %d not %d values on line %d in file '%s'\n"
                         "Line in question is\n%s",
                         nread, 7, i+1, fname, line1);               
          LALFree ((*CList));
          fclose(fp);
	  ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
        }



      i++;
    } /*  end of main while loop */

  /* number of candidates above the 2F threshold. */
  /*  *candilenFthr = numlinesFthr; */

  /* check that we read ALL lines! */
  if (i != numlines) {
    LALPrintError(
            "Read of file %s terminated after %d line but numlines=%d\n",
            fname, i, numlines);
    LALFree((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* read final line with %DONE\n marker */
  if (!fgets(line1, sizeof(line1), fp)) {
    LALPrintError(
            "Failed to find marker line of file %s\n",
            fname);
    LALFree((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* check for %DONE\n marker */
  if (strcmp(line1, DONE_MARKER)) {
    LALPrintError(
            "Failed to parse marker: 'final' line of file %s contained %s not %s",
            fname, line1, DONE_MARKER);
    LALFree ((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* check that we are now at the end-of-file */
  if (fgetc(fp) != EOF) {
    LALPrintError(
            "File %s did not terminate after %s",
            fname, DONE_MARKER);
    LALFree ((*CList));
    fclose(fp);
    ABORT (lalStatus, POLKAC_EINVALIDFSTATS, POLKAC_MSGEINVALIDFSTATS);
  }

  /* -- close candidate file -- */
  fclose(fp);     

  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);

} /* ReadOneCandidateFile() */







/* ########################################################################################## */
/*!
  Read command line arguments and fill a PolkaConfigVars structure \b CLA. 
  Almost all failures in this code invoke an exit of the code.
  This function checks 
  \li if a user has glob and specify \b input data dir.
  \li if a user specified either \b input data file or \b input data dir but not both.
  \li if a user did not define USE_UNZIP but specify \b input data dir.

  @param[in,out] lalStatus LALStatus* 
  @param[in]     argc      INT4  
  @param[in]     argv[]    CHAR* 
  @param[out]    CLA       PolkaConfigVars* Configuration variables
*/
void 
ReadCommandLineArgs( LALStatus *lalStatus, 
		     INT4 argc, 
		     CHAR *argv[], 
		     PolkaConfigVars *CLA ) 
{

  CHAR* uvar_InputData;
  CHAR* uvar_OutputData;

  CHAR* uvar_InputDirectory;
  CHAR* uvar_BaseName;

  BOOLEAN uvar_AutoOut;
  INT4 uvar_Nthr;      
  REAL8 uvar_Sthr;      
  REAL8 uvar_TwoFthr;

  REAL8 uvar_FreqWindow;
  REAL8 uvar_AlphaWindow;
  REAL8 uvar_DeltaWindow;
  REAL8 uvar_Kappa;
  REAL8 uvar_F1dotWindow;
  REAL8 uvar_FreqShift;
  REAL8 uvar_AlphaShift;
  REAL8 uvar_DeltaShift;
  REAL8 uvar_F1dotShift;
  BOOLEAN uvar_help;
  BOOLEAN uvar_UseUnzip;

  const CHAR BNAME[] = "Test";


  INITSTATUS( lalStatus, "ReadCommandLineArgs", rcsid );
  ATTATCHSTATUSPTR (lalStatus);

  ASSERT( CLA != NULL, lalStatus, POLKAC_ENULL, POLKAC_MSGENULL);


  uvar_AutoOut = 0;
  uvar_help = 0;

  uvar_InputData = NULL;
  uvar_OutputData = NULL;

  uvar_InputDirectory = NULL;
  uvar_BaseName = (CHAR*)LALCalloc (1, strlen(BNAME)+1);
  strcpy (uvar_BaseName, BNAME);

  /* The following numbers are arbitrary. */
  uvar_Nthr = 65536;     
  uvar_Sthr = 1.0e5; 
  uvar_TwoFthr = 0.0;

  uvar_FreqWindow = 0.0;
  uvar_AlphaWindow = 0.0;
  uvar_DeltaWindow = 0.0;
  uvar_F1dotWindow = 0.0;
  uvar_Kappa = 4.3;
  
  uvar_FreqShift = 0.0;
  uvar_AlphaShift = 0.0;
  uvar_DeltaShift = 0.0;
  uvar_F1dotShift = 0.0;

  uvar_UseUnzip = 0;


  /* register all our user-variables */
  LALregBOOLUserVar(lalStatus,       help,           'h', UVAR_HELP,     "Print this message"); 
  LALregBOOLUserVar(lalStatus,       UseUnzip,       'z', UVAR_OPTIONAL, "Use Unzip"); 

  LALregSTRINGUserVar(lalStatus,     OutputData,     'o', UVAR_REQUIRED, "Ouput candidates file name");

  LALregSTRINGUserVar(lalStatus,     InputData,      'I', UVAR_OPTIONAL, "Input candidates Fstats file.");
  LALregSTRINGUserVar(lalStatus,     InputDirectory, 'i', UVAR_OPTIONAL,"Input candidates Fstats files directory.");
  LALregSTRINGUserVar(lalStatus,     BaseName,       'b', UVAR_OPTIONAL,"BaseName of the Input Fstats files");

  LALregINTUserVar(lalStatus,        Nthr,            0,  UVAR_OPTIONAL, "Threshold on number of coincidence");
  LALregREALUserVar(lalStatus,       Sthr,            0,  UVAR_OPTIONAL, "Threshold on significance.");
  LALregBOOLUserVar(lalStatus,       AutoOut,         0,  UVAR_OPTIONAL, "Set Nthr and Sthr to print most significant cell only."); 

  LALregREALUserVar(lalStatus,       TwoFthr,         0,  UVAR_OPTIONAL, "Threshold on TwoF values for candidates.");

  LALregREALUserVar(lalStatus,       FreqWindow,     'f', UVAR_REQUIRED, "Frequency window in Hz");
  LALregREALUserVar(lalStatus,       F1dotWindow,    's', UVAR_REQUIRED, "First Spindown parameter window");
  LALregREALUserVar(lalStatus,       AlphaWindow,    'a', UVAR_REQUIRED, "Right ascension window in radians");
  LALregREALUserVar(lalStatus,       DeltaWindow,    'd', UVAR_REQUIRED, "Declination window in radians");
  LALregREALUserVar(lalStatus,       Kappa,          'k', UVAR_OPTIONAL, "Tuning parameter for declination window");

  LALregREALUserVar(lalStatus,       FreqShift,      'F', UVAR_OPTIONAL, "Frequency shift in FreqWindow");
  LALregREALUserVar(lalStatus,       F1dotShift,     'S', UVAR_OPTIONAL, "First Spindown shift in F1dotWindow");
  LALregREALUserVar(lalStatus,       AlphaShift,     'A', UVAR_OPTIONAL, "Right ascension shift in AlphaWindow");
  LALregREALUserVar(lalStatus,       DeltaShift,     'D', UVAR_OPTIONAL, "Declination shift in DeltaWindow");


  TRY (LALUserVarReadAllInput(lalStatus->statusPtr,argc,argv),lalStatus); 


  if (uvar_help) {	/* if help was requested, we're done here */
    LALPrintError("%s\n",rcsid);
    fflush(stderr);
    LALDestroyUserVars(lalStatus->statusPtr);
    exit(POLKA_EXIT_OK);
  }


  if( LALUserVarWasSet (&uvar_InputData) && 
      LALUserVarWasSet (&uvar_InputDirectory) ) {
    LALPrintError("\nCannot set both of InputData and InputDirectory\n");
    exit(POLKA_EXIT_ERR);
  }

  if( (!LALUserVarWasSet (&uvar_InputData)) && 
      (!LALUserVarWasSet (&uvar_InputDirectory)) ) {
    LALPrintError("\nPlease set either InputData and InputDirectory\n");
    exit(POLKA_EXIT_ERR);
  }


  if( uvar_UseUnzip ) {
#ifndef USE_UNZIP
    LALPrintError("\n unzip can be used only when compiling with unzip enabled.\n");
    exit(POLKA_EXIT_ERR);
#endif
  }

  CLA->UseUnzip = uvar_UseUnzip;

  CLA->FstatsFile = NULL;
  CLA->OutputFile = NULL;
  CLA->InputDir = NULL;
  CLA->BaseName = NULL;


  if( LALUserVarWasSet (&uvar_InputData) ) {
    CLA->FstatsFile = (CHAR *) LALMalloc(strlen(uvar_InputData)+1);
    if(CLA->FstatsFile == NULL)
      {
	LALPrintError("No candidates file specified; input with -I option.\n");
	LALPrintError("For help type %s -h\n", argv[0]);
	exit(POLKA_EXIT_ERR);
      }      
    strcpy(CLA->FstatsFile,uvar_InputData);
  }

  CLA->OutputFile = (CHAR *) LALMalloc(strlen(uvar_OutputData)+1);
  if(CLA->OutputFile == NULL)
    {
      TRY( FreeConfigVars( lalStatus->statusPtr, CLA ), lalStatus);
      exit(POLKA_EXIT_ERR);
    }      

  strcpy(CLA->OutputFile,uvar_OutputData);

  if( LALUserVarWasSet (&uvar_InputDirectory) ) {
#ifndef HAVE_GLOB_H   
    LALPrintError("Sorry, but you cannot use this feature without glob.h.\n");
    exit(POLKA_EXIT_ERR);
#endif
    CLA->InputDir = (CHAR *) LALMalloc(strlen(uvar_InputDirectory)+1);
    if(CLA->InputDir == NULL)
      {
	TRY( FreeConfigVars( lalStatus->statusPtr, CLA ), lalStatus);
	exit(POLKA_EXIT_ERR);
      }          
    strcpy(CLA->InputDir,uvar_InputDirectory);
  }



  CLA->BaseName = (CHAR *) LALMalloc(strlen(uvar_BaseName)+1);
  if(CLA->BaseName == NULL)
    {
      TRY( FreeConfigVars( lalStatus->statusPtr, CLA ), lalStatus);
      exit(POLKA_EXIT_ERR);
    }          
  strcpy(CLA->BaseName,uvar_BaseName);


  CLA->AutoOut = uvar_AutoOut;
  CLA->Nthr = uvar_Nthr;
  CLA->Sthr = uvar_Sthr;
  CLA->TwoFthr = uvar_TwoFthr;

  CLA->Deltaf = uvar_FreqWindow;
  CLA->DeltaAlpha = uvar_AlphaWindow;
  CLA->DeltaDelta = uvar_DeltaWindow;
  CLA->DeltaF1dot = uvar_F1dotWindow;
  CLA->Kappa = uvar_Kappa;

  CLA->Shiftf = uvar_FreqShift;
  CLA->ShiftAlpha = uvar_AlphaShift;
  CLA->ShiftDelta = uvar_DeltaShift;
  CLA->ShiftF1dot = uvar_F1dotShift;

  LALDestroyUserVars(lalStatus->statusPtr);
  BEGINFAIL(lalStatus) {
    LALFree(CLA->FstatsFile);
    LALFree(CLA->OutputFile);
  } ENDFAIL(lalStatus);

  DETATCHSTATUSPTR (lalStatus);
  RETURN (lalStatus);
} /* void ReadCommandLineArgs()  */

