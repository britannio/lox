package dev.britannio.lox;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static dev.britannio.lox.TokenType.*;

/* Lox GRAMMAR
--------------------------------------------------------------
program        → declaration* EOF 
declaration    → varDecl | statement
varDecl        → "var" IDENTIFIER ( "=" expression )? ";"
statement      → exprStmt | forStmt | ifStmt | printStmt | whileStmt | block
exprStmt       → expression ";"?
forStmt        → "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")" statement 
ifStmt         → "if" "(" expression ")" statement ( "else" statement )?
printStmt      → "print" expression ";"? 
whileStmt      → "while" "(" expression ")" statement
block          → "{" declaration* "}"
expression     → assignment
assignment     → IDENTIFIER "=" assignment | logic_or
logic_or       → logic_and ( "or" logic_and )*
logic_and      → equality ( "and" equality )*
equality       → comparison ( ( "!=" | "==" ) comparison )* 
comparison     → term ( ( ">" | ">=" | "<" | "<=" ) term )* 
term           → factor ( ( "-" | "+" ) factor )* 
factor         → unary ( ( "/" | "*" ) unary )* 
unary          → ( "!" | "-" ) unary | primary 
primary        → NUMBER | STRING | "true" | "false" | "nil" | "(" expression ")" | IDENTIFIER 
*/

