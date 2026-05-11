// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

///
/// \file   qvectorShift.cxx
/// \brief  Task to apply the Fourier-series-based shift correction to the
///         Q-vectors stored in the ReducedEventsQvectorCentr table.
///
///         The shift correction accounts for residual event plane distortions
///         after the standard recenter+twist+rescale corrections. The correction
///         coefficients are retrieved from CCDB as TProfile3D objects.
///
///         Shift correction formula (for each detector and harmonic n):
///           deltapsi = (2/n) * sum_{k=1}^{10} (1/k) * (-c_k * cos(k*n*psi) + d_k * sin(k*n*psi))
///         where c_k, d_k are the Fourier coefficients from the shift profile,
///         and psi = atan2(Qy, Qx)/n is the event plane angle.
///
///         The corrected Q-vector is obtained by rotating the original vector
///         by deltapsi * n in the complex plane.
///
/// \author Copilot-generated for O2Physics PWGDQ
///

#include <CCDB/BasicCCDBManager.h>
#include <Common/DataModel/Centrality.h>
#include <Common/DataModel/EventSelection.h>
#include <Common/DataModel/Qvectors.h>
#include <Framework/AnalysisTask.h>
#include <Framework/Configurable.h>
#include <Framework/HistogramRegistry.h>
#include <Framework/runDataProcessing.h>
#include <TProfile3D.h>

#include "PWGDQ/DataModel/ReducedInfoTables.h"

// ------------------------------------------------------------------------------------
// This task produces the SAME table type (ReducedEventsQvectorCentr) as the upstream
// producer (dqFlow).  In a data-processing workflow you replace dqFlow with this
// task so that downstream consumers automatically receive shift-corrected Q-vectors
// without any code change.
//
// Input:  ReducedEvents + ReducedEventsExtended + central Q-vector tables
//         (QvectorFT0Cs, QvectorFT0As, QvectorFT0Ms, QvectorFV0As,
//          QvectorTPCposs, QvectorTPCnegs)
// Output: ReducedEventsQvectorCentr (same schema as upstream)
// ------------------------------------------------------------------------------------

using namespace o2;
using namespace o2::framework;

// Detector-index constants (must match qVectorsTable.cxx)
// The shift profile TProfile3D stores coefficients at y-bin 2*detIdx (cos) and 2*detIdx+1 (sin).
enum DetectorIdx {
  kFT0C = 0,
  kFT0A = 1,
  kFT0M = 2,
  kFV0A = 3,
  kTPCpos = 4,   // → BPos column
  kTPCneg = 5,   // → BNeg column
  kNdets = 6
};

// Read raw central Q-vector tables + event info, then produce
// shift-corrected ReducedEventsQvectorCentr.
using MyCollisions = soa::Join<aod::ReducedEvents, aod::ReducedEventsExtended,aod::ReducedEventsQvectorCentr>;

struct QvectorShiftTask {

  // --- CCDB configuration ---
  struct : ConfigurableGroup {
    Configurable<std::string> cfgURL{"cfgURL", "http://alice-ccdb.cern.ch", "CCDB URL"};
    Configurable<int64_t> nolaterthan{
      "ccdb-no-later-than",
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count(),
      "Latest acceptable timestamp of creation for the object"};
  } cfgCcdbParam;

  Configurable<std::string> cfgShiftPath{
    "cfgShiftPath", "Analysis/EventPlane/QVecCorrections",
    "CCDB path for shift correction (e.g. Analysis/EventPlane/QVecCorrections/v2)"};

  Configurable<int> cfgHarmonic{"cfgHarmonic", 2,
                                "Harmonic n of the Q-vectors (default: 2)"};

  Configurable<int> cfgNShiftMax{"cfgNShiftMax", 10,
                                 "Maximum Fourier term in the shift correction (1..10)"};

  Configurable<float> cfgMaxCentrality{"cfgMaxCentrality", 100.f,
                                       "Maximum centrality up to which shift correction is applied"};

  Configurable<int> cfgCentEsti{"cfgCentEsti", 2,
                                "Centrality estimator: 0=FT0M, 1=FT0A, 2=FT0C (default), 3=FV0A"};

  // --- CCDB service ---
  Service<o2::ccdb::BasicCCDBManager> ccdb;

  // --- Run-local calibration objects ---
  TProfile3D* mShiftProfile = nullptr;
  int mCurrentRun = -1;

  // --- Produces the standard ReducedEventsQvectorCentr (same schema as dqFlow) ---
  Produces<aod::ReducedEventsQvectorCentr_001> qVectorCentr;

  // --- QA histograms ---
  HistogramRegistry mQA{"qaQvectorShift", {}, OutputObjHandlingPolicy::AnalysisObject, false, false};

  void init(InitContext&)
  {
    ccdb->setURL(cfgCcdbParam.cfgURL.value);
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();
    ccdb->setCreatedNotAfter(cfgCcdbParam.nolaterthan.value);
    ccdb->setFatalWhenNull(false);

    // QA histograms
    AxisSpec axisCent{20, 0., 100., "Centrality (%)"};
    AxisSpec axisDeltaPsi{100, -TMath::Pi(), TMath::Pi(), "#Delta#psi (rad)"};
    AxisSpec axisDetIdx{kNdets, -0.5, kNdets - 0.5, "Detector"};

    mQA.add("deltaPsi", ";Detector;#Delta#psi",
            {HistType::kTH2F, {axisDetIdx, axisDeltaPsi}});
    mQA.add("qvecBefore", ";Re;Im",
            {HistType::kTH2F, {{200, -2., 2.}, {200, -2., 2.}}});
    mQA.add("qvecAfter", ";Re;Im",
            {HistType::kTH2F, {{200, -2., 2.}, {200, -2., 2.}}});
  }

  /// Load the shift correction profile for the given timestamp / run.
  /// Path: cfgShiftPath/v<N>  (N = cfgHarmonic)
  void initCCDB(uint64_t timestamp, int runNumber)
  {
    std::string path = cfgShiftPath.value;
    if (path.back() != '/')
      path += '/';
    path += "v" + std::to_string(cfgHarmonic.value);

    mShiftProfile = ccdb->getForTimeStamp<TProfile3D>(path, timestamp);
    if (!mShiftProfile)
      mShiftProfile = ccdb->getForRun<TProfile3D>(path, runNumber);

    if (mShiftProfile)
      LOGF(info, "Loaded shift profile from %s", path.data());
    else
      LOGF(warning, "No shift profile at %s – no correction applied", path.data());
  }

  /// Apply the Fourier-series shift correction.
  void applyShiftCorrection(float& qRe, float& qIm,
                            float cent, int detIdx) const
  {
    if (!mShiftProfile)
      return;
    int n = cfgHarmonic.value;

    float psi = TMath::ATan2(qIm, qRe) / static_cast<float>(n);
    float sum = 0.f;
    for (int k = 1; k <= cfgNShiftMax.value; ++k) {
      float cx = mShiftProfile->GetBinContent(
        mShiftProfile->FindBin(cent, 2 * detIdx, k - 0.5));
      float cy = mShiftProfile->GetBinContent(
        mShiftProfile->FindBin(cent, 2 * detIdx + 1, k - 0.5));
      float ang = static_cast<float>(k) * static_cast<float>(n) * psi;
      sum += (2.f / static_cast<float>(k)) *
             (-cx * TMath::Cos(ang) + cy * TMath::Sin(ang));
    }
    // rotAngle = sum  (which already contains the /n factor,
    // so the actual rotation = sum * n = Δψ · n)
    float c = TMath::Cos(sum);
    float s = TMath::Sin(sum);
    float qReNew = qRe * c - qIm * s;
    float qImNew = qRe * s + qIm * c;
    qRe = qReNew;
    qIm = qImNew;
  }

