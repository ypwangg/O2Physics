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
// skimming task.  In a data-processing workflow, replace the producer of
// ReducedEventsQvectorCentr (e.g. dqFlow) with this task so that downstream
// consumers automatically receive shift-corrected Q-vectors without any code change.
// ------------------------------------------------------------------------------------

using namespace o2;
using namespace o2::framework;

// Detector index mapping (must match qVectorsTable.cxx)
// Shift profile y-axis uses 2*detIdx (Re) and 2*detIdx+1 (Im).
enum DetectorIdx {
  kFT0C = 0,
  kFT0A = 1,
  kFT0M = 2,
  kFV0A = 3,
  kTPCpos = 4, // maps to BPos (deprecated)
  kTPCneg = 5, // maps to BNeg (deprecated)
  kNdets = 6
};

// Convenience alias for the joined input event table
using MyCollisions = soa::Join<aod::ReducedEvents, aod::ReducedEventsExtended, aod::ReducedEventsQvectorCentr>;

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

  // --- Output table ---
  Produces<aod::ReducedEventsQvectorCentr> qVectorShifted;

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
    AxisSpec axisDetIdx{kNdets, -0.5, kNdets - 0.5, "Detector index"};

    mQA.add("deltaPsi", "Shift correction angle;Detector index;#Delta#psi",
            {HistType::kTH2F, {axisDetIdx, axisDeltaPsi}});
    mQA.add("centVsPsi", "Centrality vs event plane angle",
            {HistType::kTH2F, {axisCent, {100, -TMath::Pi() / 2., TMath::Pi() / 2.}}});
    mQA.add("qvecBefore", "Input Q-vector components;Re;Im",
            {HistType::kTH2F, {{200, -2., 2.}, {200, -2., 2.}}});
    mQA.add("qvecAfter", "Shift-corrected Q-vector components;Re;Im",
            {HistType::kTH2F, {{200, -2., 2.}, {200, -2., 2.}}});
  }

  /// Load the shift correction profile for the given timestamp / run
  void initCCDB(uint64_t timestamp, int runNumber)
  {
    std::string fullPath = cfgShiftPath.value;
    if (fullPath.back() != '/') {
      fullPath += '/';
    }
    fullPath += "v";
    fullPath += std::to_string(cfgHarmonic.value);

    mShiftProfile = ccdb->getForTimeStamp<TProfile3D>(fullPath, timestamp);
    if (!mShiftProfile) {
      // Fallback to run-number-based lookup
      mShiftProfile = ccdb->getForRun<TProfile3D>(fullPath, runNumber);
    }

    if (mShiftProfile) {
      LOGF(info, "Loaded shift correction profile from %s", fullPath.data());
    } else {
      LOGF(warning, "Shift correction profile NOT found at %s. Correction will be a no-op.",
           fullPath.data());
    }
  }

  /// Apply the Fourier-series shift correction to a Q-vector.
  ///
  /// Formula (from qVectorsTable.cxx):
  ///   psi = atan2(Qy, Qx) / n
  ///   deltapsi = sum_{k=1}^{N} (2/k) * (-c_k * cos(k*n*psi) + d_k * sin(k*n*psi)) / n
  ///   Q'_{Re} = Q_{Re} * cos(deltapsi * n) - Q_{Im} * sin(deltapsi * n)
  ///   Q'_{Im} = Q_{Re} * sin(deltapsi * n) + Q_{Im} * cos(deltapsi * n)
  ///
  void applyShiftCorrection(float& qvecRe, float& qvecIm,
                            float cent, int detIdx) const
  {
    if (!mShiftProfile) {
      return;
    }

    int n = cfgHarmonic.value;

    // Event plane angle in the n-th harmonic
    float psi = TMath::ATan2(qvecIm, qvecRe) / static_cast<float>(n);

    // Accumulate deltapsi (in the original formula this is /n, then later multiplied back by n)
    float deltapsiOverN = 0.f;
    for (int k = 1; k <= cfgNShiftMax.value; ++k) {
      float coeffShiftX = mShiftProfile->GetBinContent(
        mShiftProfile->FindBin(cent, 2 * detIdx, k - 0.5));
      float coeffShiftY = mShiftProfile->GetBinContent(
        mShiftProfile->FindBin(cent, 2 * detIdx + 1, k - 0.5));

      float angle = static_cast<float>(k) * static_cast<float>(n) * psi;
      deltapsiOverN += (2.f / static_cast<float>(k)) *
                       (-coeffShiftX * TMath::Cos(angle) + coeffShiftY * TMath::Sin(angle));
    }
    // The total rotation in Q-vector space (= deltapsi * n) is simply deltapsiOverN * n
    float rotAngle = deltapsiOverN; // = deltapsi * n  (because deltapsiOverN already had /n inside)

    // Rotate the Q-vector
    float cosRot = TMath::Cos(rotAngle);
    float sinRot = TMath::Sin(rotAngle);
    float qvecReNew = qvecRe * cosRot - qvecIm * sinRot;
    float qvecImNew = qvecRe * sinRot + qvecIm * cosRot;

    qvecRe = qvecReNew;
    qvecIm = qvecImNew;
  }

  /// Process events with joined ReducedEvents + ReducedEventsExtended + ReducedEventsQvectorCentr.
  void process(MyCollisions const& collisions)
  {
    // Run-by-run calibration loading (use the first event's info)
    if (!collisions.empty()) {
      auto& first = collisions.begin();
      uint64_t timestamp = first.timestamp();
      int runNumber = first.runNumber();
      if (runNumber != mCurrentRun) {
        initCCDB(timestamp, runNumber);
        mCurrentRun = runNumber;
      }
    }

    int n = cfgHarmonic.value;
    auto isValid = [](float re, float im) { return re > -900.f && im > -900.f; };

    for (auto& coll : collisions) {
      // Centrality
      float cent = 110.f; // flag for "outside range"
      if (cfgCentEsti.value == 0) {
        cent = coll.centFT0M();
      } else if (cfgCentEsti.value == 1) {
        cent = coll.centFT0A();
      } else if (cfgCentEsti.value == 2) {
        cent = coll.centFT0C();
      } else if (cfgCentEsti.value == 3) {
        cent = coll.centFV0A();
      }

      // Read Q-vectors
      float qvecFT0ARe = coll.qvecFT0ARe();
      float qvecFT0AIm = coll.qvecFT0AIm();
      float qvecFT0CRe = coll.qvecFT0CRe();
      float qvecFT0CIm = coll.qvecFT0CIm();
      float qvecFT0MRe = coll.qvecFT0MRe();
      float qvecFT0MIm = coll.qvecFT0MIm();
      float qvecFV0ARe = coll.qvecFV0ARe();
      float qvecFV0AIm = coll.qvecFV0AIm();
      float qvecBPosRe = coll.qvecBPosRe();
      float qvecBPosIm = coll.qvecBPosIm();
      float qvecBNegRe = coll.qvecBNegRe();
      float qvecBNegIm = coll.qvecBNegIm();

      // Sum amplitudes / track counts (passed through unchanged)
      float sumAmplFT0A = coll.sumAmplFT0A();
      float sumAmplFT0C = coll.sumAmplFT0C();
      float sumAmplFT0M = coll.sumAmplFT0M();
      float sumAmplFV0A = coll.sumAmplFV0A();
      int nTrkBPos = coll.nTrkBPos();
      int nTrkBNeg = coll.nTrkBNeg();

      // QA: fill before-correction Q-vectors
      if (isValid(qvecFT0ARe, qvecFT0AIm))
        mQA.fill(HIST("qvecBefore"), qvecFT0ARe, qvecFT0AIm);
      if (isValid(qvecFT0CRe, qvecFT0CIm))
        mQA.fill(HIST("qvecBefore"), qvecFT0CRe, qvecFT0CIm);
      if (isValid(qvecFT0MRe, qvecFT0MIm))
        mQA.fill(HIST("qvecBefore"), qvecFT0MRe, qvecFT0MIm);

      // Apply shift correction (only if centrality is within range)
      if (cent >= 0.f && cent < cfgMaxCentrality.value) {
        if (isValid(qvecFT0ARe, qvecFT0AIm))
          applyShiftCorrection(qvecFT0ARe, qvecFT0AIm, cent, kFT0A);
        if (isValid(qvecFT0CRe, qvecFT0CIm))
          applyShiftCorrection(qvecFT0CRe, qvecFT0CIm, cent, kFT0C);
        if (isValid(qvecFT0MRe, qvecFT0MIm))
          applyShiftCorrection(qvecFT0MRe, qvecFT0MIm, cent, kFT0M);
        if (isValid(qvecFV0ARe, qvecFV0AIm))
          applyShiftCorrection(qvecFV0ARe, qvecFV0AIm, cent, kFV0A);
        if (isValid(qvecBPosRe, qvecBPosIm))
          applyShiftCorrection(qvecBPosRe, qvecBPosIm, cent, kTPCpos);
        if (isValid(qvecBNegRe, qvecBNegIm))
          applyShiftCorrection(qvecBNegRe, qvecBNegIm, cent, kTPCneg);
      }

      // QA: fill after-correction Q-vectors
      if (isValid(qvecFT0ARe, qvecFT0AIm))
        mQA.fill(HIST("qvecAfter"), qvecFT0ARe, qvecFT0AIm);
      if (isValid(qvecFT0CRe, qvecFT0CIm))
        mQA.fill(HIST("qvecAfter"), qvecFT0CRe, qvecFT0CIm);
      if (isValid(qvecFT0MRe, qvecFT0MIm))
        mQA.fill(HIST("qvecAfter"), qvecFT0MRe, qvecFT0MIm);

      // Fill output table (same structure as ReducedEventsQvectorCentr)
      qVectorShifted(
        qvecFT0ARe, qvecFT0AIm,
        qvecFT0CRe, qvecFT0CIm,
        qvecFT0MRe, qvecFT0MIm,
        qvecFV0ARe, qvecFV0AIm,
        qvecBPosRe, qvecBPosIm,
        qvecBNegRe, qvecBNegIm,
        sumAmplFT0A, sumAmplFT0C, sumAmplFT0M, sumAmplFV0A,
        nTrkBPos, nTrkBNeg);
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<QvectorShiftTask>(cfgc)};
}
