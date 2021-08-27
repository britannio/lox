package dev.britannio.lox;

import java.util.List;

import dev.britannio.lox.Stmt.Function;

public class LoxFunction implements LoxCallable {
    private final Stmt.Function declaration;
    private final Environment closure;

    public LoxFunction(Function declaration, Environment closure) {
        this.declaration = declaration;
        this.closure = closure;
    }

    /**
     * Creates an environment holding {@code instance} as 'this' and attaches
     * it to a copy of this function.
     */
    LoxFunction bind(LoxInstance instance) {
        Environment environment = new Environment(closure);
        environment.define("this", instance);
        return new LoxFunction(declaration, environment);
    }


    @Override
    public int arity() {
        return declaration.params.size();
    }
    
    /**
     * Creates an environment for the function then populates it with the
     * arguments of the function call before executing the function body.
     */
    @Override
    public Object call(Interpreter interpreter, List<Object> arguments) {
        Environment environment = new Environment(closure);

        // Add arguments to the environment
        for (int i = 0; i < declaration.params.size(); i++) {
            String argumentName = declaration.params.get(i).lexeme;
            environment.define(argumentName, arguments.get(i));
        }

        try {
            interpreter.executeBlock(declaration.body, environment);
        } catch (Return returnValue) {
            // Return was thrown to indicate that this function has terminated.
            return returnValue.value;
        }
        // The function reached the end of its body without a return statement.
        return null;
    }

    @Override
    public String toString() {
        return "<fn " + declaration.name.lexeme + ">";
    }

}
