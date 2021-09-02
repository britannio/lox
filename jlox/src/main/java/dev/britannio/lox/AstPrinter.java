package dev.britannio.lox;

import java.util.Arrays;
import java.util.List;

class AstPrinter implements Expr.Visitor<String>, Stmt.Visitor<String> {

    public static void main(String[] args) {
        Expr expression = new Expr.Binary(
                new Expr.Unary(new Token(TokenType.MINUS, "-", null, 1), new Expr.Literal(123)),
                new Token(TokenType.STAR, "*", null, 1), new Expr.Grouping(new Expr.Literal(45.67)));

        var block = new Stmt.Block(Arrays.asList( //
                new Stmt.Var(new Token(TokenType.VAR, "a", null, 1), expression),
                new Stmt.Var(new Token(TokenType.VAR, "b", null, 1), expression),
                new Stmt.Block(Arrays.asList(new Stmt.Var(new Token(TokenType.VAR, "a", null, 1), expression),
                        new Stmt.Var(new Token(TokenType.VAR, "b", null, 1), expression)))//
        ));
        System.out.println(new AstPrinter().print(Arrays.asList(block)));
    }

    String print(Expr expr) {
        return expr.accept(this);
    }

    String print(List<Stmt> statements) {
        var builder = new StringBuilder();
        for (Stmt statement : statements) {
            builder.append(statement.accept(this));
        }

        return builder.toString();
    }

    @Override
    public String visitBinaryExpr(Expr.Binary expr) {
        return parenthesize(expr.operator.lexeme, expr.left, expr.right);
    }

    @Override
    public String visitGroupingExpr(Expr.Grouping expr) {
        return parenthesize("group", expr.expression);
    }

    @Override
    public String visitLiteralExpr(Expr.Literal expr) {
        if (expr.value == null)
            return "nil";
        if (expr.value instanceof String)
            return "\"" + expr.value + "\"";
        return expr.value.toString();
    }

    @Override
    public String visitUnaryExpr(Expr.Unary expr) {
        return parenthesize(expr.operator.lexeme, expr.right);
    }

    private String parenthesize(String name, Expr... exprs) {
        StringBuilder builder = new StringBuilder();

        builder.append("(").append(name);
        for (Expr expr : exprs) {
            builder.append(" ");
            builder.append(expr.accept(this));
        }
        builder.append(")");

        return builder.toString();
    }

    @Override
    public String visitVariableExpr(Expr.Variable expr) {
        return expr.name.lexeme;
    }

    @Override
    public String visitExpressionStmt(Stmt.Expression stmt) {
        return stmt.expression.accept(this);
    }

    @Override
    public String visitPrintStmt(Stmt.Print stmt) {
        return "print " + stmt.expression.accept(this) + ";\n";
    }

    @Override
    public String visitVarStmt(Stmt.Var stmt) {
        String expr = stmt.initializer.accept(this);
        return "var " + stmt.name.lexeme + " = " + expr + ";\n";
    }

    @Override
    public String visitAssignExpr(Expr.Assign expr) {
        return expr.name.lexeme + " = " + expr.value.accept(this);

    }

    @Override
    public String visitBlockStmt(Stmt.Block stmt) {
        var str = new StringBuilder();

        str.append("{\n");
        for (Stmt statement : stmt.statements) {
            var resolved = statement.accept(this);
            str.append(indent(resolved));
        }
        str.append("}\n");

        return str.toString();

    }

    @Override
    public String visitIfStmt(Stmt.If stmt) {
        var str = new StringBuilder();
        str.append("if (");
        str.append(stmt.condition.accept(this));
        str.append(") ");
        str.append(stmt.thenBranch.accept(this));

        if (stmt.elseBranch != null) {
            str.append("else ");
            str.append(stmt.thenBranch.accept(this));
        }

        return str.toString();
    }

    @Override
    public String visitLogicalExpr(Expr.Logical expr) {
        var str = new StringBuilder();

        str.append(expr.left.accept(this));
        switch (expr.operator.type) {
            case OR:
                str.append(" OR ");
                break;
            case AND:
                str.append(" AND ");
                break;
            default:
                break;
        }
        str.append(expr.right.accept(this));

        return str.toString();

    }

    @Override
    public String visitWhileStmt(Stmt.While stmt) {
        var str = new StringBuilder();

        str.append("while (");
        str.append(stmt.condition.accept(this));
        str.append(")\n");
        str.append(stmt.body.accept(this));

        return str.toString();
    }

    @Override
    public String visitCallExpr(Expr.Call expr) {
        var str = new StringBuilder();

        str.append(expr.callee.accept(this));
        str.append("(");
        for (int i = 0; i < expr.arguments.size(); i++) {
            var arg = expr.arguments.get(i);
            str.append(arg.accept(this));

            if (i + 1 < expr.arguments.size()) {
                // Not the last argument
                str.append(", ");
            }
        }
        str.append(")");

        return str.toString();
    }

    @Override
    public String visitFunctionStmt(Stmt.Function stmt) {
        var str = new StringBuilder();
        str.append("fun ");

        str.append(stmt.name.lexeme);
        str.append("(");
        for (int i = 0; i < stmt.params.size(); i++) {
            var token = stmt.params.get(i);
            str.append(token.lexeme);

            if (i + 1 < stmt.params.size()) {
                // Not the last parameter
                str.append(", ");
            }
        }
        str.append(") {\n");

        for (var statement : stmt.body) {
            str.append(indent(statement.accept(this)));
        }

        str.append("}\n");
        return str.toString();
    }

    @Override
    public String visitReturnStmt(Stmt.Return stmt) {
        var str = new StringBuilder();

        str.append("return ");
        str.append(stmt.value.accept(this));
        str.append(";\n");

        return str.toString();
    }

    private String indent(String value) {
        var str = new StringBuilder();
        var lines = value.split("\n");

        for (var line : lines) {
            str.append("  ");
            str.append(line);
            str.append("\n");
        }

        return str.toString();
    }

    @Override
    public String visitClassStmt(Stmt.Class stmt) {
        var str = new StringBuilder();

        str.append("class ");
        str.append(stmt.name.lexeme);
        str.append(" {\n");

        for (Stmt method : stmt.methods) {
            str.append(indent(method.accept(this)));
        }
        str.append("}\n");

        return str.toString();
    }

    @Override
    public String visitSetExpr(Expr.Set expr) {
        var str = new StringBuilder();

        // str.append(expr.object.accept(this));
        // str.append(".");
        str.append(expr.name.lexeme);
        str.append(" = ");
        str.append(expr.value.accept(this));

        return str.toString();
    }

    @Override
    public String visitGetExpr(Expr.Get expr) {
        return expr.name.lexeme;

    }

    @Override
    public String visitThisExpr(Expr.This expr) {
        return "this";
    }

    @Override
    public String visitSuperExpr(Expr.Super expr) {
        return "";
    }

}
