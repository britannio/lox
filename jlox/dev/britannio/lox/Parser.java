package dev.britannio.lox;

import java.util.List;

import static dev.britannio.lox.TokenType.*;

// EXPRESSION GRAMMAR
// --------------------------------------------------------------
// expression     → equality ;
// equality       → comparison ( ( "!=" | "==" ) comparison )* ;
// comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
// term           → factor ( ( "-" | "+" ) factor )* ;
// factor         → unary ( ( "/" | "*" ) unary )* ;
// unary          → ( "!" | "-" ) unary | primary ;
// primary        → number | string | "true" | "false" | "nil" | "(" expression ")" ; 

/**
 * Converts a sequence of tokens into a syntax tree using Recursive Descent 
 * 
 * Recursive descent is top-down as it starts from the top/outermost grammar
 * (expression) and works down to nested sub expressions before reaching 
 * the leaves of the syntax tree (primary).
 * 
 * Expressions:
 * <ul>
 * <li>expression</li>
 * <li>equality</li>
 * <li>comparison</li>
 * <li>term</li>
 * <li>factor</li>
 * <li>unary</li>
 * <li>primary</li>
 * </ul>
 * 
 */
class Parser {
    private static class ParseError extends RuntimeException {}

    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    public Expr parse() {
        try {
            return expression();
        } catch (ParseError error) {
           return null; 
        }
    }

    /**
     * BNF: equality
     * 
     * @return
     */
    private Expr expression() {
        return equality();
    }

    /**
     * BNF: comparison ( ( "!=" | "==" ) comparison )*
     * 
     * @return
     */
    private Expr equality() {
        // The comparision on the left.
        Expr expr = comparison();

        // Capture != or == followed by another comparison, 0 or more times
        while (match(BANG_EQUAL, EQUAL_EQUAL)) {
            Token operator = previous();
            Expr right = comparison();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * BNF: term ( ( ">" | ">=" | "<" | "<=" ) term )*
     * 
     * @return
     */
    private Expr comparison() {
        Expr expr = term();

        while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
            Token operator = previous();
            Expr right = term();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * BNF: factor ( ( "-" | "+" ) factor )*
     * @return
     */
    private Expr term() {
        Expr expr = factor();

        while (match(MINUS, PLUS)) {
            Token operator = previous();
            Expr right = factor();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    } 

    /**
     * BNF: unary ( ( "/" | "*" ) unary )*
     * @return
     */
    private Expr factor() {
        Expr expr = unary();
        
        while(match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * BNF: ( "!" | "-" ) unary | primary
     * @return
     */
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator= previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return primary();
    }


    private Expr primary() {
        if (match(FALSE)) return new Expr.Literal(false);
        if (match(TRUE)) return new Expr.Literal(true);
        if (match(NIL)) return new Expr.Literal(null);
        
        if (match(NUMBER, STRING)) {
            return new Expr.Literal(previous().literal);
        }

        if (match(LEFT_PAREN)) {
            Expr expr = expression();
            consume(RIGHT_PAREN, "Expect ')' after expression.");
            return new Expr.Grouping(expr);
        }

        throw error(peek(), "Expect expression.");

    }


    /**
     * @param types
     * @return true if the current token is equal to one of the provided tokens
     */
    private boolean match(TokenType... types) {
        for (TokenType type : types) {
            if (check(type)) {
                advance();
                return true;
            }
        }

        return false;
    }

    /**
     * Consumes the proved type. If the current token does not match the provied
     * type, an error is thrown with the provided message.
     * @param type
     * @param message
     * @return
     */
    private Token consume(TokenType type, String message) {
        if (check(type)) return advance();
        
        throw error(peek(), message);
    }

    /**
     * @param type
     * @return true if the token type is equal to the current token type.
     */
    private boolean check(TokenType type) {
        if (isAtEnd())
            return false;
        return peek().type == type;
    }

    /**
     * Consumes and returns the current token.
     * @return the current token.
     */
    private Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    /**
     * @return true if the current token is the last token.
     */
    private boolean isAtEnd() {
        return peek().type == EOF;
    }



    /**
     * @return the current token <b>without</b> consuming it.
     */
    private Token peek(){
        return tokens.get(current);
    }

    /**
     * @return the previous token.
     */
    private Token previous() {
        return tokens.get(current -1);
    }

    private ParseError error(Token token, String message){
        Lox.error(token, message);
        return new ParseError();
    }
        
    /**
     * Consume tokens until the end of the current statement is reached or the
     * current token resembles a new statement.
     */
    private void synchronize() {
        advance();

        while (!isAtEnd()) {
            if (previous().type == SEMICOLON) return;
            
            switch (peek().type) {
                case CLASS:
                case FUN:
                case VAR:
                case FOR:
                case IF:
                case WHILE:
                case PRINT:
                case RETURN:
                    return;
                default:
            }

            advance();
        }
    }

}