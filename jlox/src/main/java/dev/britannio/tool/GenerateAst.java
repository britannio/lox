package dev.britannio.tool;

import java.util.List;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;

// Reasoning behind the Visitor pattern

// Actions performed on expressions could be implemented in separate
// classes e.g., Interpreter, AstPrinter..., but the implementers
// would need to determine which expression subclass is being operated
// on before acting on it. 

// Sub classes don't behave like enums so we can't use a switch statement to 
// cover all variants. Instead, we could use a series of if statements with 
// "instanceof" performing the type checking but this isn't performant (some 
// expressions will be slower) by virtue of being latter on in the if chain).

// Another alternative would be to implement each action directly
// inside the expression but that leaves expressions responsible for
// cross cutting concerns which violates the single responsibiliy 
// principle. It also forces every expression to be updated when a new
// action is introduced which isn't ideal.

public class GenerateAst {

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: generate_ast <output directory>");
            System.exit(64);
        }
        String outputDir = args[0];

        // Expressions evaluate to a value
        defineAst(outputDir, "Expr", Arrays.asList( //
                "Literal  : Object value", //
                "Logical  : Expr left, Token operator, Expr right", //
                "Set      : Expr object, Token name, Expr value", //
                "This     : Token keyword", //
                "Unary    : Token operator, Expr right", //
                "Binary   : Expr left, Token operator, Expr right", //
                "Get      : Expr object, Token name", //
                "Call     : Expr callee, Token paren, List<Expr> arguments", //
                "Grouping : Expr expression", //
                "Variable : Token name", //
                "Assign   : Token name, Expr value"//
        ), Arrays.asList("import java.util.List;"));

        // Statements perform an action
        defineAst(outputDir, "Stmt", Arrays.asList( //
                "Expression : Expr expression", //
                "Function   : Token name, List<Token> params, List<Stmt> body", //
                "If         : Expr condition, Stmt thenBranch, Stmt elseBranch", //
                "Block      : List<Stmt> statements", //
                "Class      : Token name, List<Stmt.Function> methods", //
                "Print      : Expr expression", //
                "Return     : Token keyword, Expr value", //
                "Var        : Token name, Expr initializer", //
                "While      : Expr condition, Stmt body" //
        ), Arrays.asList("import java.util.List;"));
    }

    private static void defineAst(String outputDir, String baseName, List<String> types, List<String> imports)
            throws IOException {
        var path = outputDir + "/" + baseName + ".java";
        var writer = new PrintWriter(path, "UTF-8");

        writer.println("package dev.britannio.lox;");
        writer.println();
        for (String i : imports) {
            writer.println(i);
            writer.println();
        }
        writer.println("// GENERATED BY GenerateAst.java");

        writer.println();
        // writer.println("import java.util.List;");
        // writer.println("import dev.britannio.lox.Token;");
        // writer.println();
        writer.println("abstract class " + baseName + " {");

        defineVisitor(writer, baseName, types);

        // The base accept() method.
        writer.println();
        writer.println("  abstract <R> R accept(Visitor<R> visitor);");
        writer.println();

        // The AST class
        for (var type : types) {
            String className = type.split(":")[0].trim();
            String fields = type.split(":")[1].trim();
            defineType(writer, baseName, className, fields);
        }

        writer.print("}");
        writer.close();
    }

    private static void defineVisitor(PrintWriter writer, String baseName, List<String> types) {
        writer.println("  interface Visitor<R> {");

        for (var type : types) {
            var typeName = type.split(":")[0].trim();
            writer.println("    R visit" + typeName + baseName + "(" + typeName + " " + baseName.toLowerCase() + ");");
            writer.println();
        }
        writer.println("  }");
    }

    private static void defineType(PrintWriter writer, String baseName, String className, String fieldList) {
        writer.println("  static class " + className + " extends " + baseName + " {");

        // Constructor
        writer.println("    " + className + "(" + fieldList + ") {");

        // Store parameters in fields.
        String[] fields = fieldList.split(", ");

        for (var field : fields) {
            var name = field.split(" ")[1];
            writer.println("      this." + name + " = " + name + ";");
        }

        writer.println("    }");

        // Visitor pattern
        writer.println();
        writer.println("    @Override");
        writer.println("    <R> R accept(Visitor<R> visitor) {");
        writer.println("      return visitor.visit" + className + baseName + "(this);");
        writer.println("    }");

        // Fields
        writer.println();
        for (var field : fields) {
            writer.println("    final " + field + ";");
        }

        writer.println("  }");
        writer.println();

    }

}
