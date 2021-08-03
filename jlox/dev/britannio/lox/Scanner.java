package dev.britannio.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


import static dev.britannio.lox.TokenType.*;

public class Scanner {
    private final String source;
    private final List<Token> tokens = new ArrayList<>();

    /**
     * Location of the <b>first</b> character of the lexeme being scanned.
     */
    private int start = 0;

    /**
     * Location of the <b>current</b> character being scanned.
     */
    private int current = 0;

    private int line = 1;
    private int column = 0;

    /**
     * Reserved keywords in Lox.
     */
    private static final Map<String, TokenType> keywords;

    static {
        keywords = new HashMap<>();
        keywords.put("and", AND);
        keywords.put("class", CLASS);
        keywords.put("else", ELSE);
        keywords.put("false", FALSE);
        keywords.put("for", FOR);
        keywords.put("fun", FUN);
        keywords.put("if", IF);
        keywords.put("nil", NIL);
        keywords.put("or", OR);
        keywords.put("print", PRINT);
        keywords.put("return", RETURN);
        keywords.put("super", SUPER);
        keywords.put("this", THIS);
        keywords.put("true", TRUE);
        keywords.put("var", VAR);
        keywords.put("while", WHILE);
    }

    public Scanner(String source) {
        this.source = source;
    }

    List<Token> scanTokens() {
        while (!isAtEnd()) {
            start = current;
            scanToken();
        }

        tokens.add(new Token(EOF, "", null, line));
        return tokens;
    }

    private void scanToken() {
        // parse single character lexemes first.
        char c = advance();
        switch (c) {
            case '(' -> addToken(LEFT_PAREN);
            case ')' -> addToken(RIGHT_PAREN);
            case '{' -> addToken(LEFT_BRACE);
            case '}' -> addToken(RIGHT_BRACE);
            case ',' -> addToken(COMMA);
            case '.' -> addToken(DOT);
            case '-' -> addToken(MINUS);
            case '+' -> addToken(PLUS);
            case ';' -> addToken(SEMICOLON);
            case '*' -> addToken(STAR);
            case '"' -> string();
            // Matches 1-2 characters.
            case '!' -> addToken(match('=') ? BANG_EQUAL : BANG);
            case '=' -> addToken(match('=') ? EQUAL_EQUAL : BANG);
            case '<' -> addToken(match('=') ? LESS_EQUAL : BANG);
            case '>' -> addToken(match('=') ? GREATER_EQUAL : BANG);
            // Matches single line comments or division.
            case '/' -> {
                if (match('/')) {
                    // It's a comment, consume all characters until the end of the line.
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    // It represents division
                    addToken(SLASH);
                }
            }
            // Whitespace
            case ' ', '\r', '\t' -> {
            }
            // New line
            case '\n' -> advanceLine();
            default -> {
                if (isDigit(c)) {
                    number();
                } else if (isAlpha(c)) {
                    identifier();
                } else {

                    Lox.error(line, String.format("Unexpected character (%c) at column %d", c, column));
                }
            }
        }
    }

    /**
     * Consumes an identifier or a keyword
     */
    private void identifier() {
        while (isAlphaNumeric(peek())) advance();

        String text = source.substring(start, current);
        TokenType type = keywords.get(text);
        // The scanned content doesn't form a keyword.
        if (type == null) type = IDENTIFIER;
        addToken(type);
    }


    /**
     * Consumes an integer or decimal literal. E.g., 1234, 12.34
     */
    private void number() {
        while (isDigit(peek())) advance();

        // Look for a fractional part followed by at least one digit.
        if (peek() == '.' && isDigit(peekNext())) {
            // Consume the "."
            advance();

            while (isDigit(peek())) advance();
        }

        addToken(NUMBER, Double.parseDouble(source.substring(start, current)));
    }

    /**
     * Scans the contents of a string literal. Strings are represented via ""
     */
    private void string() {
        while (peek() != '"' && !isAtEnd()) {
            if (peek() == '\n') advanceLine();
            advance();
        }

        if (isAtEnd()) {
            Lox.error(line, "Unterminated string.");
            return;
        }

        // The closing ".
        advance();

        // Exclude the surrounding quotes from the token
        String value = source.substring(start + 1, current - 1);
        addToken(STRING, value);
    }


    private void advanceLine() {
        line++;
        column = 0;
    }

    private boolean match(char expected) {
        // There is no character at `current`.
        if (isAtEnd()) return false;
        // The current character is not the one we're looking for.
        if (source.charAt(current) != expected) return false;

        current++;
        return true;
    }

    /**
     * @return the current character without advancing the scanner to the next character.
     */
    private char peek() {
        // \0 is the null character.
        if (isAtEnd()) return '\0';
        return source.charAt(current);
    }

    /**
     * @return the next character without advancing the scanner.
     */
    private char peekNext() {
        if (current + 1 >= source.length()) return '\0';
        return source.charAt(current + 1);
    }

    private boolean isAlpha(char c) {
        return (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '_';
    }

    private boolean isAlphaNumeric(char c) {
        return isAlpha(c) || isDigit(c);
    }

    private boolean isDigit(char c) {
        return c >= '0' && c <= '9';
    }

    private boolean isAtEnd() {
        return current >= source.length();
    }

    private char advance() {
        // gets the character at current then increments current.
        column++;
        return source.charAt(current++);
    }

    private void addToken(TokenType type) {
        addToken(type, null);
    }

    private void addToken(TokenType type, Object literal) {
        String text = source.substring(start, current);
        tokens.add(new Token(type, text, literal, line));
    }
}
