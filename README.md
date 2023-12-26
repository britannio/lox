# Lox

Lox is a full-featured, efficient scripting language from Robert Nystrom's book, [Crafting Interpreters](https://craftinginterpreters.com/). 

In Jlox, the Java implementation of Lox, scripts are parsed using a recursive descent parser then interpreted by traversing the generated AST.

In Clox, the C implementation of Lox, scripts are compiled to bytecode then executed using a Bytecode Virtual Machine.

A more complete description of the features of Lox can be found at https://craftinginterpreters.com/the-lox-language.html.

# Jlox Setup

## Requirements

* Java 16
* Maven
* A Unix shell

## Setup

```sh
cd jlox/
mvn clean compile
./bin/run.sh <script-name|optional>
```
# Progress

<details>
  <summary><b>Jlox Progress Log</b></summary>

## 1 - Scanning

A raw expression can be scanned into a series of tokens.

```java
Input: var tau = 6.283185307;

Output (type, lexeme, literal):
[<VAR, var, null>, <IDENTIFIER, tau, null>, <BANG, =, null>, <NUMBER, 6.283, 6.283>, <EOF, , null>]
```


## 2 - Representing Code

An expression tree can be manually constructed and pretty printed!

```java
Expr expression = new Expr.Binary(
            new Expr.Unary(
                new Token(TokenType.MINUS, "-", null, 1),
                new Expr.Literal(123)
            ),
            new Token(TokenType.STAR, "*", null, 1),
            new Expr.Grouping(new Expr.Literal(45.67))
);
    
System.out.println(new AstPrinter().print(expression));
```

```java
Output: (* (- 123) (group 45.67))
Infix: -123 * 45.67
```

## 3 - Parsing Expressions

A list of tokens can be parsed and pretty printed!

```java
List<Token> tokens = scanner.scanTokens();

var parser = new Parser(tokens);
Expr expression  = parser.parse();

System.out.println(new AstPrinter().print(expression));
```

```java
Input: 1 + 2 * -3 / 4 == -0.5
Output: (== (+ 1.0 (/ (* 2.0 (- 3.0)) 4.0)) (- 0.5))
```

## 4 - Evaluating Expressions

An AST can be evaluated!

```java
var scanner = new Scanner(source);
// [<NUMBER, 1, 1.0>, <PLUS, +, null>, <NUMBER, 2, 2.0>, <STAR, *, null>, <MINUS, -, null>, <NUMBER, 3, 3.0>, <SLASH, /, null>, <NUMBER, 4, 4.0>, <EOF, , null>]
var tokens = scanner.scanTokens();

var parser = new Parser(tokens);
var expression = parser.parse();

interpreter.interpret(expression);
```

```java
Input: 1 + 2 * -3 / 4
Output: -0.5


Input: 1 + 2 * -3 / 4 == -0.5
Output: true
```

## 5 - Statements and State

- Variables can be declared, assigned and referenced in expressions.
- Statements can be grouped into blocks with local variable scope.
- Expressions can be printed.

Input:
```dart
var a = "global a";
var b = "global b";
{
    var a = "local a";
    print a;
    print b;
}
print a;
print b;
```

Output:
```
local a
global b
global a
global b
```

## 6 - Control Flow

- With the addition of while/for loops and more importantly **if statements**, Jlox is now Turing Complete!
- Logical expressions (AND/OR) can be evaluated.

```dart
if (2 + 2 - 1 == 3) print "Quick math!"; 
else print "Slow math :(";
// prints Quick math!

var result = 0;
while (result != 5) {
    result = result + 1;
}
print result; // prints 5

print false and false or true; // prints true
```

## 7 - Functions

Functions can be declared and invoked!

```kotlin 
fun fib(n) {
  if (n <= 1) return n;
  return fib(n - 2) + fib(n - 1);
}

print fib(10); // prints 55

// A native function
print clock(); // prints seconds since Jan 1, 1970
```

## 8 - Resolving and Binding

Variables resolve to the correct scope.

```java
var a = "global";
{
  fun showA() {
    print a;
  }

  showA();
  var a = "block";
  showA();
}
 ```

Previously this would output:
```
global
block
```
It now outputs:
```
global
global
```

---

This code now produces an error during semantic analysis.
```java
{
    var a = 1;
    var a = 2;
}
```


## 9 - Classes

Classes can be created!

```java
class Person {
  // Class initialiser.
  init(name) {
    this.name = name;
  }

  // Class method.
  greet() {
    print "My name is " + this.name + " " + this.nameSuffix;
  }
}

var britannio = Person("Britannio");
britannio.nameSuffix = ":)";
britannio.greet(); // prints "My name is Britannio :)"
```

## 10 - Inheritance

Classes can inherit from super classes!


```java
class Keyboard {
  type() {
    print "Keyboard noises";
   }
}

class MechanicalKeyboard < Keyboard {
  type() {
    // super.type();
    print "Loud mechanical keyboard noises :)";
   }
}

MechanicalKeyboard().type(); // Loud mechanical keyboard noises :)
```
  
</details>


<details>
  <summary><b>Clox Progress Log</b></summary>

## 1 - Chunks of Bytecode

A chunk containing bytecode instructions can be created.

```
== test chunk ==
0000  123 OP_CONSTANT         0 '1.2'
0002    | OP_RETURN
```

## ...

forgot to take notes :(

## 5 - Types of Values

Until now, the only type was a number (double). This update introduces bool and 
nil values.

## 6 - Strings

Strings can be created. Strings are the first 'instance' of the `Obj` type.
C doesn't have struct inheritance but by making `Obj` the first field of 
`ObjString`, it becomes safe to cast one to the other.

As a challenge, I leveraged the *flexible array member* feature of C to store
the string character array within the `ObjString` struct rather than having
the struct store a pointer as this removes an indirection (better memory locality).


## 7 - Hash Tables

Hash tables that dynamically grow can be created.

The first use case for this is string interning where we use the hash table
as if it were a hash set. The benefit of mandatory string interning is that
string equality becomes a trivial pointer comparison. 

As a challenge, I switched out the `ObjString` key type for `Value` so that the
hash table key can be a string, bool, double, nil or any future object such as
functions and classes.

In doing so, I hit a bug where dereferencing a pointer was corrupting a struct.
It turns out that the pointer originated from a variable in another function and
since the function had completed, the pointer was no longer valid :(.

## 8 - Local Variables

Grammar:

```
statement      → exprStmt
| printStmt ;

declaration    → varDecl
| statement ;
```

## 9 - Global Variables



  
</details>