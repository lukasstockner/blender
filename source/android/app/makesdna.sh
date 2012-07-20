
if [ "$1" = "" ]
then
echo "1st argument must be dna.c output path"
exit
fi

if [ "$2" = "" ]
then
echo "1st argument must be dna.c output path"
exit
fi
if [ "$3" = "" ]
then
echo "2st argument must be a path to dna headers"
exit
fi


adb shell < createnb.txt
adb push $1 /sdcard/nucleusbridge/libmakesdna.so
adb push $3 /sdcard/nucleusbridge/dna
adb shell am start -n org.blender.play/.makesActivity --es exec /sdcard/nucleusbridge/libmakesdna.so --es par1  /sdcard/nucleusbridge/dna.c --es par2  /sdcard/nucleusbridge/dna/
while [ "$(adb shell ps | grep org.blender.play)" ]; 
  do sleep 1; done
adb pull /sdcard/nucleusbridge/dna.c $2
adb  shell rm -r /sdcard/nucleusbridge/

echo "DNA generation is complete"