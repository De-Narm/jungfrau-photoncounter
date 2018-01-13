#include "Alpakaconfig.hpp"
#include "Config.hpp"
#include "Dispenser.hpp"
#include "Filecache.hpp"

using Accelerator = GpuCudaRt;

auto main() -> int
{
    Filecache fc(1024UL * 1024 * 1024 * 16);
    DEBUG("filecache created");

    Maps<Data, Accelerator> pedestaldata(
        fc.loadMaps<Data, Accelerator>("../../jungfrau-photoncounter/data_pool/px_101016/"
                          "allpede_250us_1243__B_000000.dat",
                          true));
    DEBUG(pedestaldata.numMaps << " pedestaldata maps loaded");

    Maps<Data, Accelerator> data(
        fc.loadMaps<Data, Accelerator>("../../jungfrau-photoncounter/data_pool/px_101016/"
                          "Insu_6_tr_1_45d_250us__B_000000.dat",
                          true));
    DEBUG(data.numMaps << " data maps loaded");

    Maps<Gain, Accelerator> gain(fc.loadMaps<Gain, Accelerator>(
        "../../jungfrau-photoncounter/data_pool/px_101016/gainMaps_M022.bin"));
    DEBUG(gain.numMaps << " gain maps loaded");

    DEBUG("gpu count: "
          << (alpaka::pltf::getDevCount<alpaka::pltf::Pltf<alpaka::dev::Dev<
                  alpaka::acc::AccGpuCudaRt<alpaka::dim::DimInt<1u>,
                                            std::size_t>>>>()));

    Dispenser<Accelerator>* dispenser = new Dispenser<Accelerator>(&gain);

    dispenser->uploadPedestaldata(pedestaldata);

    Maps<Photon, Accelerator> photon{};
    Maps<PhotonSum, Accelerator> sum{};
    std::size_t offset = 0;
    std::size_t downloaded = 0;
    while (downloaded < data.numMaps) {
        offset = dispenser->uploadData(data, offset);
        if (dispenser->downloadData(&photon, &sum)) {
            downloaded += DEV_FRAMES;
            DEBUG(downloaded << "/" << data.numMaps << " downloaded");
        }
}

    return 0;
}
