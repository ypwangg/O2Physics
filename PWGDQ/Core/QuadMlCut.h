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

/// \file SelectorCuts.h
/// \brief Default pT bins and cut arrays for heavy-flavour selectors and analysis tasks
///
/// \author Yiping Wang <yiping.wang@cern.ch>, University of Science and Technology of China

#ifndef PWGDQ_CORE_QUADMLCUT_H_
#define PWGDQ_CORE_QUADMLCUT_H_

#include <string> // std::string
#include <vector> // std::vector

namespace o2::analysis
{
namespace quad_cuts_ml
{
static constexpr int NBinsPt = 1;
static constexpr int NCutScores = 3;
// default values for the pT bin edges, offset by 1 from the bin numbers in
constexpr double BinsPt[NBinsPt + 1] = {
  2.,
  20.};
const auto vecBinsPt = std::vector<double>{BinsPt, BinsPt + NBinsPt + 1};

// default values for the ML model paths, one model per pT bin
static const std::vector<std::string> modelPaths = {
  ""};

// default values for the cut directions
constexpr int CutDir[NCutScores] = {0, 1, 1};
const auto vecCutDir = std::vector<int>{CutDir, CutDir + NCutScores};

// default values for the cuts
constexpr double Cuts[NBinsPt][NCutScores] = {
  {0.5, 0.5, 0.5}};

// row labels
static const std::vector<std::string> labelsPt = {
  "pT bin 0"};

// column labels
static const std::vector<std::string> labelsCutScore = {"ML socre Bkg", "ML score prompt", "ML score nonprompt"};
} // namespace quad_cuts_ml
} // namespace o2::analysis
#endif // PWGDQ_CORE_QUADMLCUT_H_