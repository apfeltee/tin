
enums = <<'__eos__'
    // Preprocessor errors
    LITERROR_UNCLOSED_MACRO,
    LITERROR_UNKNOWN_MACRO,

    // Scanner errors
    LITERROR_UNEXPECTED_CHAR,
    LITERROR_UNTERMINATED_STRING,
    LITERROR_INVALID_ESCAPE_CHAR,
    LITERROR_INTERPOLATION_NESTING_TOO_DEEP,
    LITERROR_NUMBER_IS_TOO_BIG,
    LITERROR_CHAR_EXPECTATION_UNMET,

    // Parser errors
    LITERROR_EXPECTATION_UNMET,
    LITERROR_INVALID_ASSIGMENT_TARGET,
    LITERROR_TOO_MANY_FUNCTION_ARGS,
    LITERROR_MULTIPLE_ELSE_BRANCHES,
    LITERROR_VAR_MISSING_IN_FORIN,
    LITERROR_NO_GETTER_AND_SETTER,
    LITERROR_STATIC_OPERATOR,
    LITERROR_SELF_INHERITED_CLASS,
    LITERROR_STATIC_FIELDS_AFTER_METHODS,
    LITERROR_MISSING_STATEMENT,
    LITERROR_EXPECTED_EXPRESSION,
    LITERROR_DEFAULT_ARG_CENTRED,

    // Emitter errors
    LITERROR_TOO_MANY_CONSTANTS,
    LITERROR_TOO_MANY_PRIVATES,
    LITERROR_VAR_REDEFINED,
    LITERROR_TOO_MANY_LOCALS,
    LITERROR_TOO_MANY_UPVALUES,
    LITERROR_VARIABLE_USED_IN_INIT,
    LITERROR_JUMP_TOO_BIG,
    LITERROR_NO_SUPER,
    LITERROR_THIS_MISSUSE,
    LITERROR_SUPER_MISSUSE,
    LITERROR_UNKNOWN_EXPRESSION,
    LITERROR_UNKNOWN_STATEMENT,
    LITERROR_LOOP_JUMP_MISSUSE,
    LITERROR_RETURN_FROM_CONSTRUCTOR,
    LITERROR_STATIC_CONSTRUCTOR,
    LITERROR_CONSTANT_MODIFIED,
    LITERROR_INVALID_REFERENCE_TARGET,
__eos__


src = <<__eos__
        case LITERROR_UNCLOSED_MACRO:
            return "(LITERROR_UNCLOSED_MACRO) unclosed macro.";
        case LITERROR_UNKNOWN_MACRO:
            return "(LITERROR_UNKNOWN_MACRO) unknown macro '%.*s'.";
        case LITERROR_UNEXPECTED_CHAR:
            return "(LITERROR_UNEXPECTED_CHAR) unexpected character '%c'";
        case LITERROR_UNTERMINATED_STRING:
            return "(LITERROR_UNTERMINATED_STRING) unterminated string";
        case LITERROR_INVALID_ESCAPE_CHAR:
            return "(LITERROR_INVALID_ESCAPE_CHAR) invalid escape character '%c'";
        case LITERROR_INTERPOLATION_NESTING_TOO_DEEP:
            return "(LITERROR_INTERPOLATION_NESTING_TOO_DEEP) interpolation nesting is too deep, maximum is %i";
        case LITERROR_NUMBER_IS_TOO_BIG:
            return "(LITERROR_NUMBER_IS_TOO_BIG) number is too big to be represented by a single literal";
        case LITERROR_CHAR_EXPECTATION_UNMET:
            return "(LITERROR_CHAR_EXPECTATION_UNMET) expected '%c' after '%c', got '%c'";
        case LITERROR_EXPECTATION_UNMET:
            return "(LITERROR_EXPECTATION_UNMET) expected %s, got '%.*s'";
        case LITERROR_INVALID_ASSIGMENT_TARGET:
            return "(LITERROR_INVALID_ASSIGMENT_TARGET) invalid assigment target";
        case LITERROR_TOO_MANY_FUNCTION_ARGS:
            return "(LITERROR_TOO_MANY_FUNCTION_ARGS) function cannot have more than 255 arguments, got %i";
        case LITERROR_MULTIPLE_ELSE_BRANCHES:
            return "(LITERROR_MULTIPLE_ELSE_BRANCHES) if-statement can have only one else-branch";
        case LITERROR_VAR_MISSING_IN_FORIN:
            return "(LITERROR_VAR_MISSING_IN_FORIN) for-loops using in-iteration must declare a new variable";
        case LITERROR_NO_GETTER_AND_SETTER:
            return "(LITERROR_NO_GETTER_AND_SETTER) expected declaration of either getter or setter, got none";
        case LITERROR_STATIC_OPERATOR:
            return "(LITERROR_STATIC_OPERATOR) operator methods cannot be static or defined in static classes";
        case LITERROR_SELF_INHERITED_CLASS:
            return "(LITERROR_SELF_INHERITED_CLASS) class cannot inherit itself";
        case LITERROR_STATIC_FIELDS_AFTER_METHODS:
            return "(LITERROR_STATIC_FIELDS_AFTER_METHODS) all static fields must be defined before the methods";
        case LITERROR_MISSING_STATEMENT:
            return "(LITERROR_MISSING_STATEMENT) expected statement but got nothing";
        case LITERROR_EXPECTED_EXPRESSION:
            return "(LITERROR_EXPECTED_EXPRESSION) expected expression after '%.*s', got '%.*s'";
        case LITERROR_DEFAULT_ARG_CENTRED:
            return "(LITERROR_DEFAULT_ARG_CENTRED) default arguments must always be in the end of the argument list.";
        case LITERROR_TOO_MANY_CONSTANTS:
            return "(LITERROR_TOO_MANY_CONSTANTS) too many constants for one chunk";
        case LITERROR_TOO_MANY_PRIVATES:
            return "(LITERROR_TOO_MANY_PRIVATES) too many private locals for one module";
        case LITERROR_VAR_REDEFINED:
            return "(LITERROR_VAR_REDEFINED) variable '%.*s' was already declared in this scope";
        case LITERROR_TOO_MANY_LOCALS:
            return "(LITERROR_TOO_MANY_LOCALS) too many local variables for one function";
        case LITERROR_TOO_MANY_UPVALUES:
            return "(LITERROR_TOO_MANY_UPVALUES) too many upvalues for one function";
        case LITERROR_VARIABLE_USED_IN_INIT:
            return "(LITERROR_VARIABLE_USED_IN_INIT) variable '%.*s' cannot use itself in its initializer";
        case LITERROR_JUMP_TOO_BIG:
            return "(LITERROR_JUMP_TOO_BIG) too much code to jump over";
        case LITERROR_NO_SUPER:
            return "(LITERROR_NO_SUPER) 'super' cannot be used in class '%s', because it does not have a super class";
        case LITERROR_THIS_MISSUSE:
            return "(LITERROR_THIS_MISSUSE) 'this' cannot be used %s";
        case LITERROR_SUPER_MISSUSE:
            return "(LITERROR_SUPER_MISSUSE) 'super' cannot be used %s";
        case LITERROR_UNKNOWN_EXPRESSION:
            return "(LITERROR_UNKNOWN_EXPRESSION) unknown expression with id '%i'";
        case LITERROR_UNKNOWN_STATEMENT:
            return "(LITERROR_UNKNOWN_STATEMENT) unknown statement with id '%i'";
        case LITERROR_LOOP_JUMP_MISSUSE:
            return "(LITERROR_LOOP_JUMP_MISSUSE) cannot use '%s' outside of loops";
        case LITERROR_RETURN_FROM_CONSTRUCTOR:
            return "(LITERROR_RETURN_FROM_CONSTRUCTOR) cannot use 'return' in constructors";
        case LITERROR_STATIC_CONSTRUCTOR:
            return "(LITERROR_STATIC_CONSTRUCTOR) constructors cannot be static (at least for now)";
        case LITERROR_CONSTANT_MODIFIED:
            return "(LITERROR_CONSTANT_MODIFIED) attempt to modify constant '%.*s'";
        case LITERROR_INVALID_REFERENCE_TARGET:
            return "(LITERROR_INVALID_REFERENCE_TARGET) invalid refence target";
__eos__

map = {}

src.gsub(/:\n/, ":").each_line do |ln|
  rx = /case\s*\b(?<ename>\w+)\b\s*:\s*return\s*"(?<str>.*?)"/
  m = ln.match(rx)
  if m == nil then
    $stderr.printf("cannot match %p\n", ln)
  end
  ename = m["ename"]
  str = m["str"].gsub(/^\s*\(\s*\w+\s*\)\s*/, "").downcase
  map[ename] = str
end

p map

