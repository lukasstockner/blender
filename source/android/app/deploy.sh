#!/bin/sh
if !(ndk-build) then
echo "!!!\tStopped. Compilation failed."
exit
fi
ant clean
ant debug install
adb shell am start -a android.intent.action.MAIN -n org.blender.play/.GhostActivity
