package dev.britannio.lox;

import java.util.List;
import java.util.Map;

/**
 * Stores class data and enables instances to be created.
 */
public class LoxClass implements LoxCallable {
    final String name;
    final LoxClass superclass;
    final Map<String, LoxFunction> methods;

    LoxClass(String name, LoxClass superclass, Map<String, LoxFunction> methods) {
        this.name = name;
        this.superclass = superclass;
        this.methods = methods;
    }

    LoxFunction findMethod(String name) {
        // Check if the method exists in this class.
        if (methods.containsKey(name)) {
            return methods.get(name);
        }

        // Check if the method exists in the super class.
        if (superclass != null) {
            return superclass.findMethod(name);
        }

        return null;
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public int arity() {
        LoxFunction initializer = findMethod("init");
        // No init method was defined.
        if (initializer == null) return 0;
        return initializer.arity();
    }

    /**
     * Creates a new instance of this class and invokes the init method if it
     * exists.
     */
    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        // Create a new instance of this LoxClass
        LoxInstance instance = new LoxInstance(this);
        LoxFunction initializer = findMethod("init");
        if (initializer != null) {
            // Connect the initializer to the instance then call it.
            initializer.bind(instance).call(interpreter, arguments);
        }
        return instance;
    }
}
