#!/usr/bin/ruby

require "optparse"


def replaceinfile(mapping, file)
  $stderr.printf("now processing %p ...\n", file)
  count = 0 
  data = File.read(file)
  mapping.each do |old, new|
    rx = /\b#{old}\b/
    if data.match?(rx) then
      data.gsub!(rx, new)
      count += 1
    end
  end
  if count > 0 then
    File.write(file, data)
  end
end

def replaceinfiles(mapping)
  Dir.glob("*.{h,c}") do |file|
    replaceinfile(mapping, file)
  end
end


SOURCE = <<'__eos__'

    LITERAL_EXPRESSION,
    BINARY_EXPRESSION,
    UNARY_EXPRESSION,
    VAR_EXPRESSION,
    ASSIGN_EXPRESSION,
    CALL_EXPRESSION,
    SET_EXPRESSION,
    GET_EXPRESSION,
    LAMBDA_EXPRESSION,
    ARRAY_EXPRESSION,
    OBJECT_EXPRESSION,
    SUBSCRIPT_EXPRESSION,
    THIS_EXPRESSION,
    SUPER_EXPRESSION,
    RANGE_EXPRESSION,
    TERNARY_EXPRESSION,
    INTERPOLATION_EXPRESSION,
    REFERENCE_EXPRESSION,

    EXPRESSION_STATEMENT,
    BLOCK_STATEMENT,
    IF_STATEMENT,
    WHILE_STATEMENT,
    FOR_STATEMENT,
    VAR_STATEMENT,
    CONTINUE_STATEMENT,
    BREAK_STATEMENT,
    FUNCTION_STATEMENT,
    RETURN_STATEMENT,
    METHOD_STATEMENT,
    CLASS_STATEMENT,
    FIELD_STATEMENT
__eos__

def makemapping
  map = {}
  items = SOURCE.strip.split(",").map(&:strip).reject(&:empty?)
  items.each.with_index do |str, i|
    newname = str.gsub(/_(EXPRESSION|STATEMENT)$/, "").gsub(/_/, "")
    newname = "LIT_EXPR_" + newname
    if map.key?(str) then
      $stderr.printf("!!! key %p already exists (was %p)\n", newname, map[str])
      exit(1)
    end
    map[str] = newname
    
    $stderr.printf("%p -> %p\n", str, newname)
  end
  return map
end

begin
  doit = false
  OptionParser.new{|prs|
    prs.on("-f", "--doit"){
      doit = true
    }
  }.parse!
  mapping = makemapping()
  if doit then
    replaceinfiles(mapping)
  end
end

