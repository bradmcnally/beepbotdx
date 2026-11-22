#include <unity.h>
#include <cstring>
#include "../../platform_zero/frame_diff.h"

void setUp() {}
void tearDown() {}

void test_initial_frame_copies_pixels_without_touching_stride_padding() {
    const uint16_t pixels[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint16_t previous[8] = {};
    uint8_t framebuffer[24];
    memset(framebuffer, 0x7f, sizeof(framebuffer));

    Zero::copyChangedFrame(pixels, previous, framebuffer, 12, 4, 2, false);

    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT16(pixels[i],
                                 reinterpret_cast<uint16_t*>(framebuffer)[i]);
        TEST_ASSERT_EQUAL_UINT16(pixels[i + 4],
                                 reinterpret_cast<uint16_t*>(framebuffer + 12)[i]);
    }
    for (int i = 8; i < 12; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x7f, framebuffer[i]);
        TEST_ASSERT_EQUAL_HEX8(0x7f, framebuffer[i + 12]);
    }
}

void test_focus_change_writes_only_changed_pixel() {
    uint16_t pixels[4] = {1, 2, 3, 4};
    uint16_t previous[4] = {};
    uint16_t framebuffer[4] = {};
    Zero::copyChangedFrame(pixels, previous,
                           reinterpret_cast<uint8_t*>(framebuffer), 8, 4, 1, false);

    pixels[1] = 9;
    for (auto& pixel : framebuffer) pixel = 0x7777;
    Zero::copyChangedFrame(pixels, previous,
                           reinterpret_cast<uint8_t*>(framebuffer), 8, 4, 1, true);

    TEST_ASSERT_EQUAL_UINT16(0x7777, framebuffer[0]);
    TEST_ASSERT_EQUAL_UINT16(9, framebuffer[1]);
    TEST_ASSERT_EQUAL_UINT16(0x7777, framebuffer[2]);
    TEST_ASSERT_EQUAL_UINT16(0x7777, framebuffer[3]);
    TEST_ASSERT_EQUAL_UINT16(9, previous[1]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_initial_frame_copies_pixels_without_touching_stride_padding);
    RUN_TEST(test_focus_change_writes_only_changed_pixel);
    return UNITY_END();
}
