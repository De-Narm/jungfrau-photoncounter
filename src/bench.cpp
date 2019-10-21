#include "bench.hpp"
#include "confgen.hpp"

#include "jungfrau-photoncounter/Alpakaconfig.hpp"
#include "jungfrau-photoncounter/Config.hpp"
#include "jungfrau-photoncounter/Dispenser.hpp"

#include "jungfrau-photoncounter/Debug.hpp"
#include <chrono>
#include <unordered_map>
#include <vector>

/**
 * only change this line to change the backend
 * see Alpakaconfig.hpp for all available
 */

template <std::size_t MAPSIZE>
using Accelerator = CpuOmp2Blocks<MAPSIZE>; // CpuOmp2Blocks< // GpuCudaRt<
// MAPSIZE>; // CpuSerial<MAPSIZE>;//GpuCudaRt<MAPSIZE>;//GpuCudaRt<MAPSIZE>;
// // CpuSerial;

/*
//#define MOENCH

#ifdef MOENCH
// using Config = MoenchConfig;
const std::string pedestalPath =
    "../../../../moench_data/1000_frames_pede_e17050_1_00018_00000.dat";
const std::string gainPath = "../../../../moench_data/moench_gain.bin";
const std::string dataPath =
    "../../../../moench_data/e17050_1_00018_00000_image.dat";
#else
// using Config = JungfrauConfig;
const std::string pedestalPath =
    "../../../data_pool/px_101016/allpede_250us_1243__B_000000.dat";
const std::string gainPath = "../../../data_pool/px_101016/gainMaps_M022.bin";
const std::string dataPath =
    "../../../data_pool/px_101016/Insu_6_tr_1_45d_250us__B_000000.dat";
#endif
*/

constexpr auto framesPerStageG0 = Values<std::size_t, 1000>();
constexpr auto framesPerStageG1 = Values<std::size_t, 1000>();
constexpr auto framesPerStageG2 = Values<std::size_t, 999>();
constexpr auto dimX = Values<std::size_t, 1024>();
constexpr auto dimY = Values<std::size_t, 512>();
constexpr auto sumFrames =
    Values<std::size_t, 2>(); // Values<std::size_t, 2, 10, 20, 100>();
constexpr auto devFrames =
    Values<std::size_t, 10>(); // Values<std::size_t, 10, 100, 1000>();
constexpr auto movingStatWindowSize = Values<std::size_t, 100>();
constexpr auto clusterSize =
    Values<std::size_t, 2>(); // Values<std::size_t, 2, 3, 7, 11>();
constexpr auto cs = Values<std::size_t, 5>();

constexpr auto parameterSpace =
    framesPerStageG0 * framesPerStageG1 * framesPerStageG2 * dimX * dimY *
    sumFrames * devFrames * movingStatWindowSize * clusterSize * cs;

template <class Tuple> struct ConfigFrom {
  using G0 = Get_t<0, Tuple>;
  using G1 = Get_t<1, Tuple>;
  using G2 = Get_t<2, Tuple>;
  using DimX = Get_t<3, Tuple>;
  using DimY = Get_t<4, Tuple>;
  using SumFrames = Get_t<5, Tuple>;
  using DevFrames = Get_t<6, Tuple>;
  using MovingStatWinSize = Get_t<7, Tuple>;
  using ClusterSize = Get_t<8, Tuple>;
  using C = Get_t<9, Tuple>;
  using Result =
      DetectorConfig<G0::value, G1::value, G2::value, DimX::value, DimY::value,
                     SumFrames::value, DevFrames::value,
                     MovingStatWinSize::value, ClusterSize::value, C::value>;
};

using Duration = std::chrono::milliseconds;
using Timer = std::chrono::high_resolution_clock;

