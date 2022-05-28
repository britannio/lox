package dev.britannio.lox;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox {
    private static final Interpreter interpreter = new Interpreter();
    static boolean hadError = false;
    static boolean hadRuntimeError = false;

    public static void main(String[] args) throws IOException {

        if (args.length > 1) {
            System.out.println("Usage: jlox [script]");
            System.exit(64);
        } else if (args.length == 1) {
            runFile(args[0]);
        } else {
            runPrompt();
        }
    }

    /**
     * Reads the provided file from its path then runs it
     *
     * @param path the path of the file to run
     * @throws IOException if the file cannot be read
     */
    private static void runFile(String path) throws IOException {
        // Suitable for small files as it reads the whole file at once.
        byte[] bytes = Files.readAllBytes(Paths.get(path));
        run(new String(bytes, Charset.defaultCharset()), false);

        // The code has an error
        if (hadError)
            System.exit(65);
        if (hadRuntimeError)
            System.exit(70);
    }

    /**
     * Runs Lox in REPL(read-eval-print-loop) mode
     * 
     * @throws IOException
     */
    private static void runPrompt() throws IOException {
        InputStreamReader input = new InputStreamReader(System.in);
        BufferedReader reader = new BufferedReader(input);

        for (;;) {
            System.out.println("> ");
            String line = reader.readLine();
            // Exit shortcut was triggered
            if (line == null)
                break;
            run(line, true);
            // Resets for the next piece of code to run
            hadError = false;
        }
    }

    private static void run(String source, boolean printExpressions) {
        Scanner scanner = new Scanner(source);
        // Split the input into meaningful blobs of characters i.e. lexemes
        // [var language = "lox";] becomes [var] [language] [=] ["lox"] [;]
        List<Token> tokens = scanner.scanTokens();
        // System.out.println(tokens);

        // Parse tokens into a tree.
        var parser = new Parser(tokens);
        List<Stmt> statements = parser.parse();
        // System.out.println(new AstPrinter().print(statements));

        // Stop if a syntax error was encountered.
        if (hadError) return;

        Resolver resolver = new Resolver(interpreter);
        resolver.resolve(statements);
        // Stop if a resolution error was encountered
        if (hadError) return;

        interpreter.interpret(statements, new Interpreter.Config(printExpressions));

    }

    static void error(int line, String message) {
        report(line, "", message);
    }

    static void error(Token token, String message) {
        if (token.type == TokenType.EOF) {
            report(token.line, " at end", message);
        } else {
            report(token.line, " at '" + token.lexeme + "'", message);
        }
    }

    public static void runtimeError(RuntimeError error) {
        System.err.println(error.getMessage() + "\n[line " + error.token.line + "]");
        hadRuntimeError = true;
    }

    private static void report(int line, String where, String message) {
        System.err.println("[line " + line + "] Error" + where + ": " + message);
        hadError = true;
    }

}
