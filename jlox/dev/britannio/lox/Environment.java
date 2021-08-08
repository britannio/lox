package dev.britannio.lox;

import java.util.HashMap;
import java.util.Map;

/**
 * Stores global variables.
 */
public class Environment {
    private final Map<String, Object> values = new HashMap<>();

    public void define(String name, Object value) {
        // Allows variables to be redefined e.g.,
        // var a = 1;
        // var a = 2;
        // This is convenient for REPL mode.
        values.put(name, value);
    }

    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        throw new RuntimeError(name, String.format("Undefined variable '%s'.", name.lexeme));
    }

}
