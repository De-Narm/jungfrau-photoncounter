#pragma once
#include "helpers.hpp"

template <typename Config> struct PhotonFinderKernel {
  template <typename TAcc, typename TDetectorData, typename TGainMap,
            typename TInitPedestalMap, typename TPedestalMap,
            typename TGainStageMap, typename TEnergyMap, typename TPhotonMap,
            typename TNumFrames, typename TBeamConst, typename TMask,
            typename TNumStdDevs = int>
  ALPAKA_FN_ACC auto operator()(
      TAcc const &acc, TDetectorData const *const detectorData,
      TGainMap const *const gainMaps, TInitPedestalMap *const initPedestalMaps,
      TPedestalMap *const pedestalMaps, TGainStageMap *const gainStageMaps,
      TEnergyMap *const energyMaps, TPhotonMap *const photonMaps,
      TNumFrames const numFrames, TBeamConst beamConst, TMask *const mask,
      bool pedestalFallback, TNumStdDevs const c = Config::C) const -> void {
    auto globalId = getLinearIdx(acc);
    auto elementsPerThread = getLinearElementExtent(acc);

    // iterate over all elements in the thread
    for (auto id = globalId * elementsPerThread;
         id < (globalId + 1) * elementsPerThread; ++id) {

      // check range
      if (id >= Config::MAPSIZE)
        break;

      for (TNumFrames i = 0; i < numFrames; ++i) {
        // generate energy maps and gain stage maps
        processInput(acc, detectorData[i], gainMaps, pedestalMaps,
                     initPedestalMaps, gainStageMaps[i], energyMaps[i], mask,
                     id, pedestalFallback);

        // read data from generated maps
        auto dataword = detectorData[i].data[id];
        auto adc = getAdc(dataword);
        const auto &gainStage = gainStageMaps[i].data[id];

        // first thread copies frame header to output
        if (id == 0) {
          copyFrameHeader(detectorData[i], photonMaps[i]);
        }

        const auto &energy = energyMaps[i].data[id];
        auto &photonCount = photonMaps[i].data[id];

        // calculate photon count from calibrated energy
        double photonCountFloat = (static_cast<double>(energy) +
                                   static_cast<double>(beamConst) / 2.0) /
                                  static_cast<double>(beamConst);
        photonCount = (photonCountFloat < 0.0f) ? 0 : photonCountFloat;

        const auto &pedestal = pedestalMaps[gainStage][id];
        const auto &stddev = initPedestalMaps[gainStage][id].stddev;

	//! @todo: dark condition should only occur in gain stages > 0

        // check "dark pixel" condition
        if (pedestal - c * stddev <= adc && pedestal + c * stddev >= adc &&
            !pedestalFallback) {
          updatePedestal(adc, Config::MOVING_STAT_WINDOW_SIZE,
                         pedestalMaps[gainStage][id]);
        }
      }
    }
  }
};