#! /bin/sh

sed '/<!-- FUNCTIONS -->/q' $1

for f in `ls man/*.3 | sort`
do
	# Extract list of functions.
	functions=`sed -n '/SYNOPSIS/,/DESCRIPTION/p' $f | grep -e '^\.Fo' -e '^\.Fn' | cut -d " " -f 2 | sort -u`
	# If no functions, don't do anything here.
	if [ -z "$functions" ]
	then
		continue
	fi

	desc=`grep -e '^\.Nd' $f | sed 's/^[^ ]* //'`

	echo "<dt><a href=\"`basename $f`.html\">`basename $f .3`(3)</a>: $desc</dt>"
	echo "<dd>"
	echo "<ul>"

	# Extract description of file.
	for ff in $functions
	do
		echo "<li>$ff</li>"
	done

	echo "</ul>"
	echo "</dd>"
done

sed '1,/<!-- FUNCTIONS -->/ d' $1
