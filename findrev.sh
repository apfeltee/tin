#!/bin/zsh

idx=1
maxidx=500

filename="$1"
pattern="$2"
echo "filename=$filename"
echo "pattern=$pattern"
if [[ -z "$pattern" ]] || [[ -z "$filename" ]]; then
  echo "need filename and pattern"
  exit 1
else
  tmpfile="tmp_findrev.txt"
  while [[ "$idx" < "$maxidx" ]]; do
    ref="HEAD~$idx"
    gitpath="${ref}:${filename}"
    if ! git show "$gitpath" > "$tmpfile"; then
      echo "exhausted max index of $maxidx. giving up"
      rm -f "$tmpfile"
      exit 1
    else
      if grep -P "$pattern" "$tmpfile"; then
        echo "found at <$gitpath>! file is $tmpfile."
        exit 0
      fi
    fi
    idx=((idx + 1))
  done
fi