/**
 * Converts a sequence of tokens into a syntax tree using Recursive Descent
 * 
 * Recursive descent is top-down as it starts from the top/outermost grammar
 * (expression) and works down to nested sub expressions before reaching the
 * leaves of the syntax tree (primary).
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
    private static class ParseError extends RuntimeException {
    }

    private final List<Token> tokens;
    private int current = 0;

    Parser(List<Token> tokens) {
        this.tokens = tokens;
    }

    public List<Stmt> parse() {
        List<Stmt> statements = new ArrayList<>();
        while (!isAtEnd()) {
            // declaration() can return null, should we be adding it to the list?
            statements.add(declaration());
        }

        return statements;
    }

    /**
     * BNF: assignment
     * 
     * @return
     */
    private Expr expression() {
        return assignment();
    }

    /**
     * BNF: varDecl | statement
     * 
     * @return
     */
    private Stmt declaration() {
        try {
            if (match(VAR)) {
                return varDeclaration();
            }

            return statement();
        } catch (ParseError error) {
            // Move to the next declaration.
            synchronize();
            return null;
        }
    }

    /**
     * BNF: exprStmt | forStmt | ifStmt | printStmt | whileStmt | block
     * 
     * @return
     */
    private Stmt statement() {
        if (match(FOR))
            return forStatement();
        if (match(IF))
            return ifStatement();
        if (match(PRINT))
            return printStatement();
        if (match(WHILE))
            return whileStatement();
        if (match(LEFT_BRACE))
            return new Stmt.Block(block());

        return expressionStatement();
    }

    /**
     * BNF: "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")"
     * statement
     * 
     * @return
     */
    private Stmt forStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'for'.");

        Stmt initialiser;
        if (match(SEMICOLON)) {
            // No initialiser
            initialiser = null;
        } else if (match(VAR)) {
            // varDecl initialiser
            initialiser = varDeclaration();
        } else {
            // exprStmt initialiser
            initialiser = expressionStatement();
        }

        // Loop condition
        Expr condition = null;
        if (!check(SEMICOLON)) {
            condition = expression();
        }
        consume(SEMICOLON, "Expect ';' after loop condition.");

        Expr increment = null;
        if (!check(RIGHT_PAREN)) {
            increment = expression();
        }

        consume(RIGHT_PAREN, "Expect ')' after for clauses");
        Stmt body = statement();

        // Now desugar the for loop into a while loop:
        //
        // for (initialiser; condition; increment) {
        // body
        // }
        //
        // becomes:
        //
        // initialiser;
        // while (condition) {
        // body
        // increment
        // }

        if (increment != null) {
            body = new Stmt.Block(Arrays.asList( //
                    body, //
                    new Stmt.Expression(increment)//
            ));
        }

        if (condition == null)
            condition = new Expr.Literal(true);
        body = new Stmt.While(condition, body);

        if (initialiser != null) {
            body = new Stmt.Block(Arrays.asList( //
                    initialiser, //
                    body //
            ));
        }

        return body;
    }

    /**
     * BNF: "if" "(" expression ")" statement ( "else" statement )?
     * 
     * Else statements belong to the closest if statement e.g., for the folliwng
     * statements, else whenFalse() belongs to if (second)
     * 
     * if (first) if (second) whenTrue(); else whenFalse();
     * 
     * @return
     */
    private Stmt ifStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'if'.");
        Expr condition = expression();
        consume(RIGHT_PAREN, "Expect ')' after if condition.");

        Stmt thenBranch = statement();
        Stmt elseBranch = null;
        if (match(ELSE)) {
            elseBranch = statement();
        }

        return new Stmt.If(condition, thenBranch, elseBranch);
    }

    /**
     * BNF: "print" expression ";"
     * 
     * @return
     */
    private Stmt printStatement() {
        Expr value = expression();
        consume(SEMICOLON, "Expect ';' after value.");
        return new Stmt.Print(value);
    }

    /**
     * BNF: "var" IDENTIFIER ( "=" expression )? ";"
     * 
     * @return
     */
    private Stmt varDeclaration() {
        Token name = consume(IDENTIFIER, "Expect variable name.");

        Expr initializer = null;
        if (match(EQUAL)) {
            initializer = expression();
        }

        consume(SEMICOLON, "Expect ';' after variable declaration.");
        return new Stmt.Var(name, initializer);
    }

    /**
     * BNF: "while" "(" expression ")" statement
     * 
     * @return
     */
    private Stmt whileStatement() {
        consume(LEFT_PAREN, "Expect '(' after 'while'.");
        Expr condition = expression();
        consume(RIGHT_PAREN, "Expect ')' after condition.");
        Stmt body = statement();

        return new Stmt.While(condition, body);
    }

    /**
     * BNF: expression ";"?
     * 
     * @return
     */
    private Stmt expressionStatement() {
        Expr expr = expression();
        match(SEMICOLON);
        return new Stmt.Expression(expr);
    }

    /**
     * BNF: "{" declaration* "}"
     * 
     * @return
     */
    private List<Stmt> block() {
        List<Stmt> statements = new ArrayList<>();

        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            statements.add(declaration());
        }

        consume(RIGHT_BRACE, "Expect '}' after block.");
        return statements;
    }

    /**
     * BNF: IDENTIFIER "=" assignment | logic_or
     * 
     * @return
     */
    private Expr assignment() {
        // Valid assignment targets (identifiers) can always be treated as
        // expressions e.g., the a in a=1; is valid on its own. If = is matched
        // then the expression is casted to a variable.
        Expr expr = or();

        if (match(EQUAL)) {
            Token equals = previous();
            Expr value = assignment();

            if (expr instanceof Expr.Variable) {
                Token name = ((Expr.Variable) expr).name;
                return new Expr.Assign(name, value);
            }

            error(equals, "Invalid assignment target.");
        }

        return expr;
    }

    /**
     * BNF: logic_and ( "or" logic_and )*
     * 
     * @return
     */
    private Expr or() {
        Expr expr = and();

        while (match(OR)) {
            Token operator = previous();
            Expr right = and();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
    }

    /**
     * BNF: equality ( "and" equality )*
     * 
     * @return
     */
    private Expr and() {
        Expr expr = equality();

        while (match(AND)) {
            // The consumed AND token
            Token operator = previous();
            Expr right = equality();
            expr = new Expr.Logical(expr, operator, right);
        }

        return expr;
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
     * 
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
     * 
     * @return
     */
    private Expr factor() {
        Expr expr = unary();

        while (match(SLASH, STAR)) {
            Token operator = previous();
            Expr right = unary();
            expr = new Expr.Binary(expr, operator, right);
        }

        return expr;
    }

    /**
     * BNF: ( "!" | "-" ) unary | primary
     * 
     * @return
     */
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return primary();
    }

    private Expr primary() {
        if (match(FALSE))
            return new Expr.Literal(false);
        if (match(TRUE))
            return new Expr.Literal(true);
        if (match(NIL))
            return new Expr.Literal(null);

        if (match(NUMBER, STRING)) {
            return new Expr.Literal(previous().literal);
        }

        if (match(IDENTIFIER)) {
            return new Expr.Variable(previous());
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
     * 
     * @param type
     * @param message
     * @return
     */
    private Token consume(TokenType type, String message) {
        if (check(type))
            return advance();

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
     * 
     * @return the current token.
     */
    private Token advance() {
        if (!isAtEnd())
            current++;
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
    private Token peek() {
        return tokens.get(current);
    }

    /**
     * @return the previous token.
     */
    private Token previous() {
        return tokens.get(current - 1);
    }

    private ParseError error(Token token, String message) {
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
            if (previous().type == SEMICOLON)
                return;

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