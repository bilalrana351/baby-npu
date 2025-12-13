#include "../include/assembler/assembler.hpp"
#include "../include/common.hpp"
#include "../include/cpu/single_cycle.hpp"
#include "../include/cpu/pipelined.hpp"
#include "../include/memory.hpp"
#include "../include/utils/config.hpp"
#include "../include/utils/logger.hpp"

#include <filesystem>
#include <iostream>

using namespace npu;

// =============================================================================
// Data Initialization Helper
// =============================================================================

void initializeTestData(Memory &mem) {
  // Initialize test vectors at common addresses used by sample programs

  // Address 0x100 (256): Vector A = [10, 20, 30, 40]
  std::vector<int32_t> vectorA = {10, 20, 30, 40};
  mem.loadData(vectorA, 256);

  // Address 0x200 (512): Vector B = [1, 2, 3, 4]
  std::vector<int32_t> vectorB = {1, 2, 3, 4};
  mem.loadData(vectorB, 512);

  // Address 0x400 (1024): Input vectors for loop program (3 vectors)
  std::vector<int32_t> inputs = {
      1, 2,  3,  4, // Input 1
      5, 6,  7,  8, // Input 2
      9, 10, 11, 12 // Input 3
  };
  mem.loadData(inputs, 1024);

  // Address 0x500 (1280): Weight vectors
  std::vector<int32_t> weights = {
      1, 1, 1, 1, // Weight 1
      2, 2, 2, 2, // Weight 2
      3, 3, 3, 3  // Weight 3
  };
  mem.loadData(weights, 1280);

  // Address 0x1000 (4096): Neural network weights (4x4 matrix row-major)
  std::vector<int32_t> nnWeights = {
      1, 0, 0, 0, // Row 0
      0, 2, 0, 0, // Row 1
      0, 0, 3, 0, // Row 2
      0, 0, 0, 4  // Row 3 (diagonal matrix for testing)
  };
  mem.loadData(nnWeights, 4096);

  // Address 0x1100 (4352): Input vector for NN
  std::vector<int32_t> nnInput = {10, 20, 30, 40};
  mem.loadData(nnInput, 4352);

  // Address 0x1200 (4608): Bias vector
  std::vector<int32_t> nnBias = {1, 2, 3, 4};
  mem.loadData(nnBias, 4608);

  // Address 0x2000 (8192): Second layer weights (for MLP)
  std::vector<int32_t> layer2Weights = {1, 1,  1, 1,  -1, 1,  -1, 1,
                                        1, -1, 1, -1, -1, -1, 1,  1};
  mem.loadData(layer2Weights, 8192);

  // Address 0x2100 (8448): RNN hidden state weights
  std::vector<int32_t> rnnWhWeights = {1, 0, 0, 0, 0, 1, 0, 0,
                                       0, 0, 1, 0, 0, 0, 0, 1};
  mem.loadData(rnnWhWeights, 8448);

  // Address 0x2200 (8704): RNN input weights
  std::vector<int32_t> rnnWxWeights = {1, 1, 0, 0, 0, 1, 1, 0,
                                       0, 0, 1, 1, 1, 0, 0, 1};
  mem.loadData(rnnWxWeights, 8704);

  // Address 0x2300 (8960): RNN input sequence
  std::vector<int32_t> rnnInputSeq = {
      1, 2, 3, 4, // Timestep 1
      2, 3, 4, 5, // Timestep 2
      3, 4, 5, 6  // Timestep 3
  };
  mem.loadData(rnnInputSeq, 8960);
}

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char *argv[]) {
  std::cout << "================================================\n";
  std::cout << "         NPU Simulator - RISC-V + NPU          \n";
  std::cout << "        Neural Processing Unit Extension        \n";
  std::cout << "================================================\n\n";

  // Parse command line arguments
  Config config = ConfigParser::parse(argc, argv);
  std::string inputFile = ConfigParser::getInputFile(argc, argv);

  if (inputFile.empty()) {
    ConfigParser::printHelp(argv[0]);
    return 1;
  }

  // Extract program name for logging
  std::filesystem::path filePath(inputFile);
  std::string programName = filePath.filename().string();

  std::cout << "Input file: " << inputFile << "\n";
  std::cout << "CPU Mode:   " << (config.pipelined ? "Pipelined (5-stage)" : "Single-Cycle")
            << "\n";
  std::cout << "Verbose:    " << (config.verbose ? "enabled" : "disabled")
            << "\n";
  std::cout << "Output dir: " << config.outputDir << "\n\n";

  try {
    // Phase 1: Assembly
    std::cout << "=== ASSEMBLY PHASE ===\n";
    Assembler assembler(config.verbose);
    AssemblyResult asmResult = assembler.assembleFile(inputFile);

    if (!asmResult.success) {
      std::cerr << "Assembly failed:\n";
      for (const auto &err : asmResult.errors) {
        std::cerr << "  Error: " << err << "\n";
      }
      return 1;
    }

    std::cout << "Assembly successful: " << asmResult.machineCode.size()
              << " instructions\n\n";

    if (config.verbose) {
      Assembler::printProgram(asmResult);
      std::cout << "\n";
    }

    // Phase 2: Initialize Memory
    std::cout << "=== MEMORY INITIALIZATION ===\n";
    Memory memory(config.memorySize);
    memory.loadProgram(asmResult.machineCode);
    initializeTestData(memory);
    std::cout << "Memory initialized with test data\n\n";

    // Phase 3: Execution
    std::cout << "=== EXECUTION PHASE ===\n";
    Logger logger(programName, config.outputDir, config.csvOutput,
                  config.jsonOutput);
    
    if (config.pipelined) {
      // Use pipelined CPU
      PipelinedCPU cpu(memory, config.traceEnabled ? &logger : nullptr,
                       config.verbose);
      cpu.run();
      
      // Phase 4: Results
      std::cout << "\n=== EXECUTION COMPLETE ===\n";
      
      const Stats &stats = cpu.getStats();
      const PipelineStats &pipeStats = cpu.getPipelineStats();
      Logger::printPipelineStats(stats, pipeStats);
      Logger::printRegisters(cpu.getRegisters());
      
      // Write output files
      if (config.traceEnabled) {
        std::filesystem::create_directories(config.outputDir + "/traces");
        std::filesystem::create_directories(config.outputDir + "/logs");
        
        if (config.csvOutput) {
          logger.writeCSV();
        }
        if (config.jsonOutput) {
          logger.writeJSON("", stats, cpu.getRegisters());
        }
      }
    } else {
      // Use single-cycle CPU
      SingleCycleCPU cpu(memory, config.traceEnabled ? &logger : nullptr,
                         config.verbose);
      cpu.run();
      
      // Phase 4: Results
      std::cout << "\n=== EXECUTION COMPLETE ===\n";
      
      const Stats &stats = cpu.getStats();
      Logger::printStats(stats);
      Logger::printRegisters(cpu.getRegisters());
      
      // Write output files
      if (config.traceEnabled) {
        std::filesystem::create_directories(config.outputDir + "/traces");
        std::filesystem::create_directories(config.outputDir + "/logs");
        
        if (config.csvOutput) {
          logger.writeCSV();
        }
        if (config.jsonOutput) {
          logger.writeJSON("", stats, cpu.getRegisters());
        }
      }
    }

    std::cout << "\nSimulation completed successfully!\n";

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
