/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of BUFF-EM.
 *
 * BUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * BUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * SWGGeometry.cc -- implementation of some methods in the SWGGeometry class
 *
 * homer reid      -- 5/2014 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <fenv.h>

#include <libhrutil.h>

#include "libbuff.h"

using namespace scuff;
namespace buff{

#define MAXTOK 50

/***************************************************************/
/* initialization of static class variables                    */
/***************************************************************/
double SWGGeometry::TaylorDuffyTolerance=1.0e-6;
int SWGGeometry::MaxTaylorDuffyEvals=10000;

/***********************************************************************/
/* parser subroutine for OBJECT...ENDOBJECT section in file ************/
/***********************************************************************/
SWGVolume *ParseObjectSection(FILE *f, int *LineNum, char *Label, char **pErrMsg)
{
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  char Line[MAXSTR];
  char *MeshFileName=0;
  char *MatFileName=0;
  GTransformation *OTGT=0;
  bool IsMatProp=false;
  while( fgets(Line,MAXSTR,f) )
   { 
     (*LineNum)++;

     /*--------------------------------------------------------------*/
     /*- break up line into tokens; skip blank lines and comments ---*/
     /*--------------------------------------------------------------*/
     char *Tokens[MAXTOK];
     int NumTokens=Tokenize(Line, Tokens, MAXTOK);
     if ( NumTokens==0 || Tokens[0][0]=='#' )
      continue; 

     /*--------------------------------------------------------------*/
     /*- MESHFILE keyword -------------------------------------------*/
     /*--------------------------------------------------------------*/
     if ( !StrCaseCmp(Tokens[0],"MESHFILE") )
      { if (NumTokens!=2)
         { *pErrMsg=strdupEC("MESHFILE keyword requires one argument");
           return 0;
         };
        MeshFileName=strdupEC(Tokens[1]);
      }

     /*--------------------------------------------------------------*/
     /*- MATERIAL keyword -------------------------------------------*/
     /*--------------------------------------------------------------*/
     else if (    !StrCaseCmp(Tokens[0],"MATERIAL")
               || !StrCaseCmp(Tokens[0],"SVTENSOR")
             )
      { if (NumTokens!=2)
         { *pErrMsg=vstrdup("%s keyword requires one argument",Tokens[0]);
           return 0;
         };
        MatFileName=strdupEC(Tokens[1]);
        IsMatProp=(!StrCaseCmp(Tokens[0],"MATERIAL"));
      }

     /*--------------------------------------------------------------*/
     /*- DISPLACED / ROTATED keywords -------------------------------*/
     /*--------------------------------------------------------------*/
     else if ( !StrCaseCmp(Tokens[0],"DISPLACED") || !StrCaseCmp(Tokens[0],"ROTATED") )
      { 
        // try to parse the line as a geometrical transformation.
        // note that OTGT is used as a running GTransformation that may
        // be augmented by multiple DISPLACED ... and/or ROTATED ...
        // lines within the OBJECT...ENDOBJECT or SURFACE...ENDSURFACE section, 
        // and which is applied to the object at its birth, then
        // subsequently stored as the OTGT field in the SWGVolume class.
        int TokensConsumed;
        if (OTGT)
	 { GTransformation *OTGT2 = new GTransformation(Tokens, NumTokens, pErrMsg, &TokensConsumed);
           if (*pErrMsg)
            return 0;

           OTGT->Transform(OTGT2);
           delete OTGT2;
	 }
        else
	 { OTGT = new GTransformation(Tokens, NumTokens, pErrMsg, &TokensConsumed);
           if (*pErrMsg)
            return 0;
	 };
        if (TokensConsumed!=NumTokens) 
         { *pErrMsg=strdupEC("junk at end of line");
           return 0;
         };
      } 
     /*--------------------------------------------------------------*/
     /*- ENDOBJECT keyword            -------------------------------*/
     /*--------------------------------------------------------------*/
     else if ( !StrCaseCmp(Tokens[0],"ENDOBJECT") )
      { 
        break; 
      }
     /*--------------------------------------------------------------*/
     /*- unrecognized keywords        -------------------------------*/
     /*--------------------------------------------------------------*/
     else
      { *pErrMsg=strdupEC("syntax error");
        return 0;
      };
      
   }; //while( fgets(Line,MAXSTR,f) )

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  if ( MeshFileName==0 )
   *pErrMsg = strdupEC("no MESHFILE specified for object");
  if ( MatFileName==0 )
   *pErrMsg = strdupEC("no MATERIAL or SVTENSOR specified for object");
  if (*pErrMsg) 
   return 0;
  
  return new SWGVolume(MeshFileName, Label, MatFileName, IsMatProp, OTGT);
 
}

/***************************************************************/
/* 20150818 convenience hack to support special file names of  */
/* the following forms                                         */
/*  MESH__MyMesh                                               */
/*  MESH__MyMesh__MAT__MatProp                                 */
/*  MESH__MyMesh__SVT__SVTName                                 */
/***************************************************************/
bool ProcessSpecialBUFFGeoFileName(const char *GeoFileName)
{
  // check for a special geometry file name, which
  //  begins with MESH__
  int N=strlen(GeoFileName);
  if ( N<7 || strncmp(GeoFileName,"MESH__",6) )
   return false;

  char GFNCopy[MAXSTR];
  strncpy(GFNCopy, GeoFileName+6, MAXSTR);

  char *MeshStr=GFNCopy;
  char *MatStr=strstr(GFNCopy,"__MAT__");
  char *SVTStr=strstr(GFNCopy,"__SVT__");

  if (MatStr)
   { *MatStr=0;
     MatStr+=7;
   };
  if (SVTStr)
   { *SVTStr=0;
     SVTStr+=7;
   };

  Log("Processing special file name: mesh=%s",MeshStr);
  if (MatStr)
   Log("           special file name: mat=%s",MatStr);
  if (SVTStr)
   Log("           special file name: svt=%s",SVTStr);

  FILE *f=vfopen("%s.buffgeo","w",GeoFileName);
  fprintf(f,"OBJECT %s\n",GetFileBase(MeshStr));
  fprintf(f," MESHFILE %s.vmsh\n",GetFileBase(MeshStr));
  if (MatStr) 
   fprintf(f," MATERIAL %s\n",MatStr);
  if (SVTStr)
   fprintf(f," SVTENSOR %s.SVTensor\n",SVTStr);
  fprintf(f,"ENDOBJECT\n");
  fclose(f);

  return true;

}

/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
SWGGeometry::SWGGeometry(const char *pGeoFileName)
{ 
  /***************************************************************/
  /* initialize simple fields ************************************/
  /***************************************************************/
  GeoFileName=strdup(pGeoFileName);
  NumObjects=0;
  Objects=0;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  char *s;
  if ( (s=getenv("BUFF_TAYLORDUFFY_EVALS")) )
   { sscanf(s,"%i",&MaxTaylorDuffyEvals);
     Log("Setting max TaylorDuffy evals=%i.",MaxTaylorDuffyEvals);
   };
  if ( (s=getenv("BUFF_TAYLORDUFFY_TOLERANCE")) )
   { sscanf(s,"%le",&TaylorDuffyTolerance);
     Log("Setting TaylorDuffy tolerance=%e.",TaylorDuffyTolerance);
   };

  /***************************************************************/
  /* try to open input file **************************************/
  /***************************************************************/
  if (ProcessSpecialBUFFGeoFileName(pGeoFileName))
   GeoFileName=vstrappend(GeoFileName,".buffgeo");

  FILE *f=fopen(GeoFileName,"r");
  if (!f)
   ErrExit("could not open %s",GeoFileName);

  /***************************************************************/
  /* read and process lines from input file one at a time        */
  /***************************************************************/
  char Line[MAXSTR];
  int LineNum=0; 
  while( fgets(Line,MAXSTR,f) )
   { 
     LineNum++;

     /*--------------------------------------------------------------*/
     /*- break up line into tokens; skip blank lines and comments ---*/
     /*--------------------------------------------------------------*/
     char *Tokens[MAXTOK];
     int nTokens=Tokenize(Line, Tokens, MAXTOK);
     if ( nTokens==0 || Tokens[0][0]=='#' )
      continue; 
    
     /*--------------------------------------------------------------*/
     /*- switch off based on first token on the line ----------------*/
     /*--------------------------------------------------------------*/
     if ( !StrCaseCmp(Tokens[0],"MESHPATH") )
      { 
        if ( nTokens!=2 )
         ErrExit("%s:%i: invalid MESHPATH specification",GeoFileName,LineNum);  
        char *OldPath=getenv("BUFF_MESH_PATH");
        char *NewPath=vstrappend( OldPath, ":%s",Tokens[1] );
        setenv("BUFF_MESH_PATH", NewPath, 1);
      }
     else if ( !StrCaseCmp(Tokens[0],"OBJECT") )
      { 
        if (nTokens!=2)
         ErrExit("%s:%i: invalid OBJECT specification",GeoFileName,LineNum);

        char *ErrMsg=0;
        SWGVolume *O = ParseObjectSection(f, &LineNum, Tokens[1], &ErrMsg);
        if (ErrMsg)
         ErrExit("%s:%i: %s",GeoFileName,LineNum,ErrMsg);

        NumObjects++;
        Objects=(SWGVolume **)realloc(Objects, NumObjects*sizeof(SWGVolume *) );
        Objects[NumObjects-1]=O;
        O->Index=NumObjects-1;
      }
     else if ( !StrCaseCmp(Tokens[0],"MATERIAL") )
      {
        /*--------------------------------------------------------------*/
        /* hand off to MatProp class constructor to parse this section  */
        /*--------------------------------------------------------------*/
        if ( nTokens==1 )
         ErrExit("%s:%i: no name given for MATERIAL ",GeoFileName,LineNum);
        else if ( nTokens>2 )
         ErrExit("%s:%i: syntax error",GeoFileName,LineNum);
         
        char *ErrMsg=AddMaterialToMatPropDataBase(f, GeoFileName, Tokens[1], &LineNum);
        if (ErrMsg)
         ErrExit("%s:%i: %s",GeoFileName,LineNum,ErrMsg); 
      }
     else 
      { 
        /*--------------------------------------------------------------*/
        /* unknown keyword                                              */
        /*--------------------------------------------------------------*/
        ErrExit("%s:%i: syntax error",GeoFileName,LineNum);
      };

   }; // while( fgets(Line,MAXSTR,f) )

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  BFIndexOffset=(int *)malloc(NumObjects * sizeof(int));
  BFIndexOffset[0]=TotalBFs=0;
  for(int no=0; no<NumObjects; no++)
   { 
     int NumBFs = Objects[no]->NumInteriorFaces;
     TotalBFs += NumBFs; 
     if ( no < (NumObjects-1) )
      BFIndexOffset[no+1]=BFIndexOffset[no] + NumBFs;
   };

  /***************************************************************/
  /* initialize Mate[] array.                                    */
  /*                                                             */
  /* how it works:                                               */
  /*                                                             */
  /* (1) two objects are considered identical if they are        */
  /*     described by the same mesh file and the same material   */
  /*     file.                                                   */
  /*                                                             */
  /* (2) Mate[] array: If surfaces i, j, k, ... are identical and*/
  /*                   i<j<k<..., then we set                    */
  /*                   Mate[i] = -1                              */
  /*                   Mate[j] = i                               */
  /*                   Mate[k] = i                               */
  /***************************************************************/
  Mate=(int *)mallocEC(NumObjects*sizeof(int));
  Mate[0]=-1;
  for(int no=1; no<NumObjects; no++)
   { SWGVolume *O=Objects[no];
     Mate[no]=-1;
     for(int nop=0; nop<no && Mate[no]==-1; nop++)
      { SWGVolume *OP=Objects[nop];
        if (    !strcmp(O->MeshFileName, OP->MeshFileName)
             && !strcmp(O->MatFileName,  OP->MatFileName)
           )
         { Mate[no]=nop;
           Log("Noting that object %i (%s) is a duplicate of object %i (%s)...",
                no,O->Label,nop,OP->Label);
         };
      };
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  ObjectGCaches  = (FIBBICache **)mallocEC(NumObjects * sizeof(FIBBICache *));

}

/***************************************************************/
/* SWGGeometry class destructor *******************************/
/***************************************************************/
SWGGeometry::~SWGGeometry()
{
  for(int no=0; no<NumObjects; no++)
   delete Objects[no];
  free(Objects);
  free(BFIndexOffset);
  free(Mate);
  free(GeoFileName);

}

/***************************************************************/
/* return the SWGVolume whose label is Label. if pno is        */
/* non-NULL on entry, then on return it is set to the index of */
/* the SWGVolume. If no corresponding SWGVolume was found,     */
/* the return value is NULL and *pno is set to -1.             */
/***************************************************************/
SWGVolume *SWGGeometry::GetObjectByLabel(const char *Label, int *pno)
{
  if (pno) *pno=-1;
 
  if (Label==0)
   return NULL;
  
  for(int no=0; no<NumObjects; no++)
   if ( !StrCaseCmp(Label, Objects[no]->Label) )
    { if (pno) *pno = no;
      return Objects[no];
    };

  return NULL;
}

/***************************************************************/
/* Apply the specified GTComplex to transform the geometry.    */
/* (Note that a 'GTComplex' is a list of GTransformations, each*/
/* of which is applied to one specific surface in the geometry.)*/
/***************************************************************/
void SWGGeometry::Transform(GTComplex *GTC)
{ 
  // loop over the individual transformations in the complex
  for(int nsa=0; nsa<GTC->NumSurfacesAffected; nsa++)
   { 
     // find the surface corresponding to the label for this transformation
     SWGVolume *O=GetObjectByLabel(GTC->SurfaceLabel[nsa]);

     // apply the transformation to that object
     if (O) 
      O->Transform(GTC->GT + nsa);
   };

}

/***************************************************************/
/* Undo transformations. ***************************************/
/***************************************************************/
void SWGGeometry::UnTransform()
{ 
  for(int no=0; no<NumObjects; no++)
   Objects[no]->UnTransform();
}

/***************************************************************/
/* Quick sanity check to make sure that a given list of        */
/* GTComplex structures actually makes sense for the given     */
/* geometry, which is to say that it doesn't request           */
/* transformations on any objects that don't exist in the      */
/* geometry.                                                   */
/* Returns 0 if the check passed, or an error message if not.  */
/***************************************************************/
char *SWGGeometry::CheckGTCList(GTComplex **GTCList, int NumGTCs)
{
  int ngtc, nsa;
  
  for(ngtc=0; ngtc<NumGTCs; ngtc++)
   for (nsa=0; nsa<GTCList[ngtc]->NumSurfacesAffected; nsa++)
    if (!GetObjectByLabel(GTCList[ngtc]->SurfaceLabel[nsa]))
     return vstrdup("transformation requested for unknown object %s",
                     GTCList[ngtc]->SurfaceLabel[nsa]);

  return 0;
}

} // namespace buff
