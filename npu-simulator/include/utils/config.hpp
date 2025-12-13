#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "../common.hpp"
#include <getopt.h>

namespace npu {

// =============================================================================
// Command Line Configuration
// =============================================================================

class ConfigParser {
public:
    static Config parse(int argc, char* argv[]) {
        Config config;
        
        static struct option long_options[] = {
            {"verbose", no_argument, 0, 'v'},
            {"output", required_argument, 0, 'o'},
            {"no-trace", no_argument, 0, 't'},
            {"no-json", no_argument, 0, 'j'},
            {"no-csv", no_argument, 0, 'c'},
            {"memory", required_argument, 0, 'm'},
            {"pipelined", no_argument, 0, 'p'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        
        int opt;
        int option_index = 0;
        
        while ((opt = getopt_long(argc, argv, "vo:tjcm:ph", long_options, &option_index)) != -1) {
            switch (opt) {
                case 'v':
                    config.verbose = true;
                    break;
                case 'o':
                    config.outputDir = optarg;
                    break;
                case 't':
                    config.traceEnabled = false;
                    break;
                case 'j':
                    config.jsonOutput = false;
                    break;
                case 'c':
                    config.csvOutput = false;
                    break;
                case 'm':
                    config.memorySize = std::stoul(optarg) * 1024;  // KB
                    break;
                case 'p':
                    config.pipelined = true;
                    break;
                case 'h':
                default:
                    printHelp(argv[0]);
                    exit(0);
            }
        }
        
        return config;
    }
    
    static void printHelp(const char* programName) {
        std::cout << "NPU Simulator - RISC-V + NPU Extension\n";
        std::cout << "\n";
        std::cout << "Usage: " << programName << " [options] <program.asm>\n";
        std::cout << "\n";
        std::cout << "Options:\n";
        std::cout << "  -v, --verbose     Enable verbose output (cycle-by-cycle trace)\n";
        std::cout << "  -o, --output DIR  Output directory for logs (default: output)\n";
        std::cout << "  -t, --no-trace    Disable trace logging\n";
        std::cout << "  -j, --no-json     Disable JSON output\n";
        std::cout << "  -c, --no-csv      Disable CSV trace output\n";
        std::cout << "  -m, --memory KB   Memory size in KB (default: 64)\n";
        std::cout << "  -p, --pipelined   Use 5-stage pipelined CPU (default: single-cycle)\n";
        std::cout << "  -h, --help        Show this help message\n";
        std::cout << "\n";
        std::cout << "Examples:\n";
        std::cout << "  " << programName << " programs/sample/program1_arithmetic.asm\n";
        std::cout << "  " << programName << " -v programs/neural_nets/mlp_2layer.asm\n";
        std::cout << "  " << programName << " -p programs/hazards/load_use.asm\n";
        std::cout << "  " << programName << " -p -v programs/sample/program2_loop.asm\n";
    }
    
    static std::string getInputFile(int argc, char* argv[]) {
        // Find the first non-option argument
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] != '-') {
                return argv[i];
            }
            // Skip option arguments
            if ((strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0 ||
                 strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) &&
                i + 1 < argc) {
                ++i;
            }
        }
        return "";
    }
};

} // namespace npu

#endif // CONFIG_HPP


