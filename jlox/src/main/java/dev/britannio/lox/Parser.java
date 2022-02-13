package dev.britannio.lox;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static dev.britannio.lox.TokenType.*;

/* Lox GRAMMAR (lowest to highest precedence)
--------------------------------------------------------------
program        → declaration* EOF 
declaration    → classDecl | funcDecl | varDecl | statement
classDecl      → "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}"
funDecl        → "fun" function
function       → IDENTIFIER "(" parameters? ")" block
parameters     → IDENTIFIER ( "," IDENTIFIER )*
varDecl        → "var" IDENTIFIER ( "=" expression )? ";"
statement      → exprStmt | forStmt | ifStmt | printStmt | returnStmt | whileStmt | block
returnStmt     → "return" expression? ";"
exprStmt       → expression ";"?
forStmt        → "for" "(" ( varDecl | exprStmt | ";" ) expression? ";" expression? ")" statement 
ifStmt         → "if" "(" expression ")" statement ( "else" statement )?
printStmt      → "print" expression ";"? 
whileStmt      → "while" "(" expression ")" statement
block          → "{" declaration* "}"
expression     → assignment
assignment     → ( call "." )? IDENTIFIER "=" assignment | logic_or
logic_or       → logic_and ( "or" logic_and )*
logic_and      → equality ( "and" equality )*
equality       → comparison ( ( "!=" | "==" ) comparison )* 
comparison     → shift ( ( ">" | ">=" | "<" | "<=" ) shift )* 
shift          → term ( ( ">>" | "<<" | ">>>" ) term )*
term           → factor ( ( "-" | "+" ) factor )* 
factor         → unary ( ( "/" | "*" ) unary )* 
unary          → ( "!" | "-" ) unary | call 
call           → primary ( "(" arguments? ")" | "." IDENTIFIER )*
arguments      → expression ( "," expression )*
primary        → "true" | "false" | "nil" | "this" | NUMBER | STRING | IDENTIFIER | "(" expression ")" | "super" "." IDENTIFIER
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
     * BNF: classDecl | funcDecl | varDecl | statement
     * 
     * @return
     */
    private Stmt declaration() {
        try {
            if (match(CLASS))
                return classDeclaration();
            if (match(FUN))
                return function("function");
            if (match(VAR))
                return varDeclaration();

            return statement();
        } catch (ParseError error) {
            // Move to the next declaration.
            synchronize();
            return null;
        }
    }

    /**
     * BNF: "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}"
     */
    private Stmt classDeclaration() {
        Token name = consume(IDENTIFIER, "Expect class name.");

        Expr.Variable superclass = null;
        if (match(LESS)) {
            consume(IDENTIFIER, "Expect superclass name.");
            superclass = new Expr.Variable(previous());
        }

        consume(LEFT_BRACE, "Expect '{' before class body");

        List<Stmt.Function> methods = new ArrayList<>();
        while (!check(RIGHT_BRACE) && !isAtEnd()) {
            // We haven't hit the closing '}' or the end of the file so the
            // class has more methods
            methods.add(function("method"));
        }

        consume(RIGHT_BRACE, "Expect '}' after class body.");

        return new Stmt.Class(name, superclass, methods);
    }

    /**
     * BNF: exprStmt | forStmt | ifStmt | printStmt | returnStmt | whileStmt | block
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
        if (match(RETURN))
            return returnSatement();
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

        consume(RIGHT_PAREN, "Expect ')' after for clauses.");
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
     * BNF: "return" expression? ";"
     * 
     * @return
     */
    private Stmt returnSatement() {
        Token keyword = previous();
        Expr value = null;
        if (!check(SEMICOLON)) {
            // An expression cannot begin with a semicolon so since it's not
            // present, an expression is present.
            value = expression();
        }

        consume(SEMICOLON, "Expect ';' after return value.");
        return new Stmt.Return(keyword, value);
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
        // match(SEMICOLON);
        consume(SEMICOLON, "Expect ';' after expression.");
        return new Stmt.Expression(expr);
    }

    /**
     * BNF: IDENTIFIER "(" parameters? ")" block
     * 
     * @param kind
     * @return
     */
    private Stmt.Function function(String kind) {
        Token name = consume(IDENTIFIER, "Expect " + kind + " name.");
        consume(LEFT_PAREN, "Expect '(' after " + kind + " name.");
        List<Token> parameters = new ArrayList<>();

        if (!check(RIGHT_PAREN)) {
            // The function has parameters
            do {
                if (parameters.size() >= 255) {
                    error(peek(), "Can't have more than 255 parameters.");
                }

                parameters.add(consume(IDENTIFIER, "Expect parameter name."));

            } while (match(COMMA));
        }
        consume(RIGHT_PAREN, "Expect ')' after parameters.");

        // Parse function body
        consume(LEFT_BRACE, "Expect '{' before " + kind + " body.");
        List<Stmt> body = block();
        return new Stmt.Function(name, parameters, body);
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
     * BNF: ( call "." )? IDENTIFIER "=" assignment | logic_or
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
            } else if (expr instanceof Expr.Get) {
                Expr.Get get = (Expr.Get) expr;
                return new Expr.Set(get.object, get.name, value);
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
     * BNF: shift ( ( ">" | ">=" | "<" | "<=" ) shift )*
     * 
     * @return
     */
    private Expr comparison() {
        Expr expr = shift();

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
     * BNF: term ( ( ">>" | "<<" ) term )*
     */
    private Expr shift() {
        Expr expr = term();

        while (match(SHIFT_LEFT, SHIFT_RIGHT, TRIPLE_SHIFT)) {
            Token operator = previous();
            Expr right = term();
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
     * BNF: ( "!" | "-" ) unary | call
     * 
     * @return
     */
    private Expr unary() {
        if (match(BANG, MINUS)) {
            Token operator = previous();
            Expr right = unary();
            return new Expr.Unary(operator, right);
        }

        return call();
    }

    /**
     * 
     * @param callee
     * @return
     */
    private Expr finishCall(Expr callee) {
        List<Expr> arguments = new ArrayList<>();
        if (!check(RIGHT_PAREN)) {
            do {
                if (arguments.size() >= 255) {
                    // We're still in a valid state so no error is thrown.
                    error(peek(), "Can't have more than 255 arguments.");
                }
                arguments.add(expression());
            } while (match(COMMA));
        }

        Token paren = consume(RIGHT_PAREN, "Expect ')' after arguments.");

        return new Expr.Call(callee, paren, arguments);
    }

    /**
     * BNF: primary ( "(" arguments? ")" | "." IDENTIFIER )*
     */
    private Expr call() {
        Expr expr = primary();

        while (true) {
            if (match(LEFT_PAREN)) {
                expr = finishCall(expr);
            } else if (match(DOT)) {
                Token name = consume(IDENTIFIER, "Expect property name after '.'.");
                expr = new Expr.Get(expr, name);
            } else {
                break;
            }
        }

        return expr;
    }

    /**
     * BNF: "true" | "false" | "nil" | "this" | NUMBER | STRING | IDENTIFIER | "("
     * expression ")" | "super" "." IDENTIFIER
     */
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

        if (match(SUPER)) {
            Token keyword = previous();
            consume(DOT, "Expect '.' after 'super'.");
            Token method = consume(IDENTIFIER, "Expect superclass method name.");
            return new Expr.Super(keyword, method);
        }

        if (match(THIS))
            return new Expr.This(previous());

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