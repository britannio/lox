package dev.britannio.lox;

import dev.britannio.lox.Expr.Binary;
import dev.britannio.lox.Expr.Grouping;
import dev.britannio.lox.Expr.Literal;
import dev.britannio.lox.Expr.Unary;

public class Interpreter implements Expr.Visitor<Object>{

    @Override
    public Object visitBinaryExpr(Binary expr) {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public Object visitGroupingExpr(Grouping expr) {
        return evaluate(expr.expression);
    }

    @Override
    public Object visitLiteralExpr(Literal expr) {
        return expr.value;
    }

    @Override
    public Object visitUnaryExpr(Unary expr) {
        // TODO Auto-generated method stub
        return null;
    }
    
}
