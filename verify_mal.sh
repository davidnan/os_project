#!/bin/bash

if [ $# -ne 1 ]; then
    echo 1
    exit
fi

perm="$(ls -l $1 | cut -c 8-10)"

if [ "$perm" = "---" ]; then
  echo 1
  exit
fi

no_of_lines=$(wc -l "$1" | cut -d ' ' -f1)
no_of_words=$(wc -w "$1" | cut -d ' ' -f1)
no_of_chars=$(wc -c "$1" | cut -d ' ' -f1)

if [ "$no_of_lines" -lt 3 ] && [ "$no_of_words" -gt 1000 ] && [ "$no_of_chars" -gt 2000 ]; then
  echo 1
  exit
fi

dangerous_words=("corrupt" "dangerous" "risk" "attack" "malware" "malicious")

for i in "${dangerous_words[@]}"
do
  if [[ "$1" == *"$i"* ]]; then
    echo 1
    exit
  fi
done

echo 0
