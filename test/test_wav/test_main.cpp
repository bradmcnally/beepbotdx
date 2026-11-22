#include <unity.h>
#include <vector>
#include <cstring>
#include "core/wav.h"
#include "config.h"

class MemoryStream : public Wav::Stream {
public:
    std::vector<uint8_t> data;
    size_t pos = 0;
    size_t limit = SIZE_MAX;
    size_t read(void* out, size_t n) override {
        size_t count = pos < data.size() ? data.size() - pos : 0;
        if (count > n) count = n;
        if (count) memcpy(out, data.data() + pos, count);
        pos += count;
        return count;
    }
    size_t write(const void* in, size_t n) override {
        if (pos >= limit) return 0;
        if (n > limit - pos) n = limit - pos;
        if (data.size() < pos + n) data.resize(pos + n);
        memcpy(data.data() + pos, in, n);
        pos += n;
        return n;
    }
    uint32_t tell() override { return pos; }
    bool seek(uint32_t p) override { pos = p; return true; }
};
static SoundSlot input, output;
void setUp() { SoundSlotOps::init(input); SoundSlotOps::init(output); }
void tearDown() { SoundSlotOps::free(input); SoundSlotOps::free(output); }
void roundtrip(BitDepth depth) {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(input, 2, depth));
    input.length = 2; input.occupied = true;
    SoundSlotOps::setSample(input, 0, -32768);
    SoundSlotOps::setSample(input, 1, 16384);
    MemoryStream stream;
    TEST_ASSERT_TRUE(Wav::save(stream, input));
    stream.pos = 0;
    TEST_ASSERT_TRUE(Wav::load(stream, output, "test.wav", depth));
    TEST_ASSERT_EQUAL_UINT32(2, output.length);
    TEST_ASSERT_EQUAL_INT16(-32768, SoundSlotOps::getSample(output, 0));
    TEST_ASSERT_EQUAL_INT16(16384, SoundSlotOps::getSample(output, 1));
}
void test_roundtrip_8() { roundtrip(BIT_DEPTH_8); }
void test_roundtrip_16() { roundtrip(BIT_DEPTH_16); }
void test_truncated_data_fails() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(input, 2)); input.length = 2;
    MemoryStream stream; TEST_ASSERT_TRUE(Wav::save(stream, input));
    stream.data.pop_back(); stream.pos = 0;
    TEST_ASSERT_FALSE(Wav::load(stream, output, "broken.wav", BIT_DEPTH_16));
}
void test_stereo_downsampling() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(input, 4)); input.length = 4;
    MemoryStream stream; TEST_ASSERT_TRUE(Wav::save(stream, input));
    uint16_t channels = 2; uint32_t rate = SAMPLE_RATE * 2;
    memcpy(stream.data.data() + 22, &channels, 2);
    memcpy(stream.data.data() + 24, &rate, 4);
    int16_t frames[] = {1000, 3000, 2000, 4000};
    memcpy(stream.data.data() + 44, frames, sizeof(frames));
    stream.pos = 0;
    TEST_ASSERT_TRUE(Wav::load(stream, output, "stereo.wav", BIT_DEPTH_16));
    TEST_ASSERT_EQUAL_UINT32(1, output.length);
    TEST_ASSERT_EQUAL_INT16(2000, SoundSlotOps::getSample(output, 0));
}
void test_write_failure_propagates() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(input, 2)); input.length = 2;
    MemoryStream stream; stream.limit = 20;
    TEST_ASSERT_FALSE(Wav::save(stream, input));
}
int main() { UNITY_BEGIN(); RUN_TEST(test_roundtrip_8); RUN_TEST(test_roundtrip_16); RUN_TEST(test_truncated_data_fails); RUN_TEST(test_stereo_downsampling); RUN_TEST(test_write_failure_propagates); return UNITY_END(); }
