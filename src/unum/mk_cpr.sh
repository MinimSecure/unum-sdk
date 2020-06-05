#!/bin/bash

if [ "$1" != "-s" ]; then
  echo "$0 <-s>"
  echo "Fix copyright for all .c, .h, .mk and Makefile files in the current folder and below."
  echo "Requires -s to execute. "
  exit 1
fi

PREFIX="(c)"
NAME="minim.co"

gen_cpr() {
        local f=$1
  first_date="X"
  last_date="X"
  while read -r line; do
    auth_date="$line"
    if [ "$last_date" == "X" ]; then
      first_date=$auth_date
          fi
    last_date=$auth_date
  done < <(git log --date=short --pretty=format:"%ad" "$f" | sed 's/-..-..//' | sort | uniq)

  if [ "$first_date" == "$last_date" ]; then
    echo "$PREFIX $last_date $NAME"
  else
    echo "$PREFIX ${first_date}-${last_date} $NAME"
  fi
}


while read -r fname; do
  VAL=`gen_cpr $fname`
  echo $fname $VAL
  sed -i -e "s/(c) .* minim.co/$VAL/" $fname
done < <(find . -type f \( -name '*.c' -or -name '*.h' -or -name 'Makefile' -or -name '*.mk' \) -exec grep -l -E '\(c\) .* minim.co' {} \;)

