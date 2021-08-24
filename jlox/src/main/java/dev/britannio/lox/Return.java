package dev.britannio.lox;

/**
 * Stores the value of a return statement. 
 */
public class Return extends RuntimeException {
    final Object value;

    Return(Object value) {
        super(null, null, false, false);
        this.value = value;
    }
}
