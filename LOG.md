
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