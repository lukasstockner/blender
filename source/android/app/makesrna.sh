if [ "$1" = "" ]
then
echo "1st argument must be makesrna.so library"
exit
fi

if [ "$2" = "" ]
then
echo "2nd argument must be a path to rna headers"
exit
fi


adb shell < createnb.txt
adb push $1 /sdcard/nucleusbridge/libmakesrna.so
adb shell mkdir -p  /sdcard/nucleusbridge/rna

while [ "$(adb shell ps | grep org.blender.play)" ]; 
  do sleep 1; echo "Waiting DNA to finish"; done

adb shell am start -n org.blender.play/.makesActivity --es exec /sdcard/nucleusbridge/libmakesrna.so --es par1  /sdcard/nucleusbridge/rna/
while [ "$(adb shell ps | grep org.blender.play)" ]; 
  do sleep 1; done
adb pull /sdcard/nucleusbridge/rna/ $2
adb  shell rm -r /sdcard/nucleusbridge/

echo "RNA generation is complete"