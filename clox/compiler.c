#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

#endif

#include "vm.h"

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

// Relies on C assigning larger numbers to latter enums.
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
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
} Local;

typedef struct {
    // Local locals[UINT8_COUNT];
    Local locals[STACK_MAX];
    int localCount;
    int scopeDepth;
    Table globalMutability;
} Compiler;

Parser parser;
// Global variables aren't the best practice but the book author made the
// decision to save time. This is particularly a problem for multithreaded
// scenarios.
Compiler *current = NULL;
Chunk *compilingChunk;

static int resolveLocal(Compiler *compiler, Token *name);

static Chunk *currentChunk() { return compilingChunk; }

// Prints the token error message
static void errorAt(Token *token, const char *message) {
    // If we have already reported an error, ignore subsequent errors to avoid
    // errors cascading
    if (parser.panicMode) return;
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
        if (parser.current.type != TOKEN_ERROR) break;

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

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() { emitByte(OP_RETURN); }

static int makeConstant(Value value) {
    int constant = -1;
    Chunk *chunk = currentChunk();
    // Risky optimisation to reuse values in the constant pool to avoid filling it up
    // This assumes that we never remove values from the pool?
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

static void initCompiler(Compiler *compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    initTable(&compiler->globalMutability);
    current = compiler;
}

static void endCompiler() {
    emitReturn();
    // TODO this assumes that Compiler=current.
    freeTable(&current->globalMutability);
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void beginScope() { current->scopeDepth++; }

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
           current->scopeDepth) {
        emitByte(OP_POP);
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
            return;  // Unreachable.
    }
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
            return;  // Unreachable
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
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        Value mutableVal;
        Value* key = &currentChunk()->constants.values[arg];
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
            return;  // Unreachable
    }
}

// Recall that this is just an array and that enums have a corresponding number.
// This table makes it easy to see which tokens are available for use too!
ParseRule rules[] = {
        [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
        [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
        [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
        [TOKEN_MINUS] = {unary, binary, PREC_TERM},
        [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
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
        [TOKEN_AND] = {NULL, NULL, PREC_NONE},
        [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
        [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
        [TOKEN_IF] = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL] = {literal, NULL, PREC_NONE},
        [TOKEN_OR] = {NULL, NULL, PREC_NONE},
        [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
        [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
        [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
        [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
        [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
        [TOKEN_FINAL] = {NULL, NULL, PREC_NONE},
        [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
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
    if (a->length != b->length) return false;
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
}

static void declareVariable(bool mutable) {
    if (current->scopeDepth == 0) return;

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
    if (current->scopeDepth > 0) return 0;

    uint8_t constPoolIndex = identifierConstant(&parser.previous);
    Value *varName = &currentChunk()->constants.values[constPoolIndex];
    tableSet(&current->globalMutability, varName, BOOL_VAL(mutable));
}

static void markInitialized() {
    // Previously it was set to -1.
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        // Nothing to do for local variables.
        return;
    }

    // 'global' is the index of the name of the variable in the constants table.
    emitBytes(OP_DEFINE_GLOBAL, global);
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

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void synchronize() {
    parser.panicMode = false;

    // Skip tokens until we're at a statement boundary.
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
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
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else if (match(TOKEN_FINAL)) {
        finalVarDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();
    return !parser.hadError;
}
