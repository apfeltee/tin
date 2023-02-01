#!/usr/bin/ruby

# this file is a mess, and that's ok.

require "shellwords"

def deparse(str)
  tmp = []
  str.each_line do |ln|
    next if ln.match?(/^\s*#/)
    tmp.push(ln)
  end
  return tmp.join("\n").strip.shellsplit
end

DEF_CC    = %w(gcc)
DEF_CXX    = %w(g++ -std=c++20)

DEF_WFLAGS = %w(-Wall -Wextra)

DEF_CFLAGS = %w(-g3 -ggdb3)

DEF_LFLAGS = %w(-lm -lreadline)

DEF_EXTRA = deparse(%q(
))


EXENAME = "run"

SOURCEDIRS = [
  ".",
]
INCDIRS = [
  ".",
]

DEPSOURCEDIRS = [
]

DEPINCDIRS = [
]

# just SETTLE ON *****ONE****** file extension, PLEASE.
# for the love of god, WHY. WHY?
CXXEXTPATTERN = /\.(cpp|cxx|c\+\+|cc|c)$/i
=begin

   rule cc
      command = gcc -c -o $out $in -MMD -MF $out.d
      depfile = $out.d
      deps = gcc

=end


def getglobs(rx, *idirs)
  dirs = []
  idirs.each{|d| dirs |= Dir.glob(d) }
  dirs.each do |d|
    Dir.glob(d +"/*") do |itm|
      next unless (File.file?(itm) && itm.match?(rx))
      $stderr.printf("adding %p\n", itm)
      yield itm
    end
  end
end

def rule(out, name, **desc)
  out.printf("rule %s\n", name)
  desc.each do |name, val|
    val = (
      if val.is_a?(Array) then
        val.join(" ")
      else
        val
      end
    )
    out.printf("  %s = %s\n", name.to_s, val.to_s)
  end
  out.printf("\n")
end

def add_instr_exclude_dir(dir)
  buf = sprintf("-finstrument-functions-exclude-file-list=%p", dir.gsub(/\\/, "/"))
end

begin
  exe = EXENAME
  ofiles = []
  cflags = DEF_CFLAGS.dup
  lflags = DEF_LFLAGS.dup
  wflags = DEF_WFLAGS.dup
  cflags.push(*DEF_EXTRA)
  lflags.push(*DEF_EXTRA)
  if DEF_CXX.first.match?(/^g\+\+/) then
    lflags.push("-lstdc++fs")
  elsif DEF_CXX.first.match?(/^clang/) then
    #-finstrument-functions-exclude-file-list=
    if (einc = ENV["INCLUDE"])!= nil then
      dirs = einc.split(";").uniq
      a = dirs.join(";")
      #cflags.push(sprintf("-fprofile-filter-files=%p",a))
    end
  end
  (INCDIRS | DEPINCDIRS).each do |di|
    cflags.push("-I", di)
  end
  File.open("build.ninja", "wb") do |out|
    rule(out, "cc",
      deps: 'gcc',
      depfile: '$in.d',
      command: [*DEF_CXX, *wflags, *cflags, '-MMD', '-MF', '$in.d', '-c', '$in', '-o', '$out'],
      description: '[CC] $in -> $out',
    )
    rule(out, "link",
      command: [*DEF_CXX, *cflags, *lflags, "-o", '$out', '$in'],
      description: '[LINK] $out',
    )
    sourcedirs = [*DEPSOURCEDIRS, *SOURCEDIRS, "."]
    getglobs(CXXEXTPATTERN, *sourcedirs) do |sourcefile|
      objfile = sourcefile.gsub(CXXEXTPATTERN, ".o")
      out.printf("build %s: cc %s\n", objfile, sourcefile)
      out.printf("  depfile = %s.d\n", sourcefile)
      ofiles.push(objfile)
    end
    out.printf("build %s: link %s\n", exe, ofiles.join(" "))
  end
end
