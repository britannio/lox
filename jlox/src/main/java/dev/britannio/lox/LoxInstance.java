package dev.britannio.lox;

import java.util.HashMap;
import java.util.Map;

public class LoxInstance {
    private LoxClass klass;

    // Holds class variables
    private final Map<String, Object> fields = new HashMap<>();

    public LoxInstance(LoxClass klass) {
        this.klass = klass;
    }

    /**
     * Return a class variable or a method matching {@code name}
     */
    Object get(Token name) {
        // First look for a field
        if (fields.containsKey(name.lexeme)) {
            return fields.get(name.lexeme);
        }

        // A field wasn't found so look for a method
        LoxFunction method = klass.findMethod(name.lexeme);
        if (method != null) return method;

        // name isnt a method or field for this class.
        throw new RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
    }

    /**
     * Store a variable in this instance.
     */
    void set(Token name, Object value) {
        fields.put(name.lexeme, value);
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }

}
