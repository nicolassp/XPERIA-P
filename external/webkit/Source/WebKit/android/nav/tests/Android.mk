# Build the unit tests.
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# Build the unit tests.
test_src_files := \
    SelectText_test.cpp

shared_libraries := \
    libcutils \
    libutils \
    libstlport \
    libskia \
    libicuuc \
    libv8 \
    libchromium_net \
    libinput \
    libharfbuzz

static_libraries := \
    libgtest \
    libgtest_main \
    libwebcore

c_includes := \
    bionic \
    bionic/libstdc++/include \
    external/gtest/include \
    external/webkit/Source/JavaScriptCore \
    external/webkit/Source/JavaScriptCore/wtf \
    external/webkit/Source/JavaScriptCore/wtf/unicode \
    external/webkit/Source/JavaScriptCore/wtf/unicode/icu \
    external/webkit/Source/WebCore/icu \
    external/webkit/Source/WebCore/icu/unicode \
    external/icu4c/common \
    external/stlport/stlport \
    external/harfbuzz/contrib \
    external/harfbuzz/src

module_tags := eng tests

$(foreach file,$(test_src_files), \
    $(eval include $(CLEAR_VARS)) \
    $(eval LOCAL_SHARED_LIBRARIES := $(shared_libraries)) \
    $(eval LOCAL_STATIC_LIBRARIES := $(static_libraries)) \
    $(eval LOCAL_C_INCLUDES := $(c_includes)) \
    $(eval LOCAL_SRC_FILES := $(file)) \
    $(eval LOCAL_MODULE := $(notdir $(file:%.cpp=%))) \
    $(eval LOCAL_MODULE_TAGS := $(module_tags)) \
    $(eval include $(BUILD_EXECUTABLE)) \
)

# Build the manual test programs.
include $(call all-makefiles-under, $(LOCAL_PATH))