
$mapping = {
  "LITERROR_UNCLOSED_MACRO" => "unclosed macro.",
  "LITERROR_UNKNOWN_MACRO" => "unknown macro '%.*s'.",
  "LITERROR_UNEXPECTED_CHAR" => "unexpected character '%c'",
  "LITERROR_UNTERMINATED_STRING" => "unterminated string",
  "LITERROR_INVALID_ESCAPE_CHAR" => "invalid escape character '%c'",
  "LITERROR_INTERPOLATION_NESTING_TOO_DEEP" => "interpolation nesting is too deep, maximum is %i",
  "LITERROR_NUMBER_IS_TOO_BIG" => "number is too big to be represented by a single literal",
  "LITERROR_CHAR_EXPECTATION_UNMET" => "expected '%c' after '%c', got '%c'",
  "LITERROR_EXPECTATION_UNMET" => "expected %s, got '%.*s'",
  "LITERROR_INVALID_ASSIGMENT_TARGET" => "invalid assigment target",
  "LITERROR_TOO_MANY_FUNCTION_ARGS" => "function cannot have more than 255 arguments, got %i",
  "LITERROR_MULTIPLE_ELSE_BRANCHES" => "if-statement can have only one else-branch",
  "LITERROR_VAR_MISSING_IN_FORIN" => "for-loops using in-iteration must declare a new variable",
  "LITERROR_NO_GETTER_AND_SETTER" => "expected declaration of either getter or setter, got none",
  "LITERROR_STATIC_OPERATOR" => "operator methods cannot be static or defined in static classes",
  "LITERROR_SELF_INHERITED_CLASS" => "class cannot inherit itself",
  "LITERROR_STATIC_FIELDS_AFTER_METHODS" => "all static fields must be defined before the methods",
  "LITERROR_MISSING_STATEMENT" => "expected statement but got nothing",
  "LITERROR_EXPECTED_EXPRESSION" => "expected expression after '%.*s', got '%.*s'",
  "LITERROR_DEFAULT_ARG_CENTRED" => "default arguments must always be in the end of the argument list.",
  "LITERROR_TOO_MANY_CONSTANTS" => "too many constants for one chunk",
  "LITERROR_TOO_MANY_PRIVATES" => "too many private locals for one module",
  "LITERROR_VAR_REDEFINED" => "variable '%.*s' was already declared in this scope",
  "LITERROR_TOO_MANY_LOCALS" => "too many local variables for one function",
  "LITERROR_TOO_MANY_UPVALUES" => "too many upvalues for one function",
  "LITERROR_VARIABLE_USED_IN_INIT" => "variable '%.*s' cannot use itself in its initializer",
  "LITERROR_JUMP_TOO_BIG" => "too much code to jump over",
  "LITERROR_NO_SUPER" => "'super' cannot be used in class '%s', because it does not have a super class",
  "LITERROR_THIS_MISSUSE" => "'this' cannot be used %s",
  "LITERROR_SUPER_MISSUSE" => "'super' cannot be used %s",
  "LITERROR_UNKNOWN_EXPRESSION" => "unknown expression with id '%i'",
  "LITERROR_UNKNOWN_STATEMENT" => "unknown statement with id '%i'",
  "LITERROR_LOOP_JUMP_MISSUSE" => "cannot use '%s' outside of loops",
  "LITERROR_RETURN_FROM_CONSTRUCTOR" => "cannot use 'return' in constructors",
  "LITERROR_STATIC_CONSTRUCTOR" => "constructors cannot be static (at least for now)",
  "LITERROR_CONSTANT_MODIFIED" => "attempt to modify constant '%.*s'",
  "LITERROR_INVALID_REFERENCE_TARGET" => "invalid refence target",
}

def replaceinfile(file, ename, rstr)
  rx = /\b#{ename}\b/
  data = File.read(file)
  if data.match?(rx) then
    $stderr.printf("modifying %p ...\n", file)
    data.gsub!(rx, rstr)
    File.write(file, data)
  end
end

def replaceinfiles(ename, rstr)
  Dir.glob("*.{h,c}") do |file|
    replaceinfile(file, ename, rstr)
  end
end

$mapping.each do |ename, str|
  rstr = str.dump
  replaceinfiles(ename, rstr)
end

