package dev.britannio.lox;

public class Token {
    final TokenType type;

    /**
     * A unit of lexical meaning e.g., var, for, while, class
     */
    final String lexeme;
    final Object literal;
    final int line;


    public Token(TokenType type, String lexeme, Object literal, int line) {
        this.type = type;
        this.lexeme = lexeme;
        this.literal = literal;
        this.line = line;
    }

    public String toString() {
        return "<" + type + ", " + lexeme + ", " + literal + ">";
    }
}
