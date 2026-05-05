struct AnalysisSameEventPairing {
  Produces<aod::Dielectrons> dielectronList;
  Produces<aod::Dimuons> dimuonList;
  Produces<aod::DielectronsExtra> dielectronsExtraList;
  Produces<aod::DielectronsInfo> dielectronInfoList;
  Produces<aod::DielectronsAll> dielectronAllList;
  Produces<aod::DimuonsExtra> dimuonsExtraList;
  Produces<aod::DimuonsAll> dimuonAllList;
  Produces<aod::DileptonsMiniTreeGen> dileptonMiniTreeGen;
  Produces<aod::DileptonsMiniTreeRec> dileptonMiniTreeRec;
  Produces<aod::DileptonsInfo> dileptonInfoList;
  Produces<aod::JPsieeCandidates> PromptNonPromptSepTable;
  Produces<aod::OniaMCTruth> MCTruthTableEffi;
  Produces<aod::DileptonPolarization> dileptonPolarList;

  o2::base::MatLayerCylSet* fLUT = nullptr;
  int fCurrentRun; // needed to detect if the run changed and trigger update of calibrations etc.
  
  OutputObj<THashList> fOutputList{"output"};

  struct : ConfigurableGroup {
    Configurable<std::string> track{"cfgTrackCuts", "jpsiO2MCdebugCuts2", "Comma separated list of barrel track cuts"};
    Configurable<std::string> muon{"cfgMuonCuts", "", "Comma separated list of muon cuts"};
    Configurable<std::string> pair{"cfgPairCuts", "", "Comma separated list of pair cuts, !!! Use only if you know what you are doing, otherwise leave empty"};
    Configurable<std::string> MCgenAcc{"cfgMCGenAccCuts", "", "Comma separated list of MC generated particles acceptance cuts, !!! Use only if you know what you are doing, otherwise leave empty"};
    // TODO: Add pair cuts via JSON
  } fConfigCuts;

  Configurable<bool> fConfigQA{"cfgQA", false, "If true, fill QA histograms"};
  Configurable<std::string> fConfigAddSEPHistogram{"cfgAddSEPHistogram", "", "Comma separated list of histograms"};
  Configurable<std::string> fConfigAddJSONHistograms{"cfgAddJSONHistograms", "", "Histograms in JSON format"};
  struct : ConfigurableGroup {
    Configurable<std::string> url{"ccdb-url", "http://alice-ccdb.cern.ch", "url of the ccdb repository"};
    Configurable<std::string> grpMagPath{"grpmagPath", "GLO/Config/GRPMagField", "CCDB path of the GRPMagField object"};
    Configurable<std::string> lutPath{"lutPath", "GLO/Param/MatLUT", "Path of the Lut parametrization"};
    Configurable<std::string> geoPath{"geoPath", "GLO/Config/GeometryAligned", "Path of the geometry file"};
  } fConfigCCDB;
  struct : ConfigurableGroup {
    Configurable<bool> useRemoteField{"cfgUseRemoteField", false, "Chose whether to fetch the magnetic field from ccdb or set it manually"};
    Configurable<float> magField{"cfgMagField", 5.0f, "Manually set magnetic field"};
    Configurable<bool> flatTables{"cfgFlatTables", false, "Produce a single flat tables with all relevant information of the pairs and single tracks"};
    Configurable<bool> polarTables{"cfgPolarTables", false, "Produce tables with dilepton polarization information"};
    Configurable<bool> useKFVertexing{"cfgUseKFVertexing", false, "Use KF Particle for secondary vertex reconstruction (DCAFitter is used by default)"};
    Configurable<bool> useAbsDCA{"cfgUseAbsDCA", false, "Use absolute DCA minimization instead of chi^2 minimization in secondary vertexing"};
    Configurable<bool> propToPCA{"cfgPropToPCA", false, "Propagate tracks to secondary vertex"};
    Configurable<bool> corrFullGeo{"cfgCorrFullGeo", false, "Use full geometry to correct for MCS effects in track propagation"};
    Configurable<bool> noCorr{"cfgNoCorrFwdProp", false, "Do not correct for MCS effects in track propagation"};
    Configurable<std::string> collisionSystem{"syst", "pp", "Collision system, pp or PbPb"};
    Configurable<float> centerMassEnergy{"energy", 13600, "Center of mass energy in GeV"};
  } fConfigOptions;
  struct : ConfigurableGroup {
    Configurable<std::string> genSignals{"cfgBarrelMCGenSignals", "", "Comma separated list of MC signals (generated)"};
    Configurable<std::string> genSignalsJSON{"cfgMCGenSignalsJSON", "", "Additional list of MC signals (generated) via JSON"};
    Configurable<std::string> recSignals{"cfgBarrelMCRecSignals", "", "Comma separated list of MC signals (reconstructed)"};
    Configurable<std::string> recSignalsJSON{"cfgMCRecSignalsJSON", "", "Comma separated list of MC signals (reconstructed) via JSON"};
    Configurable<bool> skimSignalOnly{"cfgSkimSignalOnly", false, "Configurable to select only matched candidates"};
  } fConfigMC;
  struct : ConfigurableGroup {
    Configurable<bool> fConfigMiniTree{"useMiniTree.cfgMiniTree", false, "Produce a single flat table with minimal information for analysis"};
    Configurable<float> fConfigMiniTreeMinMass{"useMiniTree.cfgMiniTreeMinMass", 2, "Min. mass cut for minitree"};
    Configurable<float> fConfigMiniTreeMaxMass{"useMiniTree.cfgMiniTreeMaxMass", 5, "Max. mass cut for minitree"};
  } useMiniTree;
  // Track related options
  Configurable<bool> fPropTrack{"cfgPropTrack", true, "Propgate tracks to associated collision to recalculate DCA and momentum vector"};
  Service<o2::ccdb::BasicCCDBManager> fCCDB;
  // Filter filterEventSelected = aod::dqanalysisflags::isEventSelected & uint32_t(1);
  Filter eventFilter = aod::dqanalysisflags::isEventSelected > static_cast<uint32_t>(0);
  HistogramManager* fHistMan;
  // keep histogram class names in maps, so we don't have to buld their names in the pair loops
  std::map<int, std::vector<TString>> fTrackHistNames;
  std::map<int, std::vector<TString>> fBarrelHistNamesMCmatched;
  std::map<int, std::vector<TString>> fMuonHistNames;
  std::map<int, std::vector<TString>> fMuonHistNamesMCmatched;
  std::vector<MCSignal*> fRecMCSignals;
  std::vector<MCSignal*> fGenMCSignals;

  std::vector<AnalysisCompositeCut> fPairCuts;
  std::vector<AnalysisCut*> fMCGenAccCuts;
  bool fUseMCGenAccCut = false;

  uint32_t fTrackFilterMask; // mask for the track cuts required in this task to be applied on the barrel cuts produced upstream
  uint32_t fMuonFilterMask;  // mask for the muon cuts required in this task to be applied on the muon cuts produced upstream
  int fNCutsBarrel;
  int fNCutsMuon;
  int fNPairCuts;
  bool fHasTwoProngGenMCsignals = false;
  bool fEnableBarrelHistos;
  bool fEnableMuonHistos;
  Preslice<soa::Join<aod::ReducedTracksAssoc, aod::BarrelTrackCuts, aod::Prefilter>> trackAssocsPerCollision = aod::reducedtrack_association::reducedeventId;
  Preslice<soa::Join<aod::ReducedMuonsAssoc, aod::MuonTrackCuts>> muonAssocsPerCollision = aod::reducedtrack_association::reducedeventId;
  void init(o2::framework::InitContext& context)
  {
    if (context.mOptions.get<bool>("processDummy")) {
      return;
    }
    bool isMCGen = context.mOptions.get<bool>("processMCGen") || context.mOptions.get<bool>("processMCGenWithGrouping") || context.mOptions.get<bool>("processBarrelOnlySkimmed");
    VarManager::SetDefaultVarNames();
    fEnableBarrelHistos = context.mOptions.get<bool>("processAllSkimmed") || context.mOptions.get<bool>("processBarrelOnlySkimmed") || context.mOptions.get<bool>("processBarrelOnlyWithCollSkimmed");
    fEnableMuonHistos = context.mOptions.get<bool>("processAllSkimmed") || context.mOptions.get<bool>("processMuonOnlySkimmed");
    // Keep track of all the histogram class names to avoid composing strings in the pairing loop
    TString histNames = "";
    TString cutNamesStr = fConfigCuts.pair.value;
    if (!cutNamesStr.IsNull()) {
      std::unique_ptr<TObjArray> objArray(cutNamesStr.Tokenize(","));
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        fPairCuts.push_back(*dqcuts::GetCompositeCut(objArray->At(icut)->GetName()));
      }
    }
    // get the list of cuts for tracks/muons, check that they were played by the barrel/muon selection tasks
    //   and make a mask for active cuts (barrel and muon selection tasks may run more cuts, needed for other analyses)
    TString trackCutsStr = fConfigCuts.track.value;
    TObjArray* objArrayTrackCuts = nullptr;
    if (!trackCutsStr.IsNull()) {
      objArrayTrackCuts = trackCutsStr.Tokenize(",");
    }
    TString muonCutsStr = fConfigCuts.muon.value;
    TObjArray* objArrayMuonCuts = nullptr;
    if (!muonCutsStr.IsNull()) {
      objArrayMuonCuts = muonCutsStr.Tokenize(",");
    }
    // Setting the MC rec signal names
    TString sigNamesStr = fConfigMC.recSignals.value;
    std::unique_ptr<TObjArray> objRecSigArray(sigNamesStr.Tokenize(","));
    for (int isig = 0; isig < objRecSigArray->GetEntries(); ++isig) {
      MCSignal* sig = o2::aod::dqmcsignals::GetMCSignal(objRecSigArray->At(isig)->GetName());
      if (sig) {
        if (sig->GetNProngs() != 2) { // NOTE: 2-prong signals required
          continue;
        }
        fRecMCSignals.push_back(sig);
      }
    }
    // Add the MCSignals from the JSON config
    TString addMCSignalsStr = fConfigMC.recSignalsJSON.value;
    if (addMCSignalsStr != "") {
      std::vector<MCSignal*> addMCSignals = dqmcsignals::GetMCSignalsFromJSON(addMCSignalsStr.Data());
      for (auto& mcIt : addMCSignals) {
        if (mcIt->GetNProngs() != 2) { // NOTE: only 2 prong signals
          continue;
        }
        fRecMCSignals.push_back(mcIt);
      }
    }
    // get the barrel track selection cuts
    string tempCuts;
    getTaskOptionValue<string>(context, "analysis-track-selection", "cfgTrackCuts", tempCuts, false);
    TString tempCutsStr = tempCuts;
    // check also the cuts added via JSON and add them to the string of cuts
    getTaskOptionValue<string>(context, "analysis-track-selection", "cfgBarrelTrackCutsJSON", tempCuts, false);
    TString addTrackCutsStr = tempCuts;
    if (addTrackCutsStr != "") {
      std::vector<AnalysisCut*> addTrackCuts = dqcuts::GetCutsFromJSON(addTrackCutsStr.Data());
      for (auto& t : addTrackCuts) {
        tempCutsStr += Form(",%s", t->GetName());
      }
    }
    // check that the barrel track cuts array required in this task is not empty
    if (!trackCutsStr.IsNull()) {
      // tokenize and loop over the barrel cuts produced by the barrel track selection task
      std::unique_ptr<TObjArray> objArray(tempCutsStr.Tokenize(","));
      fNCutsBarrel = objArray->GetEntries();
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        TString tempStr = objArray->At(icut)->GetName();
        // if the current barrel selection cut is required in this task, then switch on the corresponding bit in the mask
        // and assign histogram directories
        if (objArrayTrackCuts->FindObject(tempStr.Data()) != nullptr) {
          fTrackFilterMask |= (static_cast<uint32_t>(1) << icut);
          if (fEnableBarrelHistos) {
            // assign the pair hist directories for the current cut
            std::vector<TString> names = {
              Form("PairsBarrelSEPM_%s", objArray->At(icut)->GetName()),
              Form("PairsBarrelSEPP_%s", objArray->At(icut)->GetName()),
              Form("PairsBarrelSEMM_%s", objArray->At(icut)->GetName())};
            if (fConfigQA) {
              // assign separate hist directories for ambiguous tracks
              names.push_back(Form("PairsBarrelSEPM_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsBarrelSEPP_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsBarrelSEMM_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsBarrelSEPM_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsBarrelSEPP_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsBarrelSEMM_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
            }
            for (auto& n : names) {
              histNames += Form("%s;", n.Data());
            }
            fTrackHistNames[icut] = names;
            // if there are pair cuts specified, assign hist directories for each barrel cut - pair cut combination
            // NOTE: This could possibly lead to large histogram outputs. It is strongly advised to use pair cuts only
            //   if you know what you are doing.
            TString cutNamesStr = fConfigCuts.pair.value;
            if (!cutNamesStr.IsNull()) { // if pair cuts
              std::unique_ptr<TObjArray> objArrayPair(cutNamesStr.Tokenize(","));
              fNPairCuts = objArrayPair->GetEntries();
              for (int iPairCut = 0; iPairCut < fNPairCuts; ++iPairCut) { // loop over pair cuts
                names = {
                  Form("PairsBarrelSEPM_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName()),
                  Form("PairsBarrelSEPP_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName()),
                  Form("PairsBarrelSEMM_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName())};
                histNames += Form("%s;%s;%s;", names[0].Data(), names[1].Data(), names[2].Data());
                // NOTE: In the numbering scheme for the map key, we use the number of barrel cuts in the barrel-track selection task
                fTrackHistNames[fNCutsBarrel + icut * fNPairCuts + iPairCut] = names;
              } // end loop (pair cuts)
            } // end if (pair cuts)
            // assign hist directories for the MC matched pairs for each (track cut,MCsignal) combination
            if (!sigNamesStr.IsNull()) {
              for (unsigned int isig = 0; isig < fRecMCSignals.size(); isig++) {
                auto sig = fRecMCSignals.at(isig);
                names = {
                  Form("PairsBarrelSEPM_%s_%s", objArray->At(icut)->GetName(), sig->GetName()),
                  Form("PairsBarrelSEPP_%s_%s", objArray->At(icut)->GetName(), sig->GetName()),
                  Form("PairsBarrelSEMM_%s_%s", objArray->At(icut)->GetName(), sig->GetName())};
                if (fConfigQA) {
                  names.push_back(Form("PairsBarrelSEPMCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPMIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousInBunch_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousInBunchCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousInBunchIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousOutOfBunch_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousOutOfBunchCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsBarrelSEPM_ambiguousOutOfBunchIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                }
                for (auto& n : names) {
                  histNames += Form("%s;", n.Data());
                }
                fBarrelHistNamesMCmatched.try_emplace(icut * fRecMCSignals.size() + isig, names);
              } // end loop over MC signals
            }
          } // end if enableBarrelHistos
        }
      }
    }
    // get the muon track selection cuts
    getTaskOptionValue<string>(context, "analysis-muon-selection", "cfgMuonCuts", tempCuts, false);
    tempCutsStr = tempCuts;
    // check also the cuts added via JSON and add them to the string of cuts
    getTaskOptionValue<string>(context, "analysis-muon-selection", "cfgMuonCutsJSON", tempCuts, false);
    TString addMuonCutsStr = tempCuts;
    if (addMuonCutsStr != "") {
      std::vector<AnalysisCut*> addMuonCuts = dqcuts::GetCutsFromJSON(addMuonCutsStr.Data());
      for (auto& t : addMuonCuts) {
        tempCutsStr += Form(",%s", t->GetName());
      }
    }
    // check that in this task we have specified muon cuts
    if (!muonCutsStr.IsNull()) {
      // loop over the muon cuts computed by the muon selection task and build a filter mask for those required in this task
      std::unique_ptr<TObjArray> objArray(tempCutsStr.Tokenize(","));
      fNCutsMuon = objArray->GetEntries();
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        TString tempStr = objArray->At(icut)->GetName();
        if (objArrayMuonCuts->FindObject(tempStr.Data()) != nullptr) {
          // update the filter mask
          fMuonFilterMask |= (static_cast<uint32_t>(1) << icut);
          if (fEnableMuonHistos) {
            // assign pair hist directories for each required muon cut
            std::vector<TString> names = {
              Form("PairsMuonSEPM_%s", objArray->At(icut)->GetName()),
              Form("PairsMuonSEPP_%s", objArray->At(icut)->GetName()),
              Form("PairsMuonSEMM_%s", objArray->At(icut)->GetName())};
            if (fConfigQA) {
              // assign separate hist directories for ambiguous tracks
              names.push_back(Form("PairsMuonSEPM_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsMuonSEPP_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsMuonSEMM_ambiguousInBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsMuonSEPM_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsMuonSEPP_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
              names.push_back(Form("PairsMuonSEMM_ambiguousOutOfBunch_%s", objArray->At(icut)->GetName()));
            }
            for (auto& n : names) {
              histNames += Form("%s;", n.Data());
            }
            fMuonHistNames[icut] = names;
            // if there are specified pair cuts, assign hist dirs for each muon cut - pair cut combination
            TString cutNamesStr = fConfigCuts.pair.value;
            if (!cutNamesStr.IsNull()) { // if pair cuts
              std::unique_ptr<TObjArray> objArrayPair(cutNamesStr.Tokenize(","));
              fNPairCuts = objArrayPair->GetEntries();
              for (int iPairCut = 0; iPairCut < fNPairCuts; ++iPairCut) { // loop over pair cuts
                names = {
                  Form("PairsMuonSEPM_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName()),
                  Form("PairsMuonSEPP_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName()),
                  Form("PairsMuonSEMM_%s_%s", objArray->At(icut)->GetName(), objArrayPair->At(iPairCut)->GetName())};
                histNames += Form("%s;%s;%s;", names[0].Data(), names[1].Data(), names[2].Data());
                fMuonHistNames[fNCutsMuon + icut * fNCutsMuon + iPairCut] = names;
              } // end loop (pair cuts)
            } // end if (pair cuts)
            // assign hist directories for pairs matched to MC signals for each (muon cut, MCrec signal) combination
            if (!sigNamesStr.IsNull()) {
              for (unsigned int isig = 0; isig < fRecMCSignals.size(); isig++) {
                auto sig = fRecMCSignals.at(isig);
                names = {
                  Form("PairsMuonSEPM_%s_%s", objArray->At(icut)->GetName(), sig->GetName()),
                  Form("PairsMuonSEPP_%s_%s", objArray->At(icut)->GetName(), sig->GetName()),
                  Form("PairsMuonSEMM_%s_%s", objArray->At(icut)->GetName(), sig->GetName()),
                };
                if (fConfigQA) {
                  names.push_back(Form("PairsMuonSEPMCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPMIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousInBunch_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousInBunchCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousInBunchIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousOutOfBunch_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousOutOfBunchCorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                  names.push_back(Form("PairsMuonSEPM_ambiguousOutOfBunchIncorrectAssoc_%s_%s", objArray->At(icut)->GetName(), sig->GetName()));
                }
                for (auto& n : names) {
                  histNames += Form("%s;", n.Data());
                }
                fMuonHistNamesMCmatched.try_emplace(icut * fRecMCSignals.size() + isig, names);
              } // end loop over MC signals
            }
          }
        }
        } // end loop over cuts
    } // end if (muonCutsStr)

    // get the mc generated acceptance cuts
    TString mcgenCutsStr = fConfigCuts.MCgenAcc.value;
    if (!mcgenCutsStr.IsNull()) {
      std::unique_ptr<TObjArray> objArray(mcgenCutsStr.Tokenize(","));
      for (int icut = 0; icut < objArray->GetEntries(); ++icut) {
        AnalysisCut* cut = dqcuts::GetAnalysisCut(objArray->At(icut)->GetName());
        if (cut != nullptr) {
          fMCGenAccCuts.push_back(cut);
        }
      }
      fUseMCGenAccCut = true;
    }

    // Add histogram classes for each specified MCsignal at the generator level
    // TODO: create a std::vector of hist classes to be used at Fill time, to avoid using Form in the process function
    TString sigGenNamesStr = fConfigMC.genSignals.value;
    std::unique_ptr<TObjArray> objGenSigArray(sigGenNamesStr.Tokenize(","));
    for (int isig = 0; isig < objGenSigArray->GetEntries(); isig++) {
      MCSignal* sig = o2::aod::dqmcsignals::GetMCSignal(objGenSigArray->At(isig)->GetName());
      if (sig) {
        fGenMCSignals.push_back(sig);
      }
    }
    // Add the MCSignals from the JSON config
    TString addMCSignalsGenStr = fConfigMC.genSignalsJSON.value;
    if (addMCSignalsGenStr != "") {
      std::vector<MCSignal*> addMCSignals = dqmcsignals::GetMCSignalsFromJSON(addMCSignalsGenStr.Data());
      for (auto& mcIt : addMCSignals) {
        if (mcIt->GetNProngs() > 2) { // NOTE: only 2 prong signals
          continue;
        }
        fGenMCSignals.push_back(mcIt);
      }
    }
    if (isMCGen) {
      for (auto& sig : fGenMCSignals) {
        if (sig->GetNProngs() == 1) {
          histNames += Form("MCTruthGen_%s;", sig->GetName()); // TODO: Add these names to a std::vector to avoid using Form in the process function
          histNames += Form("MCTruthGenSel_%s;", sig->GetName());
        } else if (sig->GetNProngs() == 2) {
          histNames += Form("MCTruthGenPair_%s;", sig->GetName());
          histNames += Form("MCTruthGenPairSel_%s;", sig->GetName());
          fHasTwoProngGenMCsignals = true;
          // for these pair level signals, also add histograms for each MCgenAcc cut if specified
          if (fUseMCGenAccCut) {
            for (auto& cut : fMCGenAccCuts) {
              histNames += Form("MCTruthGenPairSel_%s_%s;", sig->GetName(), cut->GetName());
            }
          }
        }
      }
    }
    fCurrentRun = 0;
    fCCDB->setURL(fConfigCCDB.url.value);
    fCCDB->setCaching(true);
    fCCDB->setLocalObjectValidityChecking();
    if (fConfigOptions.noCorr) {
      VarManager::SetupFwdDCAFitterNoCorr();
    } else if (fConfigOptions.corrFullGeo || (fConfigOptions.useKFVertexing && fConfigOptions.propToPCA)) {
      if (!o2::base::GeometryManager::isGeometryLoaded()) {
        fCCDB->get<TGeoManager>(fConfigCCDB.geoPath);
      }
    } else {
      fLUT = o2::base::MatLayerCylSet::rectifyPtrFromFile(fCCDB->get<o2::base::MatLayerCylSet>(fConfigCCDB.lutPath));
      VarManager::SetupMatLUTFwdDCAFitter(fLUT);
    }
    fHistMan = new HistogramManager("analysisHistos", "aa", VarManager::kNVars);
    fHistMan->SetUseDefaultVariableNames(kTRUE);
    fHistMan->SetDefaultVarNames(VarManager::fgVariableNames, VarManager::fgVariableUnits);
    VarManager::SetCollisionSystem((TString)fConfigOptions.collisionSystem, fConfigOptions.centerMassEnergy); // set collision system and center of mass energy
    DefineHistograms(fHistMan, histNames.Data(), fConfigAddSEPHistogram.value.data());     // define all histograms
    dqhistograms::AddHistogramsFromJSON(fHistMan, fConfigAddJSONHistograms.value.c_str()); // ad-hoc histograms via JSON
    VarManager::SetUseVars(fHistMan->GetUsedVars());                                       // provide the list of required variables so that VarManager knows what to fill
    fOutputList.setObject(fHistMan->GetMainHistogramList());
  }
  void initParamsFromCCDB(uint64_t timestamp, bool withTwoProngFitter = true)
  {
    if (fConfigOptions.useRemoteField.value) {
      o2::parameters::GRPMagField* grpmag = fCCDB->getForTimeStamp<o2::parameters::GRPMagField>(fConfigCCDB.grpMagPath, timestamp);
      float magField = 0.0;
      if (grpmag != nullptr) {
        magField = grpmag->getNominalL3Field();
      } else {
        LOGF(fatal, "GRP object is not available in CCDB at timestamp=%llu", timestamp);
      }
      if (withTwoProngFitter) {
        if (fConfigOptions.useKFVertexing.value) {
          VarManager::SetupTwoProngKFParticle(magField);
        } else {
          VarManager::SetupTwoProngDCAFitter(magField, true, 200.0f, 4.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value); // TODO: get these parameters from Configurables
          VarManager::SetupTwoProngFwdDCAFitter(magField, true, 200.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value);
        }
      } else {
        VarManager::SetupTwoProngDCAFitter(magField, true, 200.0f, 4.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value); // needed because take in varmanager Bz from fgFitterTwoProngBarrel for PhiV calculations
      }
    } else {
      if (withTwoProngFitter) {
        if (fConfigOptions.useKFVertexing.value) {
          VarManager::SetupTwoProngKFParticle(fConfigOptions.magField.value);
        } else {
          VarManager::SetupTwoProngDCAFitter(fConfigOptions.magField.value, true, 200.0f, 4.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value); // TODO: get these parameters from Configurables
          VarManager::SetupTwoProngFwdDCAFitter(fConfigOptions.magField.value, true, 200.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value);
        }
      } else {
        VarManager::SetupTwoProngDCAFitter(fConfigOptions.magField.value, true, 200.0f, 4.0f, 1.0e-3f, 0.9f, fConfigOptions.useAbsDCA.value); // needed because take in varmanager Bz from fgFitterTwoProngBarrel for PhiV calculations
      }
    }
  }
  // Template function to run same event pairing (barrel-barrel, muon-muon, barrel-muon)
  template <bool TTwoProngFitter, int TPairType, uint32_t TEventFillMap, uint32_t TTrackFillMap, typename TEvents, typename TTrackAssocs, typename TTracks>
  void runSameEventPairing(TEvents const& events, Preslice<TTrackAssocs>& preslice, TTrackAssocs const& assocs, TTracks const& /*tracks*/, ReducedMCEvents const& /*mcEvents*/, ReducedMCTracks const& /*mcTracks*/)
  {
    if (events.size() == 0) {
      LOG(warning) << "No events in this TF, going to the next one ...";
      return;
    }
    if (fCurrentRun != events.begin().runNumber()) {
      initParamsFromCCDB(events.begin().timestamp(), TTwoProngFitter);
      fCurrentRun = events.begin().runNumber();
    }
    TString cutNames = fConfigCuts.track.value;
    std::map<int, std::vector<TString>> histNames = fTrackHistNames;
    std::map<int, std::vector<TString>> histNamesMC = fBarrelHistNamesMCmatched;
    int ncuts = fNCutsBarrel;
    if constexpr (TPairType == VarManager::kDecayToMuMu) {
      cutNames = fConfigCuts.muon.value;
      histNames = fMuonHistNames;
      histNamesMC = fMuonHistNamesMCmatched;
      ncuts = fNCutsMuon;
    }
    uint32_t twoTrackFilter = static_cast<uint32_t>(0);
    int sign1 = 0;
    int sign2 = 0;
    uint32_t mcDecision = static_cast<uint32_t>(0);
    bool isCorrectAssoc_leg1 = false;
    bool isCorrectAssoc_leg2 = false;
    dielectronList.reserve(1);
    dimuonList.reserve(1);
    dielectronsExtraList.reserve(1);
    dimuonsExtraList.reserve(1);
    dielectronInfoList.reserve(1);
    dileptonInfoList.reserve(1);
    if (fConfigOptions.flatTables.value) {
      dielectronAllList.reserve(1);
      dimuonAllList.reserve(1);
    }
    if (useMiniTree.fConfigMiniTree) {
      dileptonMiniTreeGen.reserve(1);
      dileptonMiniTreeRec.reserve(1);
    }
    if (fConfigOptions.polarTables.value) {
      dileptonPolarList.reserve(1);
    }
    constexpr bool eventHasQvector = ((TEventFillMap & VarManager::ObjTypes::ReducedEventQvector) > 0);
    constexpr bool trackHasCov = ((TTrackFillMap & VarManager::ObjTypes::ReducedTrackBarrelCov) > 0);

    for (auto& event : events) {
      if (!event.isEventSelected_bit(0)) {
        continue;
      }
      uint8_t evSel = event.isEventSelected_raw();
      // Reset the fValues array
      VarManager::ResetValues(0, VarManager::kNVars);
      VarManager::FillEvent<gkEventFillMap>(event, VarManager::fgValues);
      VarManager::FillEvent<VarManager::ObjTypes::ReducedEventMC>(event.reducedMCevent(), VarManager::fgValues);
      auto groupedAssocs = assocs.sliceBy(preslice, event.globalIndex());
      if (groupedAssocs.size() == 0) {
        continue;
      }
      for (auto& [a1, a2] : o2::soa::combinations(groupedAssocs, groupedAssocs)) {
        if constexpr (TPairType == VarManager::kDecayToEE) {
          twoTrackFilter = a1.isBarrelSelected_raw() & a2.isBarrelSelected_raw() & a1.isBarrelSelectedPrefilter_raw() & a2.isBarrelSelectedPrefilter_raw() & fTrackFilterMask;
          if (!twoTrackFilter) { // the tracks must have at least one filter bit in common to continue
            continue;
          }
          auto t1 = a1.template reducedtrack_as<TTracks>();
          auto t2 = a2.template reducedtrack_as<TTracks>();
          sign1 = t1.sign();
          sign2 = t2.sign();
          // store the ambiguity number of the two dilepton legs in the last 4 digits of the two-track filter
          if (t1.barrelAmbiguityInBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 28);
          }
          if (t2.barrelAmbiguityInBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 29);
          }
          if (t1.barrelAmbiguityOutOfBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 30);
          }
          if (t2.barrelAmbiguityOutOfBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 31);
          }
          // run MC matching for this pair
          int isig = 0;
          mcDecision = 0;
          for (auto sig = fRecMCSignals.begin(); sig != fRecMCSignals.end(); sig++, isig++) {
            if (t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
              if ((*sig)->CheckSignal(true, t1.reducedMCTrack(), t2.reducedMCTrack())) {
                mcDecision |= (static_cast<uint32_t>(1) << isig);
              }
            }
          } // end loop over MC signals
          if (t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
            isCorrectAssoc_leg1 = (t1.reducedMCTrack().reducedMCevent() == event.reducedMCevent());
            isCorrectAssoc_leg2 = (t2.reducedMCTrack().reducedMCevent() == event.reducedMCevent());
          }
          VarManager::FillPair<TPairType, TTrackFillMap>(t1, t2);
          if (fPropTrack) {
            VarManager::FillPairCollision<TPairType, TTrackFillMap>(event, t1, t2);
          }
          if constexpr (TTwoProngFitter) {
            VarManager::FillPairVertexing<TPairType, TEventFillMap, TTrackFillMap>(event, t1, t2, fConfigOptions.propToPCA);
          }
          if constexpr (eventHasQvector) {
            VarManager::FillPairVn<TPairType>(t1, t2);
          }
          if (!fConfigMC.skimSignalOnly || (fConfigMC.skimSignalOnly && mcDecision > 0)) {
            dielectronList(event.globalIndex(), VarManager::fgValues[VarManager::kMass],
                           VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi],
                           t1.sign() + t2.sign(), twoTrackFilter, mcDecision);
            if constexpr ((TTrackFillMap & VarManager::ObjTypes::ReducedTrackCollInfo) > 0) {
              dielectronInfoList(t1.collisionId(), t1.trackId(), t2.trackId());
              dileptonInfoList(t1.collisionId(), event.posX(), event.posY(), event.posZ());
            }
            if constexpr (trackHasCov && TTwoProngFitter) {
              dielectronsExtraList(t1.globalIndex(), t2.globalIndex(), VarManager::fgValues[VarManager::kVertexingTauzProjected], VarManager::fgValues[VarManager::kVertexingLzProjected], VarManager::fgValues[VarManager::kVertexingLxyProjected]);
              if constexpr ((TTrackFillMap & VarManager::ObjTypes::ReducedTrackCollInfo) > 0) {
                if (fConfigOptions.flatTables.value && t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
                  dielectronAllList(VarManager::fgValues[VarManager::kMass], VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi], t1.sign() + t2.sign(), twoTrackFilter, mcDecision,
                                    t1.pt(), t1.eta(), t1.phi(), t1.itsClusterMap(), t1.itsChi2NCl(), t1.tpcNClsCrossedRows(), t1.tpcNClsFound(), t1.tpcChi2NCl(), t1.dcaXY(), t1.dcaZ(), t1.tpcSignal(), t1.tpcNSigmaEl(), t1.tpcNSigmaPi(), t1.tpcNSigmaPr(), t1.beta(), t1.tofNSigmaEl(), t1.tofNSigmaPi(), t1.tofNSigmaPr(),
                                    t2.pt(), t2.eta(), t2.phi(), t2.itsClusterMap(), t2.itsChi2NCl(), t2.tpcNClsCrossedRows(), t2.tpcNClsFound(), t2.tpcChi2NCl(), t2.dcaXY(), t2.dcaZ(), t2.tpcSignal(), t2.tpcNSigmaEl(), t2.tpcNSigmaPi(), t2.tpcNSigmaPr(), t2.beta(), t2.tofNSigmaEl(), t2.tofNSigmaPi(), t2.tofNSigmaPr(),
                                    VarManager::fgValues[VarManager::kKFTrack0DCAxyz], VarManager::fgValues[VarManager::kKFTrack1DCAxyz], VarManager::fgValues[VarManager::kKFDCAxyzBetweenProngs], VarManager::fgValues[VarManager::kKFTrack0DCAxy], VarManager::fgValues[VarManager::kKFTrack1DCAxy], VarManager::fgValues[VarManager::kKFDCAxyBetweenProngs],
                                    VarManager::fgValues[VarManager::kKFTrack0DeviationFromPV], VarManager::fgValues[VarManager::kKFTrack1DeviationFromPV], VarManager::fgValues[VarManager::kKFTrack0DeviationxyFromPV], VarManager::fgValues[VarManager::kKFTrack1DeviationxyFromPV],
                                    VarManager::fgValues[VarManager::kKFMass], VarManager::fgValues[VarManager::kKFChi2OverNDFGeo], VarManager::fgValues[VarManager::kVertexingLxyz], VarManager::fgValues[VarManager::kVertexingLxyzOverErr], VarManager::fgValues[VarManager::kVertexingLxy], VarManager::fgValues[VarManager::kVertexingLxyOverErr], VarManager::fgValues[VarManager::kVertexingTauxy], VarManager::fgValues[VarManager::kVertexingTauxyErr], VarManager::fgValues[VarManager::kKFCosPA], VarManager::fgValues[VarManager::kKFJpsiDCAxyz], VarManager::fgValues[VarManager::kKFJpsiDCAxy],
                                    VarManager::fgValues[VarManager::kKFPairDeviationFromPV], VarManager::fgValues[VarManager::kKFPairDeviationxyFromPV],
                                    VarManager::fgValues[VarManager::kKFMassGeoTop], VarManager::fgValues[VarManager::kKFChi2OverNDFGeoTop],
                                    VarManager::fgValues[VarManager::kVertexingTauzProjected], VarManager::fgValues[VarManager::kVertexingTauxyProjected],
                                    VarManager::fgValues[VarManager::kVertexingLzProjected], VarManager::fgValues[VarManager::kVertexingLxyProjected]);
                }
                if (fConfigOptions.polarTables.value && t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
                  dileptonPolarList(VarManager::fgValues[VarManager::kCosThetaHE], VarManager::fgValues[VarManager::kPhiHE], VarManager::fgValues[VarManager::kPhiTildeHE],
                                    VarManager::fgValues[VarManager::kCosThetaCS], VarManager::fgValues[VarManager::kPhiCS], VarManager::fgValues[VarManager::kPhiTildeCS],
                                    VarManager::fgValues[VarManager::kCosThetaPP], VarManager::fgValues[VarManager::kPhiPP], VarManager::fgValues[VarManager::kPhiTildePP],
                                    VarManager::fgValues[VarManager::kCosThetaRM],
                                    VarManager::fgValues[VarManager::kCosThetaStarTPC], VarManager::fgValues[VarManager::kCosThetaStarFT0A], VarManager::fgValues[VarManager::kCosThetaStarFT0C]);
                }
              }
            }
          }
          }
        if constexpr (TPairType == VarManager::kDecayToMuMu) {
          twoTrackFilter = a1.isMuonSelected_raw() & a2.isMuonSelected_raw() & fMuonFilterMask;
          if (!twoTrackFilter) { // the tracks must have at least one filter bit in common to continue
            continue;
          }
          auto t1 = a1.template reducedmuon_as<TTracks>();
          auto t2 = a2.template reducedmuon_as<TTracks>();
          if (t1.matchMCHTrackId() == t2.matchMCHTrackId() && t1.matchMCHTrackId() >= 0)
            continue;
          if (t1.matchMFTTrackId() == t2.matchMFTTrackId() && t1.matchMFTTrackId() >= 0)
            continue;
          sign1 = t1.sign();
          sign2 = t2.sign();
          // store the ambiguity number of the two dilepton legs in the last 4 digits of the two-track filter
          if (t1.muonAmbiguityInBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 28);
          }
          if (t2.muonAmbiguityInBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 29);
          }
          if (t1.muonAmbiguityOutOfBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 30);
          }
          if (t2.muonAmbiguityOutOfBunch() > 1) {
            twoTrackFilter |= (static_cast<uint32_t>(1) << 31);
          }
          // run MC matching for this pair
          int isig = 0;
          mcDecision = 0;
          for (auto sig = fRecMCSignals.begin(); sig != fRecMCSignals.end(); sig++, isig++) {
            if (t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
              if ((*sig)->CheckSignal(true, t1.reducedMCTrack(), t2.reducedMCTrack())) {
                mcDecision |= (static_cast<uint32_t>(1) << isig);
              }
            }
          } // end loop over MC signals
          if (t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
            isCorrectAssoc_leg1 = (t1.reducedMCTrack().reducedMCevent() == event.reducedMCevent());
            isCorrectAssoc_leg2 = (t2.reducedMCTrack().reducedMCevent() == event.reducedMCevent());
          }
          VarManager::FillPair<TPairType, TTrackFillMap>(t1, t2);
          if (fPropTrack) {
            VarManager::FillPairCollision<TPairType, TTrackFillMap>(event, t1, t2);
          }
          if constexpr (TTwoProngFitter) {
            VarManager::FillPairVertexing<TPairType, TEventFillMap, TTrackFillMap>(event, t1, t2, fConfigOptions.propToPCA);
          }
          if constexpr (eventHasQvector) {
            VarManager::FillPairVn<TPairType>(t1, t2);
          }
          dimuonList(event.globalIndex(), VarManager::fgValues[VarManager::kMass],
                     VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi],
                     t1.sign() + t2.sign(), twoTrackFilter, mcDecision);
          if constexpr ((TTrackFillMap & VarManager::ObjTypes::ReducedMuonCollInfo) > 0) {
            dileptonInfoList(t1.collisionId(), event.posX(), event.posY(), event.posZ());
          }
          if constexpr (TTwoProngFitter) {
            dimuonsExtraList(t1.globalIndex(), t2.globalIndex(), VarManager::fgValues[VarManager::kVertexingTauz], VarManager::fgValues[VarManager::kVertexingLz], VarManager::fgValues[VarManager::kVertexingLxy]);
            if (fConfigOptions.flatTables.value && t1.has_reducedMCTrack() && t2.has_reducedMCTrack()) {
              dimuonAllList(event.posX(), event.posY(), event.posZ(), event.numContrib(),
                            event.selection_raw(), evSel,
                            event.reducedMCevent().mcPosX(), event.reducedMCevent().mcPosY(), event.reducedMCevent().mcPosZ(),
                            VarManager::fgValues[VarManager::kMass],
                            mcDecision,
                            VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi], t1.sign() + t2.sign(), VarManager::fgValues[VarManager::kVertexingChi2PCA],
                            VarManager::fgValues[VarManager::kVertexingTauz], VarManager::fgValues[VarManager::kVertexingTauzErr],
                            VarManager::fgValues[VarManager::kVertexingTauxy], VarManager::fgValues[VarManager::kVertexingTauxyErr],
                            VarManager::fgValues[VarManager::kCosPointingAngle],
                            VarManager::fgValues[VarManager::kPt1], VarManager::fgValues[VarManager::kEta1], VarManager::fgValues[VarManager::kPhi1], t1.sign(),
                            VarManager::fgValues[VarManager::kPt2], VarManager::fgValues[VarManager::kEta2], VarManager::fgValues[VarManager::kPhi2], t2.sign(),
                            t1.fwdDcaX(), t1.fwdDcaY(), t2.fwdDcaX(), t2.fwdDcaY(),
                            t1.mcMask(), t2.mcMask(),
                            t1.chi2MatchMCHMID(), t2.chi2MatchMCHMID(),
                            t1.chi2MatchMCHMFT(), t2.chi2MatchMCHMFT(),
                            t1.chi2(), t2.chi2(),
                            t1.reducedMCTrack().pt(), t1.reducedMCTrack().eta(), t1.reducedMCTrack().phi(), t1.reducedMCTrack().e(),
                            t2.reducedMCTrack().pt(), t2.reducedMCTrack().eta(), t2.reducedMCTrack().phi(), t2.reducedMCTrack().e(),
                            t1.reducedMCTrack().vx(), t1.reducedMCTrack().vy(), t1.reducedMCTrack().vz(), t1.reducedMCTrack().vt(),
                            t2.reducedMCTrack().vx(), t2.reducedMCTrack().vy(), t2.reducedMCTrack().vz(), t2.reducedMCTrack().vt(),
                            (twoTrackFilter & (static_cast<uint32_t>(1) << 28)) || (twoTrackFilter & (static_cast<uint32_t>(1) << 29)), (twoTrackFilter & (static_cast<uint32_t>(1) << 30)) || (twoTrackFilter & (static_cast<uint32_t>(1) << 31)),
                            -999.0, -999.0, -999.0, -999.0, -999.0,
                            -999.0, -999.0, -999.0, -999.0, -999.0,
                            -999.0, VarManager::fgValues[VarManager::kMultDimuons],
                            VarManager::fgValues[VarManager::kVertexingPz], VarManager::fgValues[VarManager::kVertexingSV]);
            }
          }
        }
        // TODO: the model for the electron-muon combination has to be thought through
        /*if constexpr (TPairType == VarManager::kElectronMuon) {
          twoTrackFilter = a1.isBarrelSelected_raw() & a1.isBarrelSelectedPrefilter_raw() & a2.isMuonSelected_raw() & fTwoTrackFilterMask;
        }*/
        // Fill histograms
        bool isAmbiInBunch = false;
        bool isAmbiOutOfBunch = false;
        bool isCorrect_pair = false;
        if (isCorrectAssoc_leg1 && isCorrectAssoc_leg2)
          isCorrect_pair = true;
        for (int icut = 0; icut < ncuts; icut++) {
          if (twoTrackFilter & (static_cast<uint32_t>(1) << icut)) {
            isAmbiInBunch = (twoTrackFilter & (static_cast<uint32_t>(1) << 28)) || (twoTrackFilter & (static_cast<uint32_t>(1) << 29));
            isAmbiOutOfBunch = (twoTrackFilter & (static_cast<uint32_t>(1) << 30)) || (twoTrackFilter & (static_cast<uint32_t>(1) << 31));
            if (sign1 * sign2 < 0) {                                                    // +- pairs
              fHistMan->FillHistClass(histNames[icut][0].Data(), VarManager::fgValues); // reconstructed, unmatched
              for (unsigned int isig = 0; isig < fRecMCSignals.size(); isig++) {        // loop over MC signals
                if (mcDecision & (static_cast<uint32_t>(1) << isig)) {
                  PromptNonPromptSepTable(VarManager::fgValues[VarManager::kMass], VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kRap], VarManager::fgValues[VarManager::kPhi], VarManager::fgValues[VarManager::kVertexingTauxyProjected], VarManager::fgValues[VarManager::kVertexingTauxyProjectedPoleJPsiMass], VarManager::fgValues[VarManager::kVertexingTauzProjected], isAmbiInBunch, isAmbiOutOfBunch, isCorrect_pair, VarManager::fgValues[VarManager::kMultFT0A], VarManager::fgValues[VarManager::kMultFT0C], VarManager::fgValues[VarManager::kCentFT0M], VarManager::fgValues[VarManager::kVtxNcontribReal]);
                  fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][0].Data(), VarManager::fgValues); // matched signal
                  if (useMiniTree.fConfigMiniTree) {
                    if constexpr (TPairType == VarManager::kDecayToMuMu) {
                      twoTrackFilter = a1.isMuonSelected_raw() & a2.isMuonSelected_raw() & fMuonFilterMask;
                      if (!twoTrackFilter) { // the tracks must have at least one filter bit in common to continue
                        continue;
                      }
                      auto t1 = a1.template reducedmuon_as<TTracks>();
                      auto t2 = a2.template reducedmuon_as<TTracks>();
                      float dileptonMass = VarManager::fgValues[VarManager::kMass];
                      if (dileptonMass > useMiniTree.fConfigMiniTreeMinMass && dileptonMass < useMiniTree.fConfigMiniTreeMaxMass) {
                        // In the miniTree the positive daughter is positioned as first
                        if (t1.sign() > 0) {
                          dileptonMiniTreeRec(mcDecision,
                                              VarManager::fgValues[VarManager::kMass],
                                              VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi], VarManager::fgValues[VarManager::kCentFT0C],
                                              t1.reducedMCTrack().pt(), t1.reducedMCTrack().eta(), t1.reducedMCTrack().phi(),
                                              t2.reducedMCTrack().pt(), t2.reducedMCTrack().eta(), t2.reducedMCTrack().phi(),
                                              t1.pt(), t1.eta(), t1.phi(),
                                              t2.pt(), t2.eta(), t2.phi());
                        } else {
                          dileptonMiniTreeRec(mcDecision,
                                              VarManager::fgValues[VarManager::kMass],
                                              VarManager::fgValues[VarManager::kPt], VarManager::fgValues[VarManager::kEta], VarManager::fgValues[VarManager::kPhi], VarManager::fgValues[VarManager::kCentFT0C],
                                              t2.reducedMCTrack().pt(), t2.reducedMCTrack().eta(), t2.reducedMCTrack().phi(),
                                              t1.reducedMCTrack().pt(), t1.reducedMCTrack().eta(), t1.reducedMCTrack().phi(),
                                              t2.pt(), t2.eta(), t2.phi(),
                                              t1.pt(), t1.eta(), t1.phi());
                        }
                      }
                    }
                  }
                  if (fConfigQA) {
                    if (isCorrectAssoc_leg1 && isCorrectAssoc_leg2) { // correct track-collision association
                      fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][3].Data(), VarManager::fgValues);
                    } else { // incorrect track-collision association
                      fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][4].Data(), VarManager::fgValues);
                    }
                    if (isAmbiInBunch) { // ambiguous in bunch
                      fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][5].Data(), VarManager::fgValues);
                      if (isCorrectAssoc_leg1 && isCorrectAssoc_leg2) {
                        fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][6].Data(), VarManager::fgValues);
                      } else {
                        fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][7].Data(), VarManager::fgValues);
                      }
                    }
                    if (isAmbiOutOfBunch) { // ambiguous out of bunch
                      fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][8].Data(), VarManager::fgValues);
                      if (isCorrectAssoc_leg1 && isCorrectAssoc_leg2) {
                        fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][9].Data(), VarManager::fgValues);
                      } else {
                        fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][10].Data(), VarManager::fgValues);
                      }
                    }
                  }
                }
                if (fConfigQA) {
                  if (isAmbiInBunch) {
                    fHistMan->FillHistClass(histNames[icut][3].Data(), VarManager::fgValues);
                  }
                  if (isAmbiOutOfBunch) {
                    fHistMan->FillHistClass(histNames[icut][3 + 3].Data(), VarManager::fgValues);
                  }
                }
              }
            } else {
              if (sign1 > 0) { // ++ pairs
                fHistMan->FillHistClass(histNames[icut][1].Data(), VarManager::fgValues);
                for (unsigned int isig = 0; isig < fRecMCSignals.size(); isig++) { // loop over MC signals
                  if (mcDecision & (static_cast<uint32_t>(1) << isig)) {
                    fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][1].Data(), VarManager::fgValues);
                  }
                }
                if (fConfigQA) {
                  if (isAmbiInBunch) {
                    fHistMan->FillHistClass(histNames[icut][4].Data(), VarManager::fgValues);
                  }
                  if (isAmbiOutOfBunch) {
                    fHistMan->FillHistClass(histNames[icut][4 + 3].Data(), VarManager::fgValues);
                  }
                }
              } else { // -- pairs
                fHistMan->FillHistClass(histNames[icut][2].Data(), VarManager::fgValues);
                for (unsigned int isig = 0; isig < fRecMCSignals.size(); isig++) { // loop over MC signals
                  if (mcDecision & (static_cast<uint32_t>(1) << isig)) {
                    fHistMan->FillHistClass(histNamesMC[icut * fRecMCSignals.size() + isig][2].Data(), VarManager::fgValues);
                  }
                }
                if (fConfigQA) {
                  if (isAmbiInBunch) {
                    fHistMan->FillHistClass(histNames[icut][5].Data(), VarManager::fgValues);
                  }
                  if (isAmbiOutOfBunch) {
                    fHistMan->FillHistClass(histNames[icut][5 + 3].Data(), VarManager::fgValues);
                  }
                }
              }
            }
            for (unsigned int iPairCut = 0; iPairCut < fPairCuts.size(); iPairCut++) {
              AnalysisCompositeCut cut = fPairCuts.at(iPairCut);
              if (!(cut.IsSelected(VarManager::fgValues))) // apply pair cuts
                continue;
              if (sign1 * sign2 < 0) {
                fHistMan->FillHistClass(histNames[ncuts + icut * fPairCuts.size() + iPairCut][0].Data(), VarManager::fgValues);
              } else {
                if (sign1 > 0) {
                  fHistMan->FillHistClass(histNames[ncuts + icut * fPairCuts.size() + iPairCut][1].Data(), VarManager::fgValues);
                } else {
                  fHistMan->FillHistClass(histNames[ncuts + icut * fPairCuts.size() + iPairCut][2].Data(), VarManager::fgValues);
                }
              }
            } // end loop (pair cuts)
          }
        } // end loop (cuts)
      } // end loop over pairs of track associations
    } // end loop over events
  }
  PresliceUnsorted<ReducedMCTracks> perReducedMcEvent = aod::reducedtrackMC::reducedMCeventId;
  template <int TPairType>
  void runMCGenWithGrouping(MyEventsVtxCovSelected const& events, ReducedMCEvents const& mcEvents, ReducedMCTracks const& mcTracks)
  {
    uint32_t mcDecision = 0;
    int isig = 0;

    for (auto& mctrack : mcTracks) {
      VarManager::FillTrackMC(mcTracks, mctrack);
      // NOTE: Signals are checked here mostly based on the skimmed MC stack, so depending on the requested signal, the stack could be incomplete.
      // NOTE: However, the working model is that the decisions on MC signals are precomputed during skimming and are stored in the mcReducedFlags member.
      // TODO:  Use the mcReducedFlags to select signals
      for (auto& sig : fGenMCSignals) {
        if (sig->CheckSignal(true, mctrack)) {
          fHistMan->FillHistClass(Form("MCTruthGen_%s", sig->GetName()), VarManager::fgValues);
        }
      }
    }
    // Fill Generated histograms taking into account selected collisions
    for (auto& event : events) {
      if (!event.isEventSelected_bit(0)) {
        continue;
      }
      if (!event.has_reducedMCevent()) {
        continue;
      }
      for (auto& track : mcTracks) {
        if (track.reducedMCeventId() != event.reducedMCeventId()) {
          continue;
        }
        VarManager::FillTrackMC(mcTracks, track);
        auto track_raw = mcTracks.rawIteratorAt(track.globalIndex());
        mcDecision = 0;
        isig = 0;
        for (auto& sig : fGenMCSignals) {
          if (sig->CheckSignal(true, track_raw)) {
            mcDecision |= (static_cast<uint32_t>(1) << isig);
            fHistMan->FillHistClass(Form("MCTruthGenSel_%s", sig->GetName()), VarManager::fgValues);
            MCTruthTableEffi(VarManager::fgValues[VarManager::kMCPt], VarManager::fgValues[VarManager::kMCEta], VarManager::fgValues[VarManager::kMCY], VarManager::fgValues[VarManager::kMCPhi], VarManager::fgValues[VarManager::kMCVz], VarManager::fgValues[VarManager::kMCVtxZ], VarManager::fgValues[VarManager::kMultFT0A], VarManager::fgValues[VarManager::kMultFT0C], VarManager::fgValues[VarManager::kCentFT0M], VarManager::fgValues[VarManager::kVtxNcontribReal]);
            if (useMiniTree.fConfigMiniTree) {
              auto mcEvent = mcEvents.rawIteratorAt(track_raw.reducedMCeventId());
              dileptonMiniTreeGen(mcDecision, mcEvent.impactParameter(), track_raw.pt(), track_raw.eta(), track_raw.phi(), -999, -999, -999);
            }
          }
          isig++;
        }
      }
    } // end loop over reconstructed events
    if (fHasTwoProngGenMCsignals) {
      for (auto& [t1, t2] : combinations(mcTracks, mcTracks)) {
        auto t1_raw = mcTracks.rawIteratorAt(t1.globalIndex());
        auto t2_raw = mcTracks.rawIteratorAt(t2.globalIndex());
        if (t1_raw.reducedMCeventId() == t2_raw.reducedMCeventId()) {
          for (auto& sig : fGenMCSignals) {
            if (sig->GetNProngs() != 2) { // NOTE: 2-prong signals required here
              continue;
            }
             if (sig->CheckSignal(true, t1_raw, t2_raw)) {
              VarManager::FillPairMC<TPairType>(t1, t2);
              fHistMan->FillHistClass(Form("MCTruthGenPair_%s", sig->GetName()), VarManager::fgValues);
            }
          }}
      }
    }
    for (auto& event : events) {
      if (!event.isEventSelected_bit(0)) {
        continue;
      }
      if (!event.has_reducedMCevent()) {
        continue;
      }
      // CURRENTLY ONLY FOR 1-GENERATION 2-PRONG SIGNALS
      if (fHasTwoProngGenMCsignals) {
        auto groupedMCTracks = mcTracks.sliceBy(perReducedMcEvent, event.reducedMCeventId());
        groupedMCTracks.bindInternalIndicesTo(&mcTracks);
        for (auto& [t1, t2] : combinations(groupedMCTracks, groupedMCTracks)) {
          auto t1_raw = mcTracks.rawIteratorAt(t1.globalIndex());
          auto t2_raw = mcTracks.rawIteratorAt(t2.globalIndex());
          if (t1_raw.reducedMCeventId() == t2_raw.reducedMCeventId()) {
            mcDecision = 0;
            isig = 0;
            for (auto& sig : fGenMCSignals) {
              if (sig->GetNProngs() != 2) { // NOTE: 2-prong signals required here
                continue;
              }
              if (sig->CheckSignal(true, t1_raw, t2_raw)) {
                mcDecision |= (static_cast<uint32_t>(1) << isig);
                VarManager::FillPairMC<TPairType>(t1, t2);
                fHistMan->FillHistClass(Form("MCTruthGenPairSel_%s", sig->GetName()), VarManager::fgValues);
                // Fill also acceptance cut histograms if requested
                if (fUseMCGenAccCut) {
                  for (auto& cut : fMCGenAccCuts) {
                    if (cut->IsSelected(VarManager::fgValues)) {
                      fHistMan->FillHistClass(Form("MCTruthGenPairSel_%s_%s", sig->GetName(), cut->GetName()), VarManager::fgValues);
                    }
                  }
                }
                if (useMiniTree.fConfigMiniTree) {
                  // WARNING! To be checked
                  dileptonMiniTreeGen(mcDecision, -999, t1.pt(), t1.eta(), t1.phi(), t2.pt(), t2.eta(), t2.phi());
  
                }
              }
              isig++;
            }
          }
        }
      } // end loop over reconstructed events
    }
  }