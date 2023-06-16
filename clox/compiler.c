#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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
typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

Parser parser;
Chunk *compilingChunk;

static Chunk *currentChunk() {
    return compilingChunk;
}

// Prints the token error message
static void errorAt(Token *token, const char *message) {
    // If we have already reported an error, ignore subsequent errors to avoid errors cascading
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

static void error(const char *message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message);
}

// Get the next token that is not an error token.
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        // Recall that during scanning, if we encounter an error, a standard Token is produced.
        // The type of this token is TOKEN_ERROR and instead of the token pointing to a string range of source code,
        // It points to an error message literal string!
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

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void expression();

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static void binary() {
    // The left operand has been consumed
    // The infix operator has also been consumed (held in parser.previous)

    TokenType operatorType = parser.previous.type;
    // Each binary operator's right-hand operand precedence is one level higher than its own (left associativity)
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
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

static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number() {
    // TODO how does it know how many characters to parse?
    // Maybe because a number can only be digits 0-9 or a dot so anything after that can be ignored.
    // Our program source is also null character terminated so we won't stray into memory we do not own.

    double value = strtod(parser.previous.start, NULL);
    emitConstant(value);
}

static void unary() {
    TokenType operatorType = parser.previous.type;

    // Compile the operand
    // When parsing an expression such as -a.b + c;, it will stop after -a.b
    parsePrecedence(PREC_UNARY);

    // emit the operator instruction.
    switch (operatorType) {
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
        [TOKEN_LEFT_PAREN]    = {grouping, NULL, PREC_NONE},
        [TOKEN_RIGHT_PAREN]   = {NULL, NULL, PREC_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL, NULL, PREC_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL, NULL, PREC_NONE},
        [TOKEN_COMMA]         = {NULL, NULL, PREC_NONE},
        [TOKEN_DOT]           = {NULL, NULL, PREC_NONE},
        [TOKEN_MINUS]         = {unary, binary, PREC_TERM},
        [TOKEN_PLUS]          = {NULL, binary, PREC_TERM},
        [TOKEN_SEMICOLON]     = {NULL, NULL, PREC_NONE},
        [TOKEN_SLASH]         = {NULL, binary, PREC_FACTOR},
        [TOKEN_STAR]          = {NULL, binary, PREC_FACTOR},
        [TOKEN_BANG]          = {NULL, NULL, PREC_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL]         = {NULL, NULL, PREC_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL, NULL, PREC_NONE},
        [TOKEN_GREATER]       = {NULL, NULL, PREC_NONE},
        [TOKEN_GREATER_EQUAL] = {NULL, NULL, PREC_NONE},
        [TOKEN_LESS]          = {NULL, NULL, PREC_NONE},
        [TOKEN_LESS_EQUAL]    = {NULL, NULL, PREC_NONE},
        [TOKEN_IDENTIFIER]    = {NULL, NULL, PREC_NONE},
        [TOKEN_STRING]        = {NULL, NULL, PREC_NONE},
        [TOKEN_NUMBER]        = {number, NULL, PREC_NONE},
        [TOKEN_AND]           = {NULL, NULL, PREC_NONE},
        [TOKEN_CLASS]         = {NULL, NULL, PREC_NONE},
        [TOKEN_ELSE]          = {NULL, NULL, PREC_NONE},
        [TOKEN_FALSE]         = {NULL, NULL, PREC_NONE},
        [TOKEN_FOR]           = {NULL, NULL, PREC_NONE},
        [TOKEN_FUN]           = {NULL, NULL, PREC_NONE},
        [TOKEN_IF]            = {NULL, NULL, PREC_NONE},
        [TOKEN_NIL]           = {NULL, NULL, PREC_NONE},
        [TOKEN_OR]            = {NULL, NULL, PREC_NONE},
        [TOKEN_PRINT]         = {NULL, NULL, PREC_NONE},
        [TOKEN_RETURN]        = {NULL, NULL, PREC_NONE},
        [TOKEN_SUPER]         = {NULL, NULL, PREC_NONE},
        [TOKEN_THIS]          = {NULL, NULL, PREC_NONE},
        [TOKEN_TRUE]          = {NULL, NULL, PREC_NONE},
        [TOKEN_VAR]           = {NULL, NULL, PREC_NONE},
        [TOKEN_WHILE]         = {NULL, NULL, PREC_NONE},
        [TOKEN_ERROR]         = {NULL, NULL, PREC_NONE},
        [TOKEN_EOF]           = {NULL, NULL, PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
    advance();
    // The first token is always part of a prefix expression. We cannot see an infix expression like + before its prefix.
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        // We hit a syntax error.
        error("Expect expression.");
        return;
    }

    // Run the prefix parsing function that matches the token we observed.
    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }

}

// Looks up the token in [rules] to find the corresponding ParseRule
static ParseRule *getRule(TokenType type) {
    // This also serves as an indirection.
    // binary() uses getRule() so we could declare binary() after getRule()
    // binary() is declared before the rules array so that the array/table can reference it.
    // So binary() cannot also access the array.

    // declaring binary() after getRule() doesn't fix the problem as getRule() would then have to be above the table...
    // To break this cycle, we can use forward declarations.
    return &rules[type];
}

static void expression() {
    // Parse at the lowest precedence level
    parsePrecedence(PREC_ASSIGNMENT);
}

bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();
    return !parser.hadError;
}
