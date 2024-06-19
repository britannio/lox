#include "compiler.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

#endif

#include "vm.h"
#include "memory.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode;
} Parser;

// Relies on C assigning larger numbers to latter enums.
typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

// A typedef for a function with no arguments that returns nothing
typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool mutable;
  bool isCaptured;
} Local;

typedef struct {
  // The local slot that the upvalue is capturing
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
  struct Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;

  // Local locals[UINT8_COUNT];
  Local locals[STACK_MAX];
  int localCount;
  // A function can reference 256 variables in an enclosing scope.
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;
  int unpatchedBreaks;
  Table globalMutability;
} Compiler;

typedef struct CurrentLoop {
  struct CurrentLoop *enclosing;
  int continueOffset;
  int unpatchedBreakJumps;
} CurrentLoop;


Parser parser;
// Global variables aren't the best practice but the book author made the
// decision to save time. This is particularly a problem for multithreaded
// scenarios.
Compiler *current = NULL;
CurrentLoop *currentLoop = NULL;
Array unpatchedBreaks;

static int resolveLocal(Compiler *compiler, Token *name);

static int resolveUpvalue(Compiler *compiler, Token *name);

static uint8_t argumentList();

static Chunk *currentChunk() {
  return &current->function->chunk;
}

// Prints the token error message
static void errorAt(Token *token, const char *message) {
  // If we have already reported an error, ignore subsequent errors to avoid
  // errors cascading
  if (parser.panicMode)
    return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}

static void error(const char *message) { errorAt(&parser.previous, message); }

static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

// Get the next token that is not an error token.
static void advance() {
  // Shift to the next token
  parser.previous = parser.current;

  for (;;) {
    // Set the next token
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    // Recall that during scanning, if we encounter an error, a standard
    // Token is produced. The type of this token is TOKEN_ERROR and instead
    // of the token pointing to a string range of source code, It points to
    // an error message literal string!
    errorAtCurrent(parser.current.start);
  }
}

// Move to the next token if the current one has the expected type
static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}


/**
 * @return true if the current token matches the provided one. No side effects.
 */
static bool check(TokenType type) { return parser.current.type == type; }

/**
 * @return true if the current token matches the provided one. Moves to the next token.
 */
static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  // We write a temporary chunk pointer here until we have compiled the body
  // of the if statement and we know where the false branch starts.
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  // +2 accounts for the two bytes we emit below;
  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX)
    error("Loop body too large.");

  emitByte((offset >> 8) & 0xFF);
  emitByte(offset & 0xFF);
}

static void emitReturn() {
  // The implicit return will return a nil value
  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

static int makeConstant(Value value) {
  int constant = -1;
  Chunk *chunk = currentChunk();
  // Risky optimisation to reuse values in the constant pool to avoid filling it
  // up This assumes that we never remove values from the pool?
  for (int i = chunk->count - 1; i >= 0; i--) {
    if (valuesEqual(value, chunk->constants.values[i])) {
      constant = i;
      break;
    }
  }
  if (constant < 0) {
    constant = addConstant(currentChunk(), value);
  }

  return constant;
}

static void emitConstant(Value value) {
  writeConstant(currentChunk(), value, parser.previous.line);
}

// Write the JUMP pointer that follows a OP_JUMP_IF_* instruction.
static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xFF;
  currentChunk()->code[offset + 1] = jump & 0xFF;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->function = newFunction();
  compiler->unpatchedBreaks = 0;
  initTable(&compiler->globalMutability);
  current = compiler;
  if (type != TYPE_SCRIPT) {
    // This function is invoked straight after parsing the function name so
    // we can access it like this.
    current->function->name = copyString(parser.previous.start, parser.previous.length);
  }

  if (type == TYPE_SCRIPT) {
    initArray(&unpatchedBreaks, sizeof(int));
  }

  // Reserve stack slot 0 as a local variable
  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  // Empty string so that it cannot clash with user defined locals
  local->isCaptured = false;
  local->name.start = "";
  local->name.length = 0;
}

static ObjFunction *endCompiler() {
  emitReturn();
  ObjFunction *function = current->function;
  freeTable(&current->globalMutability);
  if (current->type == TYPE_SCRIPT) {
    freeArray(&unpatchedBreaks);
  }
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), function->name != NULL
                                     ? function->name->chars
                                     : "<script>");
  }
#endif

  current = current->enclosing;
  return function;
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
  current->scopeDepth--;

  while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      // The local variable is captured, instead of removing it from the stack,
      // move it to the heap.
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }

    current->localCount--;
  }
}

static void expression();

static void statement();

static void declaration();

static uint8_t identifierConstant(Token *name);

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static void binary(bool canAssign) {
  // The left operand has been consumed
  // The infix operator has also been consumed (held in parser.previous)

  TokenType operatorType = parser.previous.type;
  // Each binary operator's right-hand operand precedence is one level higher
  // than its own (left associativity)
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence) (rule->precedence + 1));

  switch (operatorType) {
    case TOKEN_BANG_EQUAL:
      emitBytes(OP_EQUAL, OP_NOT);
      break;
    case TOKEN_EQUAL_EQUAL:
      emitByte(OP_EQUAL);
      break;
    case TOKEN_GREATER:
      emitByte(OP_GREATER);
      break;
    case TOKEN_GREATER_EQUAL:
      emitBytes(OP_LESS, OP_NOT);
      break;
    case TOKEN_LESS:
      emitByte(OP_LESS);
      break;
    case TOKEN_LESS_EQUAL:
      emitBytes(OP_GREATER, OP_NOT);
      break;
    case TOKEN_PLUS:
      emitByte(OP_ADD);
      break;
    case TOKEN_MINUS:
      emitByte(OP_SUBTRACT);
      break;
    case TOKEN_STAR:
      emitByte(OP_MULTIPLY);
      break;
    case TOKEN_SLASH:
      emitByte(OP_DIVIDE);
      break;
    default:
      return; // Unreachable.
  }
}

static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE:
      emitByte(OP_FALSE);
      break;
    case TOKEN_NIL:
      emitByte(OP_NIL);
      break;
    case TOKEN_TRUE:
      emitByte(OP_TRUE);
      break;
    default:
      return; // Unreachable
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
  // TODO how does it know how many characters to parse?
  // Maybe because a number can only be digits 0-9 or a dot so anything after
  // that can be ignored. Our program source is also null character terminated
  // so we won't stray into memory we do not own.

  double value = strtod(parser.previous.start, NULL);
  emitConstant(NUMBER_VAL(value));
}

static void and_(bool canAssign) {
  // The left operand expression is already on the stack.
  // If it is false then we jump outside of the expression.
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  // Otherwise, we no longer need the 'true' expression so we pop it
  // and evaluate the right hand side.
  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}

static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}


static void string(bool canAssign) {
  // Copy the string from the source program.
  // We create a copy because this simplifies memory management when we need
  // to free the string.
  emitConstant(OBJ_VAL(
                       copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  bool mutable;
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
    Local local = current->locals[arg];
    mutable = local.mutable;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    // This branch is hit if we fail to resolve the variable as one within the
    // scope of the function being compiled.
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
    Value mutableVal;
    Value *key = &currentChunk()->constants.values[arg];
    tableGet(&current->globalMutability, key, &mutableVal);
    mutable = AS_BOOL(mutableVal);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    if (!mutable) {
      error("Attempted to mutate a final variable.");
      return;
    }
    expression();
    emitBytes(setOp, (uint8_t) arg);
  } else {
    emitBytes(getOp, (uint8_t) arg);
  }
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
  TokenType operatorType = parser.previous.type;

  // Compile the operand
  // When parsing an expression such as -a.b + c;, it will stop after -a.b
  parsePrecedence(PREC_UNARY);

  // emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG:
      emitByte(OP_NOT);
      break;
    case TOKEN_MINUS:
      emitByte(OP_NEGATE);
      break;
    default:
      return; // Unreachable
  }
}

// Recall that this is just an array and that enums have a corresponding number.
// This table makes it easy to see which tokens are available for use too!
ParseRule rules[] = {
        [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
        [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
        [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
        [TOKEN_MINUS] = {unary, binary, PREC_TERM},
        [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
        [TOKEN_COLON] = {NULL, NULL, PREC_NONE},
        [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
        [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
        [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
        [TOKEN_BANG] = {unary, NULL, PREC_EQUALITY},
        [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_NONE},
        [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
        [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
        [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
        [TOKEN_STRING] = {string, NULL, PREC_NONE},
        [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
        [TOKEN_AND] = {NULL, and_, PREC_AND},
        [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
        [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
        [TOKEN_IF] = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL] = {literal, NULL, PREC_NONE},
        [TOKEN_OR] = {NULL, or_, PREC_OR},
        [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
        [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
        [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
        [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
        [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
        [TOKEN_FINAL] = {NULL, NULL, PREC_NONE},
        [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
        [TOKEN_SWITCH] = {NULL, NULL, PREC_NONE},
        [TOKEN_CASE] = {NULL, NULL, PREC_NONE},
        [TOKEN_DEFAULT] = {NULL, NULL, PREC_NONE},
        [TOKEN_BREAK] = {NULL, NULL, PREC_NONE},
        [TOKEN_CONTINUE] = {NULL, NULL, PREC_NONE},
        [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
        [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
  advance();
  // The first token is always part of a prefix expression. We cannot see an
  // infix expression like + before its prefix.
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    // We hit a syntax error.
    error("Expect expression.");
    return;
  }

  // Run the prefix parsing function that matches the token we observed.
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static uint8_t identifierConstant(Token *name) {
  // Globals are referenced by name but the name string is too large to
  // fit into an instruction. Instead, the string is placed into a constants
  // table and the index in the table is returned.

  ObjString *copied = copyString(name->start, name->length);
  int constPoolIndex = makeConstant(OBJ_VAL(copied));
  if (constPoolIndex > UINT8_MAX) {
    // This can happen if we declare more than 256 variables in one chunk.
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t) constPoolIndex;
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  // Walk through the locals from top to bottom of 'stack'.
  // Iteration occurs in this order so that shadowing is possible.
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  // No local variable with such a name exists.
  return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  // If we already created an upvalue pointing to a closed over variable, reuse it.
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;

}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  // True if 'compiler' is the top level compiler paired with <script>
  // (the invisible function containing everything)
  if (compiler->enclosing == NULL) return -1;

  // We're here because we didn't resolve the variable in 'compiler'.
  // So next we try 'compiler.enclosing' i.e. the parent function/script of this
  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    // We just captured a local variable
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(compiler, (uint8_t) local, true);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t) upvalue, false);
  }

  return -1;
}

static void addLocal(Token name, bool mutable) {
  if (current->localCount == UINT16_COUNT) {
    error("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->localCount++];
  local->name = name;
  // Local's start in an uninitialised state
  local->depth = -1;
  local->mutable = mutable;
  local->isCaptured = false;
}

static void declareVariable(bool mutable) {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    // Work backwards looking for existing declarations with the same
    // name
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in scope");
    }
  }

  addLocal(*name, mutable);
}

static uint8_t parseVariable(const char *errorMessage, bool mutable) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable(mutable);
  // If scopeDepth > 0 then this isn't a global variable.
  if (current->scopeDepth > 0)
    return 0;

  uint8_t constPoolIndex = identifierConstant(&parser.previous);
  Value *varName = &currentChunk()->constants.values[constPoolIndex];
  tableSet(&current->globalMutability, *varName, BOOL_VAL(mutable));
  return constPoolIndex;
}

static void markInitialized() {
  if (current->scopeDepth == 0) return;
  // Previously it was set to -1.
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
  // Is the variable a local variable?
  if (current->scopeDepth > 0) {
    markInitialized();
    // Nothing to do for local variables.
    return;
  }

  // 'global' is the index of the name of the variable in the constants table.
  emitBytes(OP_DEFINE_GLOBAL, global);
}

// After '(' has been parsed, parse 0 or more arguments and return the number
// of arguments parsed.
static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}


// Looks up the token in [rules] to find the corresponding ParseRule
static ParseRule *getRule(TokenType type) {
  // This also serves as an indirection.
  // binary() uses getRule() so we could declare binary() after getRule()
  // binary() is declared before the rules array so that the array/table can
  // reference it. So binary() cannot also access the array.

  // declaring binary() after getRule() doesn't fix the problem as getRule()
  // would then have to be above the table... To break this cycle, we can use
  // forward declarations.
  return &rules[type];
}

static void expression() {
  // Parse at the lowest precedence level
  parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  // No need to pair with an endScope() as we endCompiler()
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(TOKEN_RIGHT_PAREN)) {
    // Parameters
    do {
      // The function has one more parameter
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      // Define a variable and get its index in the constant pool
      uint8_t constant = parseVariable("Expect parameter name.", true);
      defineVariable(constant);
    } while (match(TOKEN_COMMA));

  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction *function = endCompiler();

  // Note that the length of the operand varies
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
  for (int i = 0; i < function->upvalueCount; i++) {
    // 1 = local variable in enclosing function
    // 0 = upvalue of a function
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    // slot/upvalue index to capture
    emitByte(compiler.upvalues[i].index);
  }
}

static void funDeclaration() {
  uint8_t global = parseVariable("Expect function name.", false);
  // Variables suffered from an issue where referencing a variable while it is
  // being defined is invalid. This is fine for (recursive) functions.
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.", true);

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void finalVarDeclaration() {
  uint8_t global = parseVariable("Expect variable name.", false);

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    error("Expect assignment of final variable.");
    return;
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

static void expressionStatement() {
  // Evaluate the expression then discard the result.
  // Statements leave the stack the way they were found.
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}

static void forStatement() {
  // A variable defined in the initialiser of the for statement should have the same scope as the for statement itself.
  // This way, the variable will be out of scope after the for statement completes.
  beginScope();

  // be scoped to the loop body.
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
  if (match(TOKEN_SEMICOLON)) {
    // The initialiser was skipped
  } else if (match(TOKEN_VAR)) {
    varDeclaration(); // initialiser
  } else if (match(TOKEN_FINAL)) {
    finalVarDeclaration(); // initialiser
  } else {
    expressionStatement();
  }

  // Loop condition
  int loopStart = currentChunk()->count;
  // We can optionally skip the loop condition (infinite loop)
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Remove
  }

  // True if the increment clause is present.
  if (!match(TOKEN_RIGHT_PAREN)) {
    // Firstly, jump over the increment clause because we increment after
    // running the body.
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression(); // The increment clause itself
    // The increment clause is an expression that we don't need the value of
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }

  CurrentLoop loop = {
          .enclosing = currentLoop,
          .continueOffset = loopStart,
          .unpatchedBreakJumps = 0,
  };
  currentLoop = &loop;

  statement(); // Loop body

  emitLoop(loopStart);

  int breakStatementsToPatch = loop.unpatchedBreakJumps;
  current->unpatchedBreaks -= breakStatementsToPatch;
  currentLoop = loop.enclosing; // Pop

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP);
  }

  // Patch all the OP_JUMP instructions produced by break statements.
  for (int i = unpatchedBreaks.count - breakStatementsToPatch; i < unpatchedBreaks.count; i++) {
    patchJump(READ_AS(int, &unpatchedBreaks, i));
  }
  unpatchedBreaks.count -= breakStatementsToPatch;

  endScope();
}

static void ifStatement() {
  // Extract the bool expression from the if statement
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  // We don't know how many instructions to jump over until we have compiled
  // the body of the if statement so we use a temporary value.
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP); // Runs in the 'then' branch to remove the if condition.
  statement();

  // If we evaluate the 'then' branch (if condition is true)
  // we need to jump over the 'else' branch.
  int elseJump = emitJump(OP_JUMP);

  patchJump(thenJump);
  emitByte(OP_POP); // Runs in the 'else' branch to remove the if condition

  if (match(TOKEN_ELSE))
    statement();
  patchJump(elseJump);
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

  if (match(TOKEN_SEMICOLON)) {
    // Return nil
    emitReturn();
  } else {
    // Parse the return value
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}

static void whileStatement() {
  // Where to jump to for subsequent iterations of the while loop.
  int loopStart = currentChunk()->count;
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(); // Loop condition
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  // Skip over the while statement if the condition is false.
  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP); // Remove the loop condition

  CurrentLoop loop = {
          .enclosing = currentLoop,
          .continueOffset = loopStart,
          .unpatchedBreakJumps = 0,
  };
  currentLoop = &loop;

  statement();      // While statement body

  int breakStatementsToPatch = loop.unpatchedBreakJumps;
  current->unpatchedBreaks -= breakStatementsToPatch;
  currentLoop = loop.enclosing; // Pop CurrentLoop

  // Adds a OP_LOOP instruction at the end of the while statement, so it jumps
  // back to the loop condition for the next iteration.
  emitLoop(loopStart);

  patchJump(exitJump); // Where to skip to if the loop condition is false
  emitByte(OP_POP);    // Remove loop condition

  // Patch all the OP_JUMP instructions produced by break statements.
  // This goes after the OP_POP to remove the loop condition as breaks will only
  // be executed when the loop condition is true and in that case, the condition
  // has already been popped.
  for (int i = unpatchedBreaks.count - breakStatementsToPatch; i < unpatchedBreaks.count; i++) {
    patchJump(READ_AS(int, &unpatchedBreaks, i));
  }
  unpatchedBreaks.count -= breakStatementsToPatch;
}

static void switchStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
  expression(); // Expression to switch over
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch expression.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' after ')'.");

  Array caseExitJumps;
  initArray(&caseExitJumps, sizeof(int));

  while (match(TOKEN_CASE)) {
    expression(); // The case expression to compare to the main switch expression.
    emitByte(OP_EQUAL_PRESERVE);
    int nextCaseJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Pop the result of the equality check
    emitByte(OP_POP); // Pop the switch statement expression
    consume(TOKEN_COLON, "Expect ':' after case expression.");
    statement(); // The body of the case statement to run if true
    int exitJump = emitJump(OP_JUMP);
    writeArray(&caseExitJumps, &exitJump);
    // If the case does not match, jump over the case body.
    patchJump(nextCaseJump);
    emitByte(OP_POP); // Pop the result of the equality check
  }

  int defaultCaseExitJump = -1;
  if (match(TOKEN_DEFAULT)) {
    emitByte(OP_POP); // Pop the switch statement expression
    consume(TOKEN_COLON, "Expect ':' after 'default'.");
    statement(); // The body of the default case statement
    defaultCaseExitJump = emitJump(OP_JUMP); // Jump over the fallthrough OP_POP instruction
  }


  // For the switch statement expression
  emitByte(OP_POP);
  if (defaultCaseExitJump != -1) patchJump(defaultCaseExitJump);

  for (int i = 0; i < caseExitJumps.count; i++) {
    patchJump(READ_AS(int, &caseExitJumps, i));
  }

  freeArray(&caseExitJumps);

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch statement.");
}

static void continueStatement() {
  // Jump to the start of a while loop, before the condition.
  // Jump to the advancement expression of a for loop.

  // Variables inside the loop should only survive the duration of the loop right?

  // This statement is only valid within a while/for loop.
  // We need to keep track of the position we should jump to for the current while/for loop.
  // This should be done in a stack fashion

  consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

  if (currentLoop == NULL) {
    error("Can't use 'continue' outside of a loop.");
    return;
  }
  emitLoop(currentLoop->continueOffset);
}

static void breakStatement() {
  consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");

  if (currentLoop == NULL) {
    error("Can't use 'break' outside of a loop.");
    return;
  }
  int exitJump = emitJump(OP_JUMP);
  writeArray(&unpatchedBreaks, &exitJump);
  currentLoop->unpatchedBreakJumps += 1;
  current->unpatchedBreaks += 1;
}

static void synchronize() {
  parser.panicMode = false;

  // Skip tokens until we're at a statement boundary.
  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FINAL:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        // Do nothing
        ;
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_FUN)) {
    funDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else if (match(TOKEN_FINAL)) {
    finalVarDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode)
    synchronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_FOR)) {
    forStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
  } else if (match(TOKEN_CONTINUE)) {
    continueStatement();
  } else if (match(TOKEN_BREAK)) {
    breakStatement();
  } else {
    expressionStatement();
  }
}

ObjFunction *compile(const char *source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);

  parser.hadError = false;
  parser.panicMode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  ObjFunction *function = endCompiler();
  return parser.hadError ? NULL : function;
}
