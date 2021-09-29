#!/bin/sh

checkreturn(){
	if [ $? -ne 0 ]
	then
		echo error $1
		exit 1;
	fi
}
OUTDIR=${PWD}
if [ $# -eq 1 ]
then
	if test -d $1
	then
		OUTDIR=$1
	else
		echo error outdir $1
		exit 1
	fi
fi


for dir in *
do
	if test -d $dir
	then
		cd $dir
		checkreturn $dir
		make
		checkreturn $dir
		echo $1
		cp sensor_${dir}_t31.ko $OUTDIR
		checkreturn $dir
		cd ../
		checkreturn $dir
	fi
done
