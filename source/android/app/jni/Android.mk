LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main


#LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES :=main.c aEvent.c


LOCAL_LDLIBS := -lEGL -lGLESv2 -llog


include $(BUILD_SHARED_LIBRARY)
