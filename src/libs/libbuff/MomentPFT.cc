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
 * MomentPFT.cc -- BUFF-EM code module to compute lowest-order
 *                 multipole contributions to power, force, and torque
 *
 * homer reid   -- 8/2015
 *
 */

#include <libscuff.h>
#include <libbuff.h>

#define II cdouble(0.0,1.0)

namespace scuff { 

void CalcGC(double R[3], cdouble Omega,
            cdouble EpsR, cdouble MuR,
            cdouble GMuNu[3][3], cdouble CMuNu[3][3],
            cdouble GMuNuRho[3][3][3], cdouble CMuNuRho[3][3][3]);
}

namespace buff {

inline cdouble VecHDot(cdouble *V1, cdouble *V2)
{ return conj(V1[0])*V2[0]+conj(V1[1])*V2[1]+conj(V1[2])*V2[2]; }

/***************************************************************/
/* compute the moments of a single unit-strength SWG basis     */
/* function.                                                   */
/* JMu[Mu]       = \int J_\Mu dV                               */
/* JMuNu[Mu][Nu] = \int J_\Mu (X_nu - X0_nu) dV                */
/***************************************************************/
void Get1BFMoments(SWGVolume *O, int nf,
                   double JMu[3], double JMuNu[3][3],
                   double *X0=0)
{
  SWGFace *F  = O->Faces[nf];
  double  A   = F->Area;
  double *QP  = O->Vertices + 3*(F->iQP);
  double *QM  = O->Vertices + 3*(F->iQM);
  
  for(int Mu=0; Mu<3; Mu++)
   JMu[Mu] = A*(QM[Mu] - QP[Mu]) / 4.0;

// note: a more compact formula for the following would be nice
  double *V1  = O->Vertices + 3*(F->iV1);
  double *V2  = O->Vertices + 3*(F->iV2);
  double *V3  = O->Vertices + 3*(F->iV3);
  double L1P[3], L2P[3], L3P[3], L1M[3], L2M[3], L3M[3];
  VecSub(V1, QP, L1P);
  VecSub(V2, QP, L2P);
  VecSub(V3, QP, L3P);
  VecSub(V1, QM, L1M);
  VecSub(V2, QM, L2M);
  VecSub(V3, QM, L3M);

  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    JMuNu[Mu][Nu] 
     = + A*(  L1P[Mu]*L1P[Nu] - L1M[Mu]*L1M[Nu]
            + L2P[Mu]*L2P[Nu] - L2M[Mu]*L2M[Nu]
            + L3P[Mu]*L3P[Nu] - L3M[Mu]*L3M[Nu]
           ) / 30.0
       + A*(  L1P[Mu]*L2P[Nu] - L1M[Mu]*L2M[Nu]
             +L1P[Mu]*L3P[Nu] - L1M[Mu]*L3M[Nu]
             +L2P[Mu]*L3P[Nu] - L2M[Mu]*L3M[Nu]
             +L1P[Nu]*L2P[Mu] - L1M[Nu]*L2M[Mu]
             +L1P[Nu]*L3P[Mu] - L1M[Nu]*L3M[Mu]
             +L2P[Nu]*L3P[Mu] - L2M[Nu]*L3M[Mu]
           ) /60.0
       + A*(  L1P[Mu]*QP[Nu]  - L1M[Mu]*QM[Nu]
            + L2P[Mu]*QP[Nu]  - L2M[Mu]*QM[Nu]
            + L3P[Mu]*QP[Nu]  - L3M[Mu]*QM[Nu]
           ) /12.0;

  if (X0)
   for(int Mu=0; Mu<3; Mu++)
    for(int Nu=0; Nu<3; Nu++)
     JMuNu[Mu][Nu] -= JMu[Mu]*X0[Nu];
}

/***************************************************************/
/* get the electric dipole, magnetic dipole, and electric      */
/* quadrupole moments of the induced current distribution      */
/***************************************************************/
void GetMoments(SWGGeometry *G, int no, cdouble Omega,
                HVector *JVector,
                cdouble p[3], cdouble m[3], cdouble Qp[3])
{
  // if the object has been transformed since it was read in
  // from the mesh file, make sure to shift appropriately the
  // origin about which we compute the torque
  SWGVolume *O=G->Objects[no];
  double *X0=0, X0Buffer[3]={0.0, 0.0, 0.0};
  if (O->GT || O->OTGT)
   { X0=X0Buffer;
     if (O->OTGT) O->OTGT->Apply(X0);
     if (O->GT  ) O->GT->Apply(X0);
   };

  p[0]=p[1]=p[2]=m[0]=m[1]=m[2]=0.0;
  cdouble Q[3][3]={{0.0,0.0,0.0}, {0.0,0.0,0.0}, {0.0,0.0,0.0}};
  cdouble iw=II*Omega;
  int Offset = G->BFIndexOffset[no];
  for(int n=0; n<O->NumInteriorFaces; n++)
   { 
     cdouble JAlpha = JVector->GetEntry(Offset + n);

     double JMu[3], JMuNu[3][3];
     Get1BFMoments(O, n, JMu, JMuNu, X0);

     cdouble JMMTrace=JAlpha*(JMuNu[0][0]+JMuNu[1][1]+JMuNu[2][2]);
     for(int Mu=0; Mu<3; Mu++)
      { 
        p[Mu] -= JAlpha*JMu[Mu] / iw;

        int MP1=(Mu+1)%3, MP2=(Mu+2)%3;
        m[Mu] -= 0.5*JAlpha*(JMuNu[MP1][MP2]-JMuNu[MP2][MP1]);

        Q[Mu][Mu] -= JAlpha*(6.0*JMuNu[Mu][Mu] - 2.0*JMMTrace)/iw;
        for(int Nu=Mu+1; Nu<3; Nu++)
         Q[Mu][Nu] -= 3.0*JAlpha*(JMuNu[Mu][Nu] + JMuNu[Nu][Mu])/iw;
      };
     Q[1][0]=Q[0][1];
     Q[2][0]=Q[0][2];
     Q[2][1]=Q[1][2];
   };

  Qp[0]=Qp[1]=Qp[2]=0.0;
  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    Qp[Mu] += Q[Mu][Nu] * p[Nu];

// TODO: need to rotate all vectors if the object has been
//       subject to a rotational transformation
// if (O->GT || O->OTGT) ...

}

/***************************************************************/
/* get the moment matrices MMuNu and MMuNuRho for a fluctuation-*/
/* induced current distribution described by a dressed DRMatrix   */
/* matrix DRMatrix.                                             */
/* if Workspace is non-null it must point to a buffer with     */
/* space for at least 12*N cdoubles, where N is the number of  */
/* basis functions on object #no.                              */
/***************************************************************/
void GetNEQMoments(SWGGeometry *G, int no, HMatrix *DRMatrix,
                   cdouble MMuNu[3][3], cdouble MMuNuRho[3][3][3],
                   cdouble *Workspace=0)
{
  SWGVolume *O      = G->Objects[no];
  int N             = O->NumInteriorFaces;

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  static int SavedN=0;
  static cdouble *SavedWorkspace=0;
  if ( Workspace==0 )
   { if (SavedN==N)
      Workspace = SavedWorkspace;
     else
      { if (SavedWorkspace) 
         free(SavedWorkspace);
        SavedN=N;
        SavedWorkspace = (cdouble *)mallocEC(12*N*sizeof(cdouble));
        Workspace = SavedWorkspace;
      };
   };
    
  int nVec=0;
  HVector *vMu[3], *vMuNu[3][3];
  for(int Mu=0; Mu<3; Mu++, nVec++)
   vMu[Mu] = new HVector(N, LHM_COMPLEX, Workspace + nVec*N);
  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++, nVec++)
    vMuNu[Mu][Nu] = new HVector(N, LHM_COMPLEX, Workspace + nVec*N);