  /// Process — read central Q-vector tables, apply shift, produce
  /// ReducedEventsQvectorCentr (same table type that dqFlow produces).
  ///
  /// Column translation:
  ///   Central table | ReducedEventsQvectorCentr column
  ///   --------------+----------------------------------
  ///   qvecFT0ARe/Im | QvecFT0ARe/Im
  ///   qvecFT0CRe/Im | QvecFT0CRe/Im
  ///   qvecFT0MRe/Im | QvecFT0MRe/Im
  ///   qvecFV0ARe/Im | QvecFV0ARe/Im
  ///   qvecTPCposRe/Im | QvecBPosRe/Im
  ///   qvecTPCnegRe/Im | QvecBNegRe/Im
  ///   sumAmplFT0A/C/M | SumAmplFT0A/C/M
  ///   sumAmplFV0A  | SumAmplFV0A
  ///   nTrkTPCpos   | NTrkBPos
  ///   nTrkTPCneg   | NTrkBNeg
  void processSkimmed(MyCollisions const& collisions)
  {
    if (collisions.size() == 0)
      return;

    // Run-by-run CCDB loading
    auto first = collisions.begin();
    int run = first.runNumber();
    if (run != mCurrentRun) {
      initCCDB(first.timestamp(), run);
      mCurrentRun = run;
    }

    auto valid = [](float x, float y) { return x > -900.f && y > -900.f; };

    for (auto& ev : collisions) {
      // Get centrality
      float cent = 110.f;
      if (cfgCentEsti.value == 0)
        cent = ev.centFT0M();
      else if (cfgCentEsti.value == 1)
        cent = ev.centFT0A();
      else if (cfgCentEsti.value == 2)
        cent = ev.centFT0C();

      // Read Q-vector components from central tables
      float rFT0A = ev.qvecFT0ARe(), iFT0A = ev.qvecFT0AIm();
      float rFT0C = ev.qvecFT0CRe(), iFT0C = ev.qvecFT0CIm();
      float rFT0M = ev.qvecFT0MRe(), iFT0M = ev.qvecFT0MIm();
      float rFV0A = ev.qvecFV0ARe(), iFV0A = ev.qvecFV0AIm();
      float rBPos = ev.qvecBPosRe(), iBPos = ev.qvecBPosIm();
      float rBNeg = ev.qvecBNegRe(), iBNeg = ev.qvecBNegIm();

      // Metadata (pass through unchanged)
      float sFT0A = ev.sumAmplFT0A(), sFT0C = ev.sumAmplFT0C();
      float sFT0M = ev.sumAmplFT0M(), sFV0A = ev.sumAmplFV0A();
      int nBPos = ev.nTrkTPCpos(), nBNeg = ev.nTrkTPCneg();

      // QA before
      if (valid(rFT0A, iFT0A))
        mQA.fill(HIST("qvecBefore"), rFT0A, iFT0A);

      // Apply shift correction
      bool doCorr = (cent >= 0.f && cent < cfgMaxCentrality.value);
      if (doCorr) {
        if (valid(rFT0A, iFT0A))
          applyShiftCorrection(rFT0A, iFT0A, cent, kFT0A);
        if (valid(rFT0C, iFT0C))
          applyShiftCorrection(rFT0C, iFT0C, cent, kFT0C);
        if (valid(rFT0M, iFT0M))
          applyShiftCorrection(rFT0M, iFT0M, cent, kFT0M);
        if (valid(rFV0A, iFV0A))
          applyShiftCorrection(rFV0A, iFV0A, cent, kFV0A);
        if (valid(rBPos, iBPos))
          applyShiftCorrection(rBPos, iBPos, cent, kTPCpos);
        if (valid(rBNeg, iBNeg))
          applyShiftCorrection(rBNeg, iBNeg, cent, kTPCneg);
      }

      // QA after
      if (valid(rFT0A, iFT0A))
        mQA.fill(HIST("qvecAfter"), rFT0A, iFT0A);

      // Produce standard ReducedEventsQvectorCentr
      qVectorCentr(rFT0A, iFT0A, rFT0C, iFT0C, rFT0M, iFT0M,
                   rFV0A, iFV0A, rBPos, iBPos, rBNeg, iBNeg,
                   sFT0A, sFT0C, sFT0M, sFV0A, nBPos, nBNeg, cfgHarmonic.value);
    }
  }
};

void processDummy(MyCollisions const& collisions) {
    for (auto& ev : collisions) {
      qVectorCentr(ev.qvecFT0ARe(), ev.qvecFT0AIm(), ev.qvecFT0CRe(), ev.qvecFT0CIm(), ev.qvecFT0MRe(), ev.qvecFT0MIm(), ev.qvecFV0ARe(), ev.qvecFV0AIm(),
                   ev.qvecBPosRe(), ev.qvecBPosIm(), ev.qvecBNegRe(), ev.qvecBNegIm(),
                   ev.sumAmplFT0A(), ev.sumAmplFT0C(), ev.sumAmplFT0M(), ev.sumAmplFV0A(),
                   ev.nTrkTPCpos(), ev.nTrkTPCneg(), -1);
    }
}

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<QvectorShiftTask>(cfgc)};
}