template <class Tuple>
std::vector<Duration> benchmark(unsigned int iterations, ExecutionFlags flags,
                                const std::string &pedestalPath,
                                const std::string &gainPath,
                                const std::string &dataPath, float beamConst) {
  using Config = typename ConfigFrom<Tuple>::Result;
  using ConcreteAcc = Accelerator<Config::MAPSIZE>;
  auto benchmarkingInput = setUp<Config, ConcreteAcc>(
      flags, pedestalPath, gainPath, dataPath, beamConst);
  std::vector<Duration> results;
  results.reserve(iterations);
  for (unsigned int i = 0; i < iterations; ++i) {
    auto dispenser = calibrate(benchmarkingInput);
    auto t0 = Timer::now();
    bench(dispenser, benchmarkingInput);
    auto t1 = Timer::now();
    results.push_back(std::chrono::duration_cast<Duration>(t1 - t0));
  }
  return results;
}

using BenchmarkFunction = std::vector<Duration> (*)(unsigned int,
                                                    ExecutionFlags,
                                                    const std::string &,
                                                    const std::string &,
                                                    const std::string &, float);

static std::unordered_map<int, BenchmarkFunction> benchmarks;

void registerBenchmarks(int, Empty) {}

template <class List> void registerBenchmarks(int x, const List &) {
  using H = typename List::Head;
  using T = typename List::Tail;
  using F = typename Flatten<Tuple<>, H>::Result;
  benchmarks[x] = benchmark<F>;
  registerBenchmarks(x + 1, T{});
}

int main(int argc, char *argv[]) {
  // check command line parameters
  if (argc != 11 && argc != 12) {
    std::cerr
        << "Usage: bench <benchmark id> <iteration count> <beamConst> <mode> "
           "<masking> <max values> <summation> <pedestal path> <gain "
           "path> <data path> [output prefix]\n";
    abort();
  }

  // initialize parameters
  int benchmarkID = std::atoi(argv[1]);
  unsigned int iterationCount = static_cast<unsigned int>(std::atoi(argv[2]));
  float beamConst = std::atoi(argv[3]);
  ExecutionFlags ef;
  ef.mode = static_cast<std::uint8_t>(std::atoi(argv[4]));
  ef.masking = static_cast<std::uint8_t>(std::atoi(argv[5]));
  ef.maxValue = static_cast<std::uint8_t>(std::atoi(argv[6]));
  ef.summation = static_cast<std::uint8_t>(std::atoi(argv[7]));

  std::string pedestalPath(argv[8]);
  std::string gainPath(argv[9]);
  std::string dataPath(argv[10]);
  std::string outputPath((argc == 12) ? argv[11] : "");

  // create output path suffix
  outputPath += ((argc == 12) ? "_" : "") + std::to_string(benchmarkID) + "_" +
                std::to_string(iterationCount) + "_" + std::to_string(ef.mode) +
                "_" + std::to_string(ef.masking) + "_" +
                std::to_string(ef.maxValue) + "_" +
                std::to_string(ef.summation) + "_" + pedestalPath + "_" +
                gainPath + "_" + dataPath + ".txt";

  // escape suffx
  std::transform(outputPath.begin(), outputPath.end(), outputPath.begin(),
                 [](char c) -> char { return (c == '/') ? ' ' : c; });

  // register benchmarks
  registerBenchmarks(0, parameterSpace);
  std::cout << "Registered " << benchmarks.size() << " benchmarks. \n";

  // check benchmark ID
  if (benchmarkID < 0 ||
      static_cast<unsigned int>(benchmarkID) >= benchmarks.size()) {
    std::cerr << "Benchmark ID out of range. \n";
    abort();
  }

  // open output file
  std::ofstream outputFile(outputPath);
  if (!outputFile.is_open()) {
    std::cerr << "Couldn't open output file " << outputPath << "\n";
    abort();
  }

  // run benchmark
  auto results = benchmarks[benchmarkID](iterationCount, ef, pedestalPath,
                                         gainPath, dataPath, beamConst);

  // store results
  for (const auto &r : results)
    outputFile << r.count() << " ";

  outputFile.flush();
  outputFile.close();

  return 0;
}
