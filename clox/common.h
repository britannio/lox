#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DEBUG_PRINT_CODE
// Disassemble and print each instruction before execution
#define DEBUG_TRACE_EXECUTION

#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT16_COUNT (UINT16_MAX + 1)

#endif