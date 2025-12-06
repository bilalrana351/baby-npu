#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "../common.hpp"
#include "../registers.hpp"
#include <fstream>
#include <chrono>
#include <ctime>

namespace npu {

// =============================================================================
// Cycle Log Entry
// =============================================================================

struct CycleLogEntry {
    uint64_t cycle;
    Address pc;
    std::string instruction;
    std::string rd;
    std::string rs1;
    std::string rs2;
    std::string result;
};

// =============================================================================
// Logger Class
// =============================================================================

class Logger {
private:
    std::vector<CycleLogEntry> cycleLog_;
    std::string programName_;
    bool csvEnabled_;
    bool jsonEnabled_;
    std::string outputDir_;
    
public:
    Logger(const std::string& programName = "", 
           const std::string& outputDir = "output",
           bool csvEnabled = true, 
           bool jsonEnabled = true)
        : programName_(programName), csvEnabled_(csvEnabled), 
          jsonEnabled_(jsonEnabled), outputDir_(outputDir) {}
    
    // Log a cycle
    void logCycle(uint64_t cycle, Address pc, const DecodedInstr& instr, 
                  const RegisterFile& regs);
    
    // Write CSV trace file
    void writeCSV(const std::string& filename = "");
    
    // Write JSON summary
    void writeJSON(const std::string& filename, const Stats& stats, 
                   const RegisterFile& regs);
    
    // Print final statistics
    static void printStats(const Stats& stats, std::ostream& os = std::cout);
    
    // Print register state
    static void printRegisters(const RegisterFile& regs, std::ostream& os = std::cout);
    
    // Get timestamp string
    static std::string getTimestamp();
    
    // Clear log
    void clear() { cycleLog_.clear(); }
    
    // Get log entries
    const std::vector<CycleLogEntry>& getLog() const { return cycleLog_; }
};

} // namespace npu

#endif // LOGGER_HPP


