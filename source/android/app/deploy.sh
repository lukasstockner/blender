#!/bin/sh
#if !(ndk-build) then
#echo "!!!\tStopped. Compilation failed."
#exit
#fi
ant clean
ant debug install
adb shell am start -a android.intent.action.MAIN -n org.blender.play/.GhostActivity

adb shell am start -n org.blender.play/.makesActivity --es exec /sdcard/libmakesrna.so --es par1  /sdcard/rna/ --es par2  /sdcard/rna/ 