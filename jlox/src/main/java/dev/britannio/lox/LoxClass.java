package dev.britannio.lox;

import java.util.List;
import java.util.Map;

/**
 * Stores class data and enables instances to be created.
 */
public class LoxClass implements LoxCallable {
    final String name;
    final Map<String, LoxFunction> methods;

    LoxClass(String name, Map<String, LoxFunction> methods) {
        this.name = name;
        this.methods = methods;
    }

    LoxFunction findMethod(String name) {
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        return null;
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public int arity() {
        return 0;
    }

    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        // Create a new instance of this LoxClass
        LoxInstance instance = new LoxInstance(this);
        return instance;
    }
}
