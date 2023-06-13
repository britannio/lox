# Development Notes

## C notes

`void functionName(const char *name)`
Here, the `char` array pointer cannot be de-referenced to change its value as it is const.


## Clox pieces

### Chunk
A sequence of bytecode

### Instructions
A one-byte operand + an optional opcode.

In the case of constants, the opcode is a single byte containing an index into the constants pool (a compile-time array of constants).