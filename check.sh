#!/bin/zsh

function do_actual_check
{
  local infile="$1"
  local stem="${infile%.*}"
  local objfile="${stem}.o"
  local depfile="${stem}.d"
  cls
  rm -f "$objfile" "$depfile"
  make "$objfile"
}

if [[ -n "$1" ]]; then
  for file in "$@"; do
    do_actual_check "$file"
  done
else
  echo "usage: check.sh <file> (file can be a source file, or object file)"
  exit 1
fi


