#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char *start;
    const char *current;
    int line;
} Scanner;

Scanner scanner;

void initScanner(const char *source) {
    // By initialising this way, de-referencing .start or .current gives you the first character of the source string.
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

// Returns the current character and moves to the next one
static char advance() {
    scanner.current++;
    // -1 just subtracts 1 from the pointer before de-referencing it
    return scanner.current[-1];
}

// Returns the current character.
static char peek() {
    return scanner.current[0];
//    return *scanner.current;
}

// Returns the next character.
static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int) (scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token errorToken(const char *message) {
    // A token pointing to the provided message string.
    // No need to worry about the lifetime of the message as it will
    // be a string literal.
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int) strlen(message);
    token.line = scanner.line;
    return token;
}

static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    // Comments span a full line
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;

        }
    }
}

// Checks if the next [length] characters match [rest], indicating that a keyword has been matched.
// Otherwise, assume it to be an identifier.
static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
    // start + length = the full length of the keyword.
    // Since the full keyword has been consumed, `scanner.current - scanner.start` should be the keyword length too.
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}

static TokenType identifierType() {
    // At this point, we have scanned the full token, but there's no need to do a hash table lookup.
    // 1. we don't even have hash tables, and 2. we can narrow down the keyword case by observing 1-2 characters.

    // Given that we have access to the full string (scanner.start to scanner.current), it would be nice if we
    // could use a switch statement over the token, however switching over strings is not possible in C :(.
    switch (scanner.start[0]) {
        case 'a':
            return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            // false, for, fun
            if (scanner.current - scanner.start > 1) {
                // We have a token with 2+ characters (as opposed to a variable with the name 'f')
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
        case 'i':
            return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            // this, true
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }


    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    // Scan token uses isAlpha() to match the first character.
    // Subsequent identifier characters may be digits too.
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

static Token number() {
    // Consume the number
    while (isDigit(peek())) advance();

    // Optionally consume the fractional part if we have a dot followed by a number
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the '.'.
        advance();

        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

// Consumes a string literal
static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote
    advance();
    return makeToken(TOKEN_STRING);
}

Token scanToken() {
    // We are at the beginning of the token
    // Skips whitespace and comments
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        // 1 character tokens
        case '(':
            return makeToken(TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(TOKEN_RIGHT_BRACE);
        case ';':
            return makeToken(TOKEN_SEMICOLON);
        case ',':
            return makeToken(TOKEN_COMMA);
        case '.':
            return makeToken(TOKEN_DOT);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '+':
            return makeToken(TOKEN_PLUS);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '*':
            return makeToken(TOKEN_STAR);
            // 2 character tokens
        case '!':
            return makeToken(
                    match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(
                    match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(
                    match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(
                    match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
            // String literal
        case '"':
            return string();
    }

    // If we get to this point, a token was not matched.
    return errorToken("Unexpected character.");
}
