/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 * All rights, including trade secret rights, reserved.
 */
#include "SelectText_test.h"

#include <gtest/gtest.h>
#include <cutils/log.h>

namespace WebCore {

bool arrayCompare(uint16_t first[], uint16_t second[], int size, int* index) {
    bool result = true;
    int i = *index;
    for (; i < 24; i++) {
        if (first[i] != second[i]) {
            result = false;
            break;
        }
    }
    *index = i;
    return result;
}

/*
 * Left-to-right Ascii text should not be modified by WebCore::ReverseBidi
 * The 'selectText' string mimics the input from the android::SelectText class and 'displayText'
 * is the expected text that can be displayed correctly.
 */
TEST(SelectText, LTRAsciiText) {
    std::wstring selectText (L"This is a text.");
    std::wstring displayText (L"This is a text.");

    ReverseBidi((uint16_t*)selectText.data(), selectText.size());

    ASSERT_STREQ(displayText.c_str(), selectText.c_str());
}

/*
 * Left-to-right non-Ascii text should not be modified by WebCore::ReverseBidi
 * The 'selectText' string mimics the input from the android::SelectText class and 'displayText'
 * is the expected text that can be displayed correctly.
 */
TEST(SelectText, LTRNonAsciiText) {
    std::wstring selectText (L"Det här är en text.");
    std::wstring displayText (L"Det här är en text.");

    ReverseBidi((uint16_t*)selectText.data(), selectText.size());

    ASSERT_STREQ(displayText.c_str(), selectText.c_str());
}

/*
 * Right-to-left text should be modified by WebCore::ReverseBidi. The word order should be
 * reversed.
 * The 'selectText' string mimics the input from the android::SelectText class and 'displayText'
 * is the expected text that can be displayed correctly.
 */
TEST(SelectText, RTLText) {
    // conversion using std::wstring does funny things with Arabic, lets use an array of unicode

    uint16_t selectText[] = {65165, 65247, 65228, 65256, 65166, 65253, ' ', 65175, 65220, 65248,
            65238, ' ', 65155, 65253, ' ', 65267, 65252, 65244, 65256, 65242, ' ', 65187, 65268,
            65178};

    // The displayed text looks like (might be reversed depending on your editor/viewer)
    // حيث يمكنك أن تطلق العنان
    uint16_t displayText[] = {65187, 65268, 65178, ' ',65267, 65252, 65244, 65256, 65242, ' ',
            65155, 65253, ' ', 65175, 65220, 65248, 65238, ' ', 65165, 65247, 65228, 65256, 65166,
            65253};

    ReverseBidi(selectText, 24);

    int index = 0;
    ASSERT_TRUE(arrayCompare(selectText, displayText, 24, &index))
            << "The texts differs on character index " << index << ". Actual: "
            << selectText[index] << " Expected: " << displayText[index];
}

}

