#!/bin/bash
home_dir=$HOME
regex="("
pipe="|"
archive_name="$2"

mkdir $1 2>/dev/null

if [ "$#" -ge 3 ]
then
	regex="$regex$3"
	for i in "${@:4}"; do regex="$regex$pipe$i"; done 
	regex="$regex)"
	files=$(find 2>/dev/null $home_dir -type f -not -path "$1/*" -regextype posix-extended -regex "^.*\.$regex\$")
	for file in $files
	do
		file_name=$(echo $file | grep -o "[^/]*$")
		num=1
    		while true
    		do
			if [ -f "$1/($num)$file_name" ]
			then
				num=$(($num+1))
			else
				cp 2>/dev/null $file "$1/($num)$file_name"
				break
			fi
		done
	done
	cd $1; tar -czf "$home_dir/$archive_name.tar.gz" .
fi
echo "done"
