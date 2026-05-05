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
//
// Contact: yiping.wang@cern.ch
//
// Class to compute the ML response for DQ-analysis selections
//

#ifndef PWGDQ_CORE_QUADMLRESPONSE_H_
#define PWGDQ_CORE_QUADMLRESPONSE_H_

#include "Tools/ML/MlResponse.h"
#include "PWGDQ/Core/VarManager.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Fill the map of available input features
// the key is the feature's name (std::string)
// the value is the corresponding value in EnumInputFeatures
#define FILL_MAP_QUAD(FEATURE)                                 \
  {                                                            \
    #FEATURE, static_cast<uint8_t>(QuadInputFeatures::FEATURE)     \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_QUAD(FEATURE, GETTER)                  \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = fValuesQuadruplet[VarManager::GETTER];        \
    break;                                                    \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_LEPTON1(FEATURE, GETTER)           \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = lepton1.GETTER(); \
    break;                                                    \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_LEPTON2(FEATURE, GETTER)           \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = lepton2.GETTER(); \
    break;                                                    \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_DILEPTON(FEATURE, GETTER)           \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = dilepton.GETTER(); \
    break;                                                    \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_TRACK1(FEATURE, GETTER)           \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = track1.GETTER(); \
    break;                                                    \
  }

// Check if the index of mCachedIndices (index associated to a FEATURE)
// matches the entry in EnumInputFeatures associated to this FEATURE
// if so, the inputFeatures vector is filled with the FEATURE's value
// by calling the corresponding GETTER=FEATURE from track
#define CHECK_AND_FILL_TRACK2(FEATURE, GETTER)           \
  case static_cast<uint8_t>(QuadInputFeatures::FEATURE): {        \
    inputFeature = track2.GETTER(); \
    break;                                                    \
  }

namespace o2::analysis
{

enum class QuadInputFeatures : uint8_t { // refer to Quad table, TODO: add more features if needed
  kDeltaQ,
  kR1,
  kR2,
  kR,
  kTPCNSigmaEl1,
  kTPCNSigmaPi1,
  kTPCNSigmaPr1,
  kTPCNSigmaEl2,
  kTPCNSigmaPi2,
  kTPCNSigmaPr2,
  kDitrackMass,
  kVertexingChi2PCA,
  kVertexingLxyProjected,
  kVertexingLzProjected
};

template <typename TypeOutputScore = float>
class QuadMlResponse : public MlResponse<TypeOutputScore>
{
 public:
  QuadMlResponse() = default;
  virtual ~QuadMlResponse() = default;

  template <typename T1, typename T2>
  float returnFeature(uint8_t idx, T1 const& lepton1, T2 const& lepton2, const float* fValuesQuadruplet)
  {
    float inputFeature = 0.;
    switch (idx) {
      CHECK_AND_FILL_QUAD(kDeltaQ, kQ);
      CHECK_AND_FILL_QUAD(kR1, kDeltaR1);
      CHECK_AND_FILL_QUAD(kR2, kDeltaR2);
      CHECK_AND_FILL_QUAD(kR, kDeltaR);
      CHECK_AND_FILL_LEPTON1(kTPCNSigmaEl1, tpcNSigmaEl);
      CHECK_AND_FILL_LEPTON1(kTPCNSigmaPi1, tpcNSigmaPi);
      CHECK_AND_FILL_LEPTON1(kTPCNSigmaPr1, tpcNSigmaPr);
      CHECK_AND_FILL_LEPTON2(kTPCNSigmaEl2, tpcNSigmaEl);
      CHECK_AND_FILL_LEPTON2(kTPCNSigmaPi2, tpcNSigmaPi);
      CHECK_AND_FILL_LEPTON2(kTPCNSigmaPr2, tpcNSigmaPr);
      CHECK_AND_FILL_QUAD(kDitrackMass, kDitrackMass);
      CHECK_AND_FILL_QUAD(kVertexingChi2PCA, kVertexingChi2PCA);
      CHECK_AND_FILL_QUAD(kVertexingLxyProjected, kVertexingLxyProjected);
      CHECK_AND_FILL_QUAD(kVertexingLzProjected, kVertexingLzProjected);
    }
    return inputFeature;
  }

  /// Method to get the input features vector needed for ML inference
  /// \return inputFeatures vector
  template <typename T1, typename T2>
  std::vector<float> getInputFeatures(T1 const& lepton1, T2 const& lepton2, const float* fValuesQuadruplet)
  {
    std::vector<float> inputFeatures;
    for (const auto& idx : MlResponse<TypeOutputScore>::mCachedIndices) {
      float inputFeature = returnFeature(idx, lepton1, lepton2, fValuesQuadruplet);
      inputFeatures.emplace_back(inputFeature);
    }
    return inputFeatures;
  }

 protected:
 /// Method to fill the map of available input features
  void setAvailableInputFeatures()
  {
    MlResponse<TypeOutputScore>::mAvailableInputFeatures = {
      FILL_MAP_QUAD(kDeltaQ),
      FILL_MAP_QUAD(kR1),
      FILL_MAP_QUAD(kR2),
      FILL_MAP_QUAD(kR),
      FILL_MAP_QUAD(kTPCNSigmaEl1),
      FILL_MAP_QUAD(kTPCNSigmaPi1),
      FILL_MAP_QUAD(kTPCNSigmaPr1),
      FILL_MAP_QUAD(kTPCNSigmaEl2),
      FILL_MAP_QUAD(kTPCNSigmaPi2),
      FILL_MAP_QUAD(kTPCNSigmaPr2),
      FILL_MAP_QUAD(kDitrackMass),
      FILL_MAP_QUAD(kVertexingChi2PCA),
      FILL_MAP_QUAD(kVertexingLxyProjected),
      FILL_MAP_QUAD(kVertexingLzProjected)};
  }
};

} // namespace o2::analysis

#endif // PWGDQ_CORE_DQMLRESPONSE_H_
