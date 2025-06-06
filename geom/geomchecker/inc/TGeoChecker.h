// @(#)root/geom:$Id$
// Author: Andrei Gheata   01/11/01

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_TGeoChecker
#define ROOT_TGeoChecker

#include "TVirtualGeoChecker.h"

// forward declarations
class TTree;
class TGeoShape;
class TGeoVolume;
class TGeoVoxelFinder;
class TGeoNode;
class TGeoManager;
class TGeoMatrix;
class TGeoOverlap;
class TBuffer3D;
class TH2F;
class TStopwatch;

///////////////////////////////////////////////////////////////////////////
// TGeoChecker - A simple checker generating random points inside a      //
//   geometry. Generates a tree of points on the surfaces corresponding  //
//   to the safety of each generated point                               //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

class TGeoChecker : public TVirtualGeoChecker {
private:
   // data members
   TGeoManager *fGeoManager; // pointer to geometry manager
   TGeoVolume *fVsafe;       // volume to which a safety sphere node was added
   TBuffer3D *fBuff1;        // Buffer containing mesh vertices for first volume
   TBuffer3D *fBuff2;        // Buffer containing mesh vertices for second volume
   Bool_t fFullCheck;        // Full overlap checking
   Double_t *fVal1;          //! Array of number of crossings per volume.
   Double_t *fVal2;          //! Array of timing per volume.
   Bool_t *fFlags;           //! Array of flags per volume.
   TStopwatch *fTimer;       //! Timer
   TGeoNode *fSelectedNode;  //! Selected node for overlap checking
   Int_t fNchecks;           //! Number of checks for current volume
   Int_t fNmeshPoints;       //! Number of points on mesh to be checked
                             // methods
   void CleanPoints(Double_t *points, Int_t &numPoints) const;
   Int_t NChecksPerVolume(TGeoVolume *vol);
   Int_t PropagateInGeom(Double_t *, Double_t *);
   void Score(TGeoVolume *, Int_t, Double_t);
   Double_t TimingPerVolume(TGeoVolume *);

public:
   // constructors
   TGeoChecker();
   TGeoChecker(TGeoManager *geom);
   // destructor
   ~TGeoChecker() override;
   // methods
   void CheckBoundaryErrors(Int_t ntracks = 1000000, Double_t radius = -1.) override;
   void CheckBoundaryReference(Int_t icheck = -1) override;
   void CheckGeometryFull(Bool_t checkoverlaps = kTRUE, Bool_t checkcrossings = kTRUE, Int_t nrays = 10000,
                          const Double_t *vertex = nullptr) override;
   void CheckGeometry(Int_t nrays, Double_t startx, Double_t starty, Double_t startz) const override;
   void CheckOverlaps(const TGeoVolume *vol, Double_t ovlp = 0.1, Option_t *option = "") override;
   void CheckOverlapsBySampling(TGeoVolume *vol, Double_t ovlp = 0.1, Int_t npoints = 1000000) const;
   void CheckPoint(Double_t x = 0, Double_t y = 0, Double_t z = 0, Option_t *option = "", Double_t safety = 0.) override;
   void CheckShape(TGeoShape *shape, Int_t testNo, Int_t nsamples, Option_t *option) override;
   Double_t CheckVoxels(TGeoVolume *vol, TGeoVoxelFinder *voxels, Double_t *xyz, Int_t npoints);
   TH2F *LegoPlot(Int_t ntheta = 60, Double_t themin = 0., Double_t themax = 180., Int_t nphi = 90,
                  Double_t phimin = 0., Double_t phimax = 360., Double_t rmin = 0., Double_t rmax = 9999999,
                  Option_t *option = "") override;
   void PrintOverlaps() const override;
   void RandomPoints(TGeoVolume *vol, Int_t npoints, Option_t *option) override;
   void RandomRays(Int_t nrays, Double_t startx, Double_t starty, Double_t startz, const char *target_vol = nullptr,
                   Bool_t check_norm = kFALSE)  override;
   TGeoOverlap *MakeCheckOverlap(const char *name, TGeoVolume *vol1, TGeoVolume *vol2, TGeoMatrix *mat1,
                                 TGeoMatrix *mat2, Bool_t isovlp, Double_t ovlp);
   void OpProgress(const char *opname, Long64_t current, Long64_t size, TStopwatch *watch = nullptr,
                   Bool_t last = kFALSE, Bool_t refresh = kFALSE, const char *msg = "") override;
   TGeoNode *SamplePoints(Int_t npoints, Double_t &dist, Double_t epsil, const char *g3path) override;
   void ShapeDistances(TGeoShape *shape, Int_t nsamples, Option_t *option);
   void ShapeSafety(TGeoShape *shape, Int_t nsamples, Option_t *option);
   void ShapeNormal(TGeoShape *shape, Int_t nsamples, Option_t *option);
   Double_t *ShootRay(Double_t *start, Double_t dirx, Double_t diry, Double_t dirz, Double_t *array, Int_t &nelem,
                      Int_t &dim, Double_t *enpoint = nullptr) const;
   void SetSelectedNode(TGeoNode *node) override { fSelectedNode = node; }
   void SetNmeshPoints(Int_t npoints = 1000) override;
   void Test(Int_t npoints, Option_t *option) override;
   void TestOverlaps(const char *path) override;
   Bool_t TestVoxels(TGeoVolume *vol, Int_t npoints = 1000000) override;
   Double_t Weight(Double_t precision = 0.01, Option_t *option = "v") override;

   ClassDefOverride(TGeoChecker, 3) // a simple geometry checker
};

#endif
