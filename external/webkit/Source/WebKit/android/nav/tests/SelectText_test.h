/*
 * Copyright (C) 2012 Sony Mobile Communications AB.
 * All rights, including trade secret rights, reserved.
 */
#ifndef SelectText_test_h
#define SelectText_test_h

extern "C" {
#include "harfbuzz-unicode.h"
}

typedef unsigned short uint16_t;

namespace WebCore {

void ReverseBidi(uint16_t* chars, int len);

}

#endif
