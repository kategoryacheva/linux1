#!/bin/bash
home_dir=$HOME
regex="("
pipe="|"
archive_name="$2"
if [ "$#" -ge 3 ]
then
	regex="$regex$3"
	for i in "${@:4}"; do regex="$regex$pipe$i"; done 
	regex="$regex)"
	find $home_dir -type f -not -path "$1/*" -regextype posix-extended -regex "^.*\.$regex\$" -exec cp -R {} "$1/" \;
	cd $1; tar -czf "$home_dir/$archive_name.tar.gz" .
fi
echo "done"