  // if the object has been transformed since it was read in
  // from the mesh file, make sure to shift appropriately the
  // origin about which we compute the torque
  double *X0=0, X0Buffer[3]={0.0, 0.0, 0.0};
  if (O->GT || O->OTGT)
   { X0=X0Buffer;
     if (O->OTGT) O->OTGT->Apply(X0);
     if (O->GT  ) O->GT->Apply(X0);
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  Log("GNM Getting 1BF moments...");
  for(int n=0; n<N; n++)
   { 
     double JMu[3], JMuNu[3][3];
     Get1BFMoments(O, n, JMu, JMuNu, X0);

     for(int Mu=0; Mu<3; Mu++)
      vMu[Mu]->SetEntry(n, JMu[Mu]);

     for(int Mu=0; Mu<3; Mu++)
      for(int Nu=0; Nu<3; Nu++)
       vMuNu[Mu][Nu]->SetEntry(n, JMuNu[Mu][Nu]);
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  Log("GNM doing linear algebra...");

  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    MMuNu[Mu][Nu]=DRMatrix->BilinearProduct(vMu[Mu], vMu[Nu]);

  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    for(int Rho=0; Rho<3; Rho++)
     MMuNuRho[Mu][Nu][Rho]=DRMatrix->BilinearProduct(vMuNu[Mu][Rho], vMu[Nu]);

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  for(int Mu=0; Mu<3; Mu++)
   delete vMu[Mu];
  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    delete vMuNu[Mu][Nu];
  
}

/***************************************************************/
/* play the principal-moment-decomposition game to get the     */
/* p_a and m_a moments of a fluctuation-induced current        */
/* distribution from its moment matrices MMuNu and MuNuRho.    */
/***************************************************************/
void DoPrincipalAxisDecomposition(cdouble MMuNu[3][3], 
                                  cdouble MMuNuRho[3][3][3], 
                                  cdouble Omega,
                                  cdouble p[3][3], cdouble m[3][3])
{
  static HMatrix *ppMatrix = new HMatrix(3,3,LHM_COMPLEX);
  static HMatrix *mpMatrix = new HMatrix(3,3,LHM_COMPLEX);
  static HMatrix *U        = new HMatrix(3,3,LHM_COMPLEX);
  static HMatrix *VT       = new HMatrix(3,3,LHM_COMPLEX);
  static HVector *Lambda   = new HVector(3,LHM_REAL);
  static HVector *Sigma    = new HVector(3,LHM_REAL);

  double w2=real(Omega)*real(Omega);
  for(int Mu=0; Mu<3; Mu++)
   { ppMatrix->SetEntry(Mu, Mu, real(MMuNu[Mu][Mu])/w2);
     for(int Nu=Mu+1; Nu<3; Nu++)
      { ppMatrix->SetEntry(Mu, Nu, MMuNu[Mu][Nu]       / w2 );
        ppMatrix->SetEntry(Nu, Mu, conj(MMuNu[Mu][Nu]) / w2 );
      };
   };

  for(int Mu=0; Mu<3; Mu++)
   for(int Nu=0; Nu<3; Nu++)
    { int NP1=(Nu+1)%3, NP2=(Nu+2)%3;
      mpMatrix->SetEntry(Mu,Nu, (  MMuNuRho[NP2][Nu][NP1]
                                 - MMuNuRho[NP1][Nu][NP2]
                                ) / (II*Omega)
                        );
    };

  ppMatrix->Eig(Lambda, U);
  double MaxEig=abs(Lambda->GetEntry(0));
  MaxEig=fmax(MaxEig, abs(Lambda->GetEntry(1)));
  MaxEig=fmax(MaxEig, abs(Lambda->GetEntry(2)));

  for(int Mu=0.0; Mu<3.0; Mu++)
   if( abs(Lambda->GetEntry(Mu)) < 1.0e-8*MaxEig )
    Lambda->SetEntry(Mu, 0.0);

  for(int a=0; a<3; a++)
   for(int Mu=0; Mu<3; Mu++)
    p[a][Mu] = sqrt(Lambda->GetEntry(a))*U->GetEntry(Mu,a);

  mpMatrix->SVD(Sigma, U, VT);
  for(int a=0; a<3; a++)
   { 
     cdouble DotProd = conj(VT->GetEntry(a,0)) * p[a][0]
                      +conj(VT->GetEntry(a,1)) * p[a][1]
                      +conj(VT->GetEntry(a,2)) * p[a][2];
     cdouble LambdaA=Lambda->GetEntry(a);
     cdouble ScaleFactor
      = (LambdaA==0.0) ? 0.0 : Sigma->GetEntry(a) * DotProd/ LambdaA;
     m[a][0] = ScaleFactor*U->GetEntry(0,a);
     m[a][1] = ScaleFactor*U->GetEntry(1,a);
     m[a][2] = ScaleFactor*U->GetEntry(2,a);
   };

}

/***************************************************************/
/* if FileBase=0 then no .moments or .pp files are written     */
/***************************************************************/
void GetMomentPFT(SWGGeometry *G, int no, cdouble Omega,
                  IncField *IF, HVector *JVector, HMatrix *DRMatrix,
                  HMatrix *PFTMatrix, bool KeepQpTerm, 
                  char *FileBase)
{
  for(int nq=0; nq<NUMPFT; nq++)
   PFTMatrix->SetEntry(no, nq, 0.0);

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  // p[a][Mu]   = Muth component of ath principal p-vector
  // m[a][Mu]   = Muth component of ath principal m-vector
  cdouble p[3][3];
  cdouble m[3][3];
  cdouble Qp[3];  // Q*p, only used in the deterministic case
  int NumMoments; // = 1 for deterministic, 3 for neq
  if (JVector)
   { NumMoments=1;
     GetMoments(G, no, Omega, JVector, p[0], m[0], Qp);
   }
  else
   { 
     NumMoments=3;
     Log("GMP getting NEQ moments...");
     cdouble MMuNu[3][3], MMuNuRho[3][3][3];
     GetNEQMoments(G, no, DRMatrix, MMuNu, MMuNuRho);
     DoPrincipalAxisDecomposition(MMuNu, MMuNuRho, Omega, p, m);
   };

  /***************************************************************/
  /* add incident-field contributions ****************************/
  /***************************************************************/
  if (IF)
   { 
     // get fields and derivatives at body centroid by finite-differencing
     double X0[3]={0.0, 0.0, 0.0};
     SWGVolume *O=G->Objects[no];
     if (O->OTGT) O->OTGT->Apply(X0);
     if (O->GT)   O->GT->Apply(X0);
     cdouble EH[6], dEH[3][6];
     IF->GetFields(X0, EH);
     IF->GetFieldGradients(X0, dEH);
     for(IncField *IFNode=IF->Next; IFNode!=0; IFNode=IFNode->Next)
      { cdouble PartialEH[6], PartialdEH[3][6];
        IFNode->GetFields(X0, PartialEH);
        IFNode->GetFieldGradients(X0, PartialdEH);
        VecPlusEquals(EH,     1.0, PartialEH    , 6);
        VecPlusEquals(dEH[0], 1.0, PartialdEH[0], 6);
        VecPlusEquals(dEH[1], 1.0, PartialdEH[1], 6);
        VecPlusEquals(dEH[2], 1.0, PartialdEH[2], 6);
      };

     cdouble *p0=p[0];
     for(int Mu=0; Mu<3; Mu++)
      {
        PFTMatrix->AddEntry(no, PFT_PABS, 0.5*Omega*imag( conj(p0[Mu])*EH[Mu] ) );

        for(int Nu=0; Nu<3; Nu++)
         PFTMatrix->AddEntry(no, PFT_XFORCE+Nu, -0.5*real( conj(p0[Mu])*dEH[Nu][Mu] ) );

        int MP1=(Mu+1)%3, MP2=(Mu+2)%3;
        PFTMatrix->AddEntry(no, PFT_XTORQUE+Mu, -0.5*real( conj(p0[MP1])*EH[MP2] - conj(p0[MP2])*EH[MP1] ) );
      };

   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  double k3=real(Omega)*real(Omega)*real(Omega);
  double k4=real(Omega)*k3;
  double k5=real(Omega)*k4;
  double PPF  = k4*ZVAC/(12.0*M_PI);
  double FPF1 = -TENTHIRDS*k4*ZVAC/(12.0*M_PI);
  double FPF2 = -TENTHIRDS*k5*ZVAC/(120.0*M_PI);
  double TPF  = -TENTHIRDS*k3*ZVAC/(6.0*M_PI);

  PFTMatrix->Zero();
  for(int nm=0; nm<NumMoments; nm++)
   for(int Mu=0; Mu<3; Mu++)
    { 
      PFTMatrix->AddEntry(no, PFT_PABS,
                              PPF*real( conj(p[nm][Mu])*p[nm][Mu] )
                         );

      int MP1=(Mu+1)%3, MP2=(Mu+2)%3;
      PFTMatrix->AddEntry(no, PFT_XFORCE + Mu,
                              FPF1*real( conj(m[nm][MP1])*p[nm][MP2]
                                        -conj(m[nm][MP2])*p[nm][MP1] )
                         );

      PFTMatrix->AddEntry(no, PFT_XTORQUE + Mu,
                              TPF*( real(p[nm][MP1]) * imag(p[nm][MP2])
                                   -real(p[nm][MP2]) * imag(p[nm][MP1])
                                  )
                         );
    };

  if (JVector && KeepQpTerm)
   { PFTMatrix->AddEntry(no, PFT_XFORCE, FPF2*imag(Qp[0]) );
     PFTMatrix->AddEntry(no, PFT_YFORCE, FPF2*imag(Qp[1]) );
     PFTMatrix->AddEntry(no, PFT_ZFORCE, FPF2*imag(Qp[2]) );
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  if (FileBase==0)
   FileBase=getenv("BUFF_MOMENT_FILEBASE");

  if (FileBase)
   {
     FILE *f=vfopen("%s.Moments","a",FileBase);
     fprintf(f,"%e ",real(Omega));
     for(int a=0; a<NumMoments; a++)
      for(int Mu=0; Mu<3; Mu++)
       fprintf(f,"%e %e %e %e ",real(p[a][Mu]),imag(p[a][Mu]),
                                real(m[a][Mu]),imag(m[a][Mu]));
     fclose(f);
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  if (FileBase)
   {
     double Radius=0.0;
     SWGVolume *O=G->Objects[no];
     for(int nv=0; nv<O->NumVertices; nv++)
      Radius=fmax(Radius, VecNorm(O->Vertices + 3*nv));

     // compute scaling factors
     double pMax=0.0;
     double mMax=0.0;
     for(int a=0; a<NumMoments; a++)
      { pMax=fmax(pMax, sqrt( norm(p[a][0]) + norm(p[a][1]) + norm(p[a][2]) ) );
        mMax=fmax(mMax, sqrt( norm(m[a][0]) + norm(m[a][1]) + norm(m[a][2]) ) );
      };
     double pScale = 1.5*Radius/pMax;
     double mScale = 1.5*Radius/mMax;
  
     FILE *f=vfopen("%s.%g.Moments.pp","w",FileBase,real(Omega));
     for(int a=0; a<NumMoments; a++)
      { 
        cdouble *pa=p[a], *ma=m[a];

        fprintf(f,"View \"Real p%i_%g\" {\n",a+1,real(Omega));
        fprintf(f,"VP(0,0,0) {%e,%e,%e};\n",
                   pScale*real(pa[0]),
                   pScale*real(pa[1]),
                   pScale*real(pa[2]));
        fprintf(f,"};\n");

        fprintf(f,"View \"Imag p%i_%g\" {\n",a+1,real(Omega));
        fprintf(f,"VP(0,0,0) {%e,%e,%e};\n",
                   pScale*imag(pa[0]),
                   pScale*imag(pa[1]),
                   pScale*imag(pa[2]));
        fprintf(f,"};\n");

        fprintf(f,"View \"Real m%i_%g\" {\n",a+1,real(Omega));
        fprintf(f,"VP(0,0,0) {%e,%e,%e};\n",
                   mScale*real(ma[0]),
                   mScale*real(ma[1]),
                   mScale*real(ma[2]));
        fprintf(f,"};\n");

        fprintf(f,"View \"Imag m%i_%g\" {\n",a+1,real(Omega));
        fprintf(f,"VP(0,0,0) {%e,%e,%e};\n",
                   mScale*imag(ma[0]),
                   mScale*imag(ma[1]),
                   mScale*imag(ma[2]));
        fprintf(f,"};\n");

      }; // for(int nm=0; nm<NumMoments; nm++)
    fclose(f);

   };

}

void GetMomentPFT(SWGGeometry *G, cdouble Omega, IncField *IF,
                  HVector *JVector, HMatrix *DRMatrix,
                  HMatrix *PFTMatrix, bool KeepQpTerm, 
                  char *FileBase)
{
  for(int no=0; no<G->NumObjects; no++)
   GetMomentPFT(G, no, Omega, IF, JVector, DRMatrix, PFTMatrix,
                KeepQpTerm, FileBase);
}

/***************************************************************/
/* contribution of surface #no to its own PFT                  */
/***************************************************************/
void GetMomentPFTSelfTerm(int no, cdouble Omega, HMatrix *PM,
                          double PFT[NUMPFT])
{
  double k3   = real(Omega)*real(Omega)*real(Omega);
  double k4   = real(Omega)*k3;
  //double k5   = real(Omega)*k4;
  double PPF  = k4*ZVAC/(12.0*M_PI);
  double FPF  = -TENTHIRDS*k4*ZVAC/(12.0*M_PI);
  // double FPF2 = -TENTHIRDS*k5/(120.0*M_PI);
  double TPF  = -TENTHIRDS*k3*ZVAC/(6.0*M_PI);

  cdouble P[3], M[3];
  PM->GetEntries(no, "0:2",  P);
  PM->GetEntries(no, "3:5",  M);

  PFT[PFT_PABS]=0.0;

  PFT[PFT_PSCAT]=PPF*real( VecHDot(P,P) );

  for(int Mu=0; Mu<3; Mu++)
   { 
     int MP1=(Mu+1)%3, MP2=(Mu+2)%3;

     PFT[PFT_XFORCE + Mu]=
      FPF*real( conj(M[MP1])*P[MP2] - conj(M[MP2])*P[MP1] );

     PFT[PFT_XTORQUE+Mu]=
      TPF*( real(P[MP1])*imag(P[MP2]) - real(P[MP2])*imag(P[MP1]) );
   };
}


/***************************************************************/
/***************************************************************/
/***************************************************************/
void GetMomentPFTContribution(SWGGeometry *G, int noa, int nob,
                              cdouble Omega, HMatrix *PM,
                              double PFT[NUMPFT])
{
   cdouble Pa[3], Ma[3];
   PM->GetEntries(noa, "0:2",  Pa);
   PM->GetEntries(noa, "3:5",  Ma);

   cdouble Pb[3], Mb[3];
   PM->GetEntries(nob, "0:2",  Pb);
   PM->GetEntries(nob, "3:5",  Mb);

   cdouble Gij[3][3], C[3][3], dG[3][3][3], dC[3][3][3];
   double R[3];
   VecSub(G->Objects[noa]->Origin, G->Objects[nob]->Origin, R);
   cdouble EpsR=1.0, MuR=1.0;
   CalcGC(R, Omega, EpsR, MuR, Gij, C, dG, dC);

   double k=real(Omega), k2=k*k;
   memset(PFT, 0, NUMPFT*sizeof(double));
   for(int i=0; i<3; i++)
    for(int j=0; j<3; j++)
     { 
       cdouble PP = conj(Pa[i])*Pb[j]*ZVAC;
 
       PFT[PFT_PSCAT] -= 0.5*k2*real( PP*II*Omega*Gij[i][j] );

       for(int Mu=0; Mu<3; Mu++)
        PFT[PFT_XFORCE + Mu]
         -= 0.5*TENTHIRDS*k*imag( PP*II*Omega*dG[Mu][i][j] );
     };

   for(int Mu=0; Mu<3; Mu++)
    for(int Sigma=0; Sigma<3; Sigma++)
     { int Nu=(Mu+1)%3, Rho=(Mu+2)%3;
       cdouble PP = conj(Pa[Nu])*Pb[Sigma]*ZVAC;
       PFT[PFT_XTORQUE + Mu] 
        -= 0.5*TENTHIRDS*k*imag( PP*II*Omega*Gij[Rho][Sigma] );
     };
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
void GetExtinctionMomentPFT(SWGGeometry *G, int no,
                            IncField *IF, cdouble Omega,
                            HMatrix *PM, double PFT[NUMPFT])
                            
{
  cdouble PDagger[3], MDagger[3];
  for(int Mu=0; Mu<3; Mu++)
   { PDagger[Mu] = conj(PM->GetEntry(no,Mu + 0*3));
     MDagger[Mu] = conj(PM->GetEntry(no,Mu + 1*3));
   };

  cdouble EH[6], dEH[3][6];
  IF->GetFields(G->Objects[no]->Origin, EH);
  IF->GetFieldGradients(G->Objects[no]->Origin, dEH);
  cdouble *E, *dE[3];
   E    =  EH    + 0*3;
  dE[0] = dEH[0] + 0*3;
  dE[1] = dEH[1] + 0*3;
  dE[2] = dEH[2] + 0*3;
     
  memset(PFT, 0, NUMPFT*sizeof(double));
  for(int i=0; i<3; i++)
   { PFT[PFT_PABS]    += 0.5*real(II*Omega*PDagger[i]*E[i]);
     PFT[PFT_XFORCE]  += 0.5*TENTHIRDS*imag(II*PDagger[i]*dE[0][i]);
     PFT[PFT_YFORCE]  += 0.5*TENTHIRDS*imag(II*PDagger[i]*dE[1][i]);
     PFT[PFT_ZFORCE]  += 0.5*TENTHIRDS*imag(II*PDagger[i]*dE[2][i]);

     PFT[PFT_XTORQUE] += 0.5*TENTHIRDS*imag(II*(PDagger[1]*E[2] - PDagger[2]*E[1]));
     PFT[PFT_YTORQUE] += 0.5*TENTHIRDS*imag(II*(PDagger[2]*E[0] - PDagger[0]*E[2]));
     PFT[PFT_ZTORQUE] += 0.5*TENTHIRDS*imag(II*(PDagger[0]*E[1] - PDagger[1]*E[0]));
   };

}

/***************************************************************/
/***************************************************************/
/***************************************************************/
HMatrix *GetMomentPFTMatrix(SWGGeometry *G, cdouble Omega, IncField *IF,
                            HVector *JVector, HMatrix *DRMatrix,
                            HMatrix *PFTMatrix, bool Itemize)
{ 
  (void) DRMatrix;

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  int NO = G->NumObjects;
  if (    (PFTMatrix==0)
       || (PFTMatrix->NR != NO)
       || (PFTMatrix->NC != NUMPFT)
     )
   ErrExit("invalid PFTMatrix in MomentPFT");

  /***************************************************************/
  /* ScatteredPFT[no] = contribution of surface #no to           */
  /*                    scattered PFT                            */
  /***************************************************************/
  static int NOSave=0;
  static HMatrix **ScatteredPFT=0, *ExtinctionPFT=0;
  static HMatrix *PM;
  if (NOSave!=NO)
   { if (ScatteredPFT)
      { for(int no=0; no<NOSave; no++)
         if (ScatteredPFT[no]) 
          delete ScatteredPFT[no];
        free(ScatteredPFT);
       if (ExtinctionPFT)
        delete ExtinctionPFT;
       if (PM)
        delete PM;
      };
     NOSave=NO;
     ScatteredPFT=(HMatrix **)mallocEC(NO*sizeof(HMatrix));
     for(int no=0; no<NO; no++)
      ScatteredPFT[no]=new HMatrix(NO, NUMPFT);
     ExtinctionPFT=new HMatrix(NO, NUMPFT);
     PM=new HMatrix(NO, 6, LHM_COMPLEX);
   };

  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  /*--------------------------------------------------------------*/
  for(int no=0; no<NO; no++)
   { cdouble p[3], m[3], Qp[3];
     GetMoments(G, no, Omega, JVector, p, m, Qp);
     PM->SetEntries(no, "0:2", p);
     PM->SetEntries(no, "3:5", m);
   };

  /*--------------------------------------------------------------*/
  /*- loop over all object to get moment PFT on that object       */
  /*--------------------------------------------------------------*/
  for(int no=0; no<NO; no++)
   ScatteredPFT[no]->Zero();

  bool UseSymmetry=true;
  char *s=getenv("BUFF_MOMENTPFT_SYMMETRY");
  if (s && s[0]=='0')
   { UseSymmetry=false;
     Log("Not using symmetry in Moment PFT calculation.");
   };

  for(int no=0; no<NO; no++)
   ScatteredPFT[no]->Zero();

  for(int noa=0; noa<NO; noa++)
   for(int nob=(UseSymmetry ? noa : 0); nob<NO; nob++)
    { 
      double PFT[NUMPFT];

      if (noa==nob)
       GetMomentPFTSelfTerm(noa, Omega, PM, PFT);
      else
       GetMomentPFTContribution(G, noa, nob, Omega, PM, PFT);

      for(int nq=0; nq<NUMPFT; nq++)
       ScatteredPFT[nob]->AddEntry(noa, nq, PFT[nq]);

      if (UseSymmetry && noa!=nob)
       { ScatteredPFT[noa]->AddEntry(nob, PFT_PSCAT, PFT[PFT_PSCAT]);
         for(int nq=PFT_XFORCE; nq<NUMPFT; nq++)
          ScatteredPFT[noa]->AddEntry(nob, nq, -1.0*PFT[nq]);
       };
    };

  /***************************************************************/
  /* get incident-field contributiono ****************************/
  /***************************************************************/
  ExtinctionPFT->Zero();
  if (IF)
   { IF->SetFrequency(Omega, true);
     for(int no=0; no<NO; no++)
      { double PFT[NUMPFT];
        GetExtinctionMomentPFT(G, no, IF, Omega, PM, PFT);
        ExtinctionPFT->SetEntriesD(no,":",PFT);
      };
   };
   
  /***************************************************************/
  /* sum scattered contributions from all surfaces plus          */
  /* extinction contributions to get total PFT.                  */
  /* note that the formula for total PFT is                      */
  /* Q^{full} = Q^{extinction} - Q^{scattering}                  */
  /* so the scattering contributions enter with a minus sign     */
  /* (except for the scattered power).                           */
  /***************************************************************/
  PFTMatrix->Zero();
  for(int noa=0; noa<NO; noa++)
   for(int nq=0; nq<NUMPFT; nq++)
    { 
      PFTMatrix->AddEntry(noa,nq,ExtinctionPFT->GetEntry(noa,nq));

      for(int nob=0; nob<NO; nob++)
       { 
         if (nq==PFT_PABS)
          PFTMatrix->AddEntry(noa,nq,-1.0*ScatteredPFT[nob]->GetEntry(noa,PFT_PSCAT));
         else if (nq==PFT_PSCAT)
          PFTMatrix->AddEntry(noa,nq,+1.0*ScatteredPFT[nob]->GetEntry(noa,PFT_PSCAT));
         else // force or torque 
          PFTMatrix->AddEntry(noa,nq,-1.0*ScatteredPFT[nob]->GetEntry(noa,nq));
       };
    };
  
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  char *ss=getenv("BUFF_ITEMIZE_PFT");
  if (ss && ss[0]=='1')
   Itemize=true;
  if (Itemize)
   { 
     static bool WrotePreamble=false;
     for(int noa=0; noa<NO; noa++)
      { char *Label=G->Objects[noa]->Label;
        FILE *f=vfopen("%s.%s.MomentPFT","a",
                        GetFileBase(G->GeoFileName),
                        Label);
        if (!f) continue;
        if (!WrotePreamble)
         { fprintf(f,"# Moment contribution to object %s\n",Label);
           fprintf(f,"# column: \n");
           fprintf(f,"# 1 frequency \n");
           fprintf(f,"# 2 destination object label \n");
           fprintf(f,"# 03-10 PAbs, PScat, Fxyz, Txyz (total)\n");
           fprintf(f,"# 11-18 PAbs, PScat, Fxyz, Txyz (extinction)\n");
           int nc=19;
           for(int nob=0; nob<NO; nob++, nc+=NUMPFT)
            fprintf(f,"# %i-%i PAbs, PScat, Fxyz, Txyz (object %s)\n",nc,nc+NUMPFT-1,G->Objects[nob]->Label);
         };
        fprintf(f,"%e %s ",real(Omega),Label);
        for(int nq=0; nq<NUMPFT; nq++)
         fprintf(f,"%e ",PFTMatrix->GetEntryD(noa,nq));
        for(int nq=0; nq<NUMPFT; nq++)
         fprintf(f,"%e ",ExtinctionPFT->GetEntryD(noa,nq));
        for(int nob=0; nob<NO; nob++)
         for(int nq=0; nq<NUMPFT; nq++)
          fprintf(f,"%e ",ScatteredPFT[nob]->GetEntryD(noa,nq));
        fprintf(f,"\n");
        fclose(f);
      };
     WrotePreamble=true;
   };

  return PFTMatrix;
}
  

} // namespace buff
