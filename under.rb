#!/usr/bin/ruby

class Underscore
  def initialize
    @cachedata = {}
    @files = []
    @symtab = {}
    @files = Dir.glob("*.{h,c}")
  end

  def getdata(file)
    if @cachedata.key?(file) then
      return @cachedata[file]
    end
    data = File.read(file)
    @cachedata[file] = data
    return data
  end

  def issomecallable_infile(sym, file)
    d = getdata(file)
    arx = /\b#{sym}\b\s*\(/
    brx = /(->|\.)\b#{sym}\b/
    if d.match?(arx) || d.match?(brx) then
      return true
    end
    return false
  end

  def issomecallable(sym)
    @files.each do |file|
      if issomecallable_infile(sym, file) then
        return true
      end
    end
    return false
  end

  BADPATS = [
    /\b\w+_t\b/,
    /\blit_\w+\b/,
    /\bva_\w+\b/,
    /jmp_buf/,
  ]

  REPLACEME = {
    "was_allowed" => "wasallowed",
    "old_count" => "oldcount",
    "expression_root_count" => "exprrootcnt",
    "statement_root_count" => "stmtrootcnt",
    "jmp_buf" => "jmpbuf",
    "force_line" => "forceline",
    "argument_count" => "argc",
    "method_name" => "mthname",
    "signal_id" => "signalid",
    "old_size" => "oldsize",
    "new_size" => "newsize",
    "num_files" => "numfiles",
    "output_file" => "outputfile",
    "error_string" => "errorstring",
    "old_capacity" => "oldcapacity",
    "object_type" => "objecttype",
    "number_index" => "numberindex",
    "is_local" => "islocal",
    "reg_a" => "rega",
    "reg_b" => "regb",
    "arg_regs" => "argregs",
    "tmp_reg" => "tmpreg",
    "where_reg" => "wherereg",
    "value_reg" => "valuereg",
    "single_expression" => "singleexpr",
    "function_reg" => "functionreg",
    "closure_prototype" => "clsproto",
    "constant_index" => "constidx",
    "condition_reg" => "condreg",
    "condition_branch_skip" => "condbranchskip",
    "else_skip" => "elseskip",
    "else_start" => "elsestart",
    "ended_scope" => "endedscope",
    "end_jump_count" => "endjumpcount",
    "end_jumps" => "endjumps",
    "elseif_condition_reg" => "elseifcondreg",
    "next_jump" => "nextjump",
    "name_constant" => "nameconst",
    "before_condition" => "beforecond",
    "tmp_instruction" => "tmpinstr",
    "exit_jump" => "exitjump",
    "body_jump" => "bodyjump",
    "increment_start" => "incrstart",
    "tmp_reg_a" => "tmprega",
    "tmp_reg_b" => "tmpregb",
    "has_parent" => "hasparent",
    "field_name_constant" => "fieldnameconst",
    "module_value" => "modulevalue",
    "old_privates_count" => "oldprivatescnt",
    "line_index" => "lineindex",
    "jump_buffer" => "jumpbuffer",
    "did_setup_rules" => "didsetuprules",
    "prev_newline" => "prevnewline",
    "parser_prev_newline" => "parserprevnewline",
    "had_default" => "haddefault",
    "arg_name" => "argname",
    "arg_length" => "arglength",
    "first_arg_start" => "firstargstart",
    "first_arg_length" => "firstarglength",
    "had_arrow" => "hadarrow",
    "had_vararg" => "hadvararg",
    "def_value" => "defvalue",
    "had_args" => "hadargs",
    "had_paren" => "hadparen",
    "finished_parsing_fields" => "finishedparsingfields",
    "field_is_static" => "fieldisstatic",
    "string_type" => "stringtype",
    "is_hex" => "ishex",
    "is_binary" => "isbinary",
    "debug_instruction_functions" => "debuginstrfuncs",
    "current_line" => "currentline",
    "next_line" => "nextline",
    "prev_line" => "prevline",
    "output_line" => "outputline",
    "string_name" => "stringname",
    "args_copy" => "argscopy",
    "buffer_size" => "buffersize",
    "next_event" => "nextevent",
    "new_capacity" => "newcapacity",
    "target_arg_count" => "targetargcount",
    "bound_method" => "boundmethod",
    "value_amount" => "valueamount",
    "values_converted" => "valuesconverted",
    "buffer_index" => "bufferindex",
    "string_value" => "stringvalue",
    "buffer_length" => "bufferlength",
    "function_arg_count" => "functionargcount",
    "vararg_count" => "varargcount",
    "new_array" => "newarray",
    "pivot_index" => "pivotindex",
    "new_values" => "newvalues",
    "has_more" => "hasmore",
    "has_wrapper" => "haswrapper",
    "file_data" => "filedata",
    "max_length" => "maxlength",
    "directory_name" => "directoryname",
    "dir_name" => "dirname",
    "base_dir_name_length" => "basedirnamelength",
    "dir_name_length" => "dirnamelength",
    "total_length" => "totallength",
    "subdir_name" => "subdirname",
    "bytes_read" => "bytesread",
    "bytecode_version" => "bytecodeversion",
    "module_count" => "modulecount",
    "privates_count" => "privatescount",
    "num_read" => "numread",
    "static_random_data" => "staticrandomdata",
    "heap_chars" => "heapchars",
    "arg_list" => "arglist",
    "default_ending" => "defaultending",
    "default_ending_copying" => "defaultendingcopying",
    "old_registers" => "oldregisters",
    "old_slots" => "oldslots",
    "is_new" => "isnew",
    "repl_state" => "replstate",
    "tests_dir_length" => "testsdirlength",
    "name_length" => "namelength",
    "file_path" => "filepath",
    "files_to_run" => "filestorun",
    "num_files_to_run" => "numfilestorun",
    "arg_array" => "argarray",
    "show_repl" => "showrepl",
    "showed_help" => "showedhelp",
    "perform_tests" => "performtests",
    "bytecode_file" => "bytecodefile",
    "args_left" => "argsleft",
    "arg_string" => "argstring",
    "measure_compilation_time" => "measurecompilationtime",
    "last_source_time" => "lastsourcetime",
    "allowed_gc" => "allowedgc",
    "new_string" => "newstring",
    "compiled_modules" => "compiledmodules",
    "patched_file_name" => "patchedfilename",
    "native_function" => "nativefunction",
    "native_primitive" => "nativeprimitive",
    "native_method" => "nativemethod",
    "primitive_method" => "primitivemethod",
    "code_point" => "codepoint",
    "remaining_bytes" => "remainingbytes",
    "had_before" => "hadbefore",
    "callee_register" => "calleeregister",
    "previous_frame" => "previousframe",
    "alternate_callee" => "alternatecallee",
    "previous_upvalue" => "previousupvalue",
    "created_upvalue" => "createdupvalue",
    "current_chunk" => "currentchunk",
    "op_string" => "opstring",
    "tmp_a" => "tmpa",
    "tmp_b" => "tmpb",
    "dispatch_table" => "dispatchtable",
    "super_klass" => "superklass",
    "result_reg" => "resultreg",
    "field_name" => "fieldname",
    "instance_klass" => "instanceklass",

  }

  def whatwewant(sym)
    BADPATS.each do |bp|
      if sym.match?(bp) then
        return false
      end
    end
    if sym.match?(/\b[A-Z]+(\d*)?_/) || sym.match(/^_/) then
      return false
    end
    if sym.match?(/\w+_\w+/) then
      return !issomecallable(sym)
    end
    return false
  end

  def findinfile(file)
    rx = /\b(?<ident>\w{3,})\b/
    $stderr.printf("now processing %p ...\n", file)
    d = getdata(file)
    d.enum_for(:scan, rx).map{ Regexp.last_match }.each do |m|
      ident = m["ident"]
      if whatwewant(ident) then
        if !@symtab.key?(ident) then
          @symtab[ident] = 0
        end
        @symtab[ident] += 1
      end
    end
  end

  def main
    @files.each do |f|
      findinfile(f)
    end
    @symtab.each do |sym, cnt|
      #printf("  %p => %p,\n", sym, sym.gsub(/_/, ""))
    end
    $stderr.printf("now replacing ...\n")
    @files.each do |file|
      d = File.read(file)
      totalcnt = 0
      @symtab.each do |sym, cnt|
        rep = REPLACEME[sym]
        if rep != nil then
          rx = /\b#{sym}\b/
          if d.match?(rx) then
            totalcnt += 1
            d.gsub!(rx, rep)
          end
        end
      end
      if totalcnt > 0 then
        $stderr.printf("replacing %d identifiers in %p\n", totalcnt, file)
        File.write(file, d)
      end
    end
  end

end


begin
  u = Underscore.new
  u.main
end
