package dev.britannio.lox;

import java.util.HashMap;
import java.util.Map;

/**
 * Stores global variables.
 */
public class Environment {
    private final Map<String, Object> values = new HashMap<>();

    /**
     * Defines a new variable. If the variable exists, it's value is overridden.
     * @param name
     * @param value
     */
    void define(String name, Object value) {
        // Allows variables to be redefined e.g.,
        // var a = 1;
        // var a = 2;
        // This is convenient for REPL mode.
        values.put(name, value);
    }

    /**
     * Gets the variable belonging to the provided name. If no variable with the
     * provided name exists, a RuntimeError is thrown.
     * @param name
     * @return 
     */
    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            return values.get(name.lexeme);
        }

        throw new RuntimeError(name, String.format("Undefined variable '%s'.", name.lexeme));
    }

    /**
     * Assigns a value to an existing variable. If the variable does not already
     * exist then a RuntimeError is thrown.
     * @param name
     * @param value
     */
    void assign(Token name, Object value) {
        if (values.containsKey(name.lexeme)) {
            values.put(name.lexeme, value);
            return;
        }

        throw new RuntimeError(name, String.format("Undefined variable '%s'.", name.lexeme));
    }

}
