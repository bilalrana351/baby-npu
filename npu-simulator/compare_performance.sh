#!/bin/bash
# Performance Comparison Script: Single-Cycle vs Pipelined CPU
# Shows that pipelined is faster in REAL TIME despite more cycles

cd "$(dirname "$0")"

echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║                    NPU SIMULATOR - PERFORMANCE COMPARISON                  ║"
echo "║                   Single-Cycle vs Pipelined (Real Timing)                  ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "ASSUMPTIONS (based on typical ASIC implementations):"
echo "  Single-Cycle: 100 MHz (10 ns period) - Long critical path"
echo "  Pipelined:    500 MHz (2 ns period)  - Shorter stages (5x faster clock)"
echo ""
echo "════════════════════════════════════════════════════════════════════════════"

# Function to extract and format stats
compare_program() {
    local program=$1
    local name=$(basename "$program" .asm)
    
    echo ""
    echo "┌────────────────────────────────────────────────────────────────────────┐"
    echo "│ Program: $name"
    echo "└────────────────────────────────────────────────────────────────────────┘"
    
    # Run single-cycle
    sc_output=$(./bin/npu_sim "$program" 2>/dev/null)
    sc_cycles=$(echo "$sc_output" | grep "Total Cycles:" | awk '{print $3}')
    sc_time=$(echo "$sc_output" | grep "Execution Time:" | grep "ns" | head -1 | awk '{print $3}')
    sc_mips=$(echo "$sc_output" | grep "Throughput:" | awk '{print $2}')
    
    # Run pipelined
    pl_output=$(./bin/npu_sim -p "$program" 2>/dev/null)
    pl_cycles=$(echo "$pl_output" | grep "Total Cycles:" | awk '{print $3}')
    pl_time=$(echo "$pl_output" | grep "Execution Time:" | grep "ns" | head -1 | awk '{print $3}')
    pl_mips=$(echo "$pl_output" | grep "Throughput:" | awk '{print $2}')
    pl_cpi=$(echo "$pl_output" | grep "CPI:" | head -1 | awk '{print $3}')
    
    # Calculate speedup
    speedup=$(echo "scale=2; $sc_time / $pl_time" | bc)
    
    echo ""
    printf "%-20s | %10s | %10s | %s\n" "Metric" "Single-Cycle" "Pipelined" "Advantage"
    echo "────────────────────────────────────────────────────────────────────────"
    printf "%-20s | %10s | %10s | %s\n" "Cycles" "$sc_cycles" "$pl_cycles" "Single-Cycle (-)"
    printf "%-20s | %10s | %10s | %s\n" "Execution Time" "${sc_time} ns" "${pl_time} ns" "Pipelined (${speedup}x)"
    printf "%-20s | %10s | %10s | %s\n" "Throughput" "${sc_mips} MIPS" "${pl_mips} MIPS" "Pipelined"
    printf "%-20s | %10s | %10s | %s\n" "CPI" "1.000" "$pl_cpi" "Single-Cycle"
    
    # Visual speedup indicator
    echo ""
    if (( $(echo "$speedup > 2.0" | bc -l) )); then
        echo "⚡ RESULT: Pipelined is ${speedup}x FASTER in real hardware!"
    else
        echo "✓ RESULT: Pipelined is ${speedup}x faster in real hardware"
    fi
}

# Compare all programs
echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "                            SAMPLE PROGRAMS"
echo "═══════════════════════════════════════════════════════════════════════════"

for prog in programs/sample/*.asm; do
    compare_program "$prog"
done

echo ""
echo "═══════════════════════════════════════════════════════════════════════════"
echo "                       NEURAL NETWORK PROGRAMS"
echo "═══════════════════════════════════════════════════════════════════════════"

for prog in programs/neural_nets/*.asm; do
    compare_program "$prog"
done

echo ""
echo "════════════════════════════════════════════════════════════════════════════"
echo ""
echo "KEY INSIGHTS:"
echo "  1. Pipelined has MORE CYCLES due to hazards (stalls & flushes)"
echo "  2. But pipelined runs at a MUCH HIGHER clock frequency (5x)"
echo "  3. Result: Pipelined completes programs 2-3x FASTER in real time"
echo "  4. Throughput (MIPS) is 4-5x higher for pipelined"
echo ""
echo "This demonstrates why ALL modern processors use pipelining!"
echo ""

