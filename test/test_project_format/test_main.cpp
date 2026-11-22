#include <unity.h>
#include <vector>
#include <cstring>
#include <cstddef>
#include "core/project_format.h"

class Input : public Wav::Stream {
public:
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    size_t read(void* data, size_t size) override {
        size_t available = bytes.size() - pos;
        if (size > available) size = available;
        if (size) memcpy(data, bytes.data() + pos, size);
        pos += size;
        return size;
    }
    size_t write(const void*, size_t) override { return 0; }
    uint32_t tell() override { return pos; }
    bool seek(uint32_t p) override { if (p > bytes.size()) return false; pos = p; return true; }
};
void setUp() {}
void tearDown() {}
void test_current_header_roundtrip() {
    Project project{}; project.bpm = 137; project.bitDepth = BIT_DEPTH_8;
    strcpy(project.name, "BEAT"); project.patterns[0].steps[0] = 42;
    project.sounds[0].occupied = true; project.sounds[0].bitDepth = BIT_DEPTH_8;
    project.sounds[0].fx.enabled[0] = true; project.sounds[0].fx.value[0] = 55;
    ProjectHeader header; ProjectFormat::encode(project, header);
    Input in; auto raw = reinterpret_cast<uint8_t*>(&header);
    in.bytes.assign(raw, raw + sizeof(header));
    ProjectHeader decoded; TEST_ASSERT_TRUE(ProjectFormat::read(in, in.bytes.size(), decoded));
    Project restored{}; ProjectFormat::apply(decoded, restored);
    ProjectFormat::applySoundFx(decoded, 0, restored.sounds[0]);
    TEST_ASSERT_EQUAL(137, restored.bpm); TEST_ASSERT_EQUAL_STRING("BEAT", restored.name);
    TEST_ASSERT_EQUAL(42, restored.patterns[0].steps[0]);
    TEST_ASSERT_EQUAL(BIT_DEPTH_8, decoded.soundBitDepth[0]);
    TEST_ASSERT_TRUE(restored.sounds[0].fx.enabled[0]); TEST_ASSERT_EQUAL(55, restored.sounds[0].fx.value[0]);
}
void test_legacy_header_without_fx() {
    ProjectHeader header{}; header.magic = PROJECT_MAGIC; header.version = 1;
    header.bpm = 111; header.patterns[0].steps[0] = 7; header.song[0] = 3;
    size_t offset = offsetof(ProjectHeader, fxValues);
    size_t fxSize = sizeof(header.fxValues) + sizeof(header.fxEnabled);
    size_t end = offsetof(ProjectHeader, bitDepth);
    auto raw = reinterpret_cast<uint8_t*>(&header);
    Input in; in.bytes.insert(in.bytes.end(), raw, raw + offset);
    in.bytes.insert(in.bytes.end(), raw + offset + fxSize, raw + end);
    ProjectHeader decoded; TEST_ASSERT_TRUE(ProjectFormat::read(in, in.bytes.size(), decoded));
    TEST_ASSERT_EQUAL(111, decoded.bpm); TEST_ASSERT_EQUAL(7, decoded.patterns[0].steps[0]);
    TEST_ASSERT_EQUAL(3, decoded.song[0]);
    SoundSlot sound{}; ProjectFormat::applySoundFx(decoded, 0, sound);
    SlotFx defaults; SlotFxOps::defaults(defaults);
    TEST_ASSERT_EQUAL_MEMORY(&defaults, &sound.fx, sizeof(defaults));
}
void test_pre_bit_depth_header() {
    ProjectHeader header{}; header.magic = PROJECT_MAGIC; header.version = 3;
    header.fxValues[0][0] = 63; header.patterns[0].steps[1] = 8;
    Input in; auto raw = reinterpret_cast<uint8_t*>(&header);
    in.bytes.assign(raw, raw + offsetof(ProjectHeader, bitDepth));
    ProjectHeader decoded; TEST_ASSERT_TRUE(ProjectFormat::read(in, in.bytes.size(), decoded));
    TEST_ASSERT_EQUAL(63, decoded.fxValues[0][0]); TEST_ASSERT_EQUAL(8, decoded.patterns[0].steps[1]);
    TEST_ASSERT_EQUAL(BIT_DEPTH_16, decoded.bitDepth);
}
void test_invalid_and_truncated_headers() {
    Input in; in.bytes.resize(4);
    ProjectHeader decoded; TEST_ASSERT_FALSE(ProjectFormat::read(in, in.bytes.size(), decoded));
    ProjectHeader header{}; header.magic = PROJECT_MAGIC; header.version = PROJECT_VERSION + 1;
    auto raw = reinterpret_cast<uint8_t*>(&header); in.bytes.assign(raw, raw + sizeof(header)); in.pos = 0;
    TEST_ASSERT_FALSE(ProjectFormat::read(in, in.bytes.size(), decoded));
}
void test_normalize_toggle_roundtrips_without_changing_fx_or_pcm() {
    Project project{};
    int16_t samples[] = {1000, -1000};
    auto& slot = project.sounds[0];
    SoundSlotOps::init(slot);
    slot.samples = samples; slot.length = 2; slot.occupied = true;
    slot.normalize = true; slot.level = 75;
    ProjectHeader header{};
    ProjectFormat::encode(project, header);
    TEST_ASSERT_BITS_HIGH(0x10, header.fxEnabled[0]);
    SoundSlot restored{}; SoundSlotOps::init(restored);
    restored.samples = samples; restored.length = 2;
    ProjectFormat::applySoundFx(header, 0, restored);
    TEST_ASSERT_TRUE(restored.normalize);
    TEST_ASSERT_EQUAL_INT16(32767, SoundSlotOps::getPlaybackSample(restored, 0));
    TEST_ASSERT_EQUAL_INT16(1000, samples[0]);
    for (int i = 0; i < NUM_FX; ++i) {
        TEST_ASSERT_EQUAL(slot.fx.enabled[i], restored.fx.enabled[i]);
        TEST_ASSERT_EQUAL(slot.fx.value[i], restored.fx.value[i]);
    }
    header.fxEnabled[0] &= ~0x10;
    ProjectFormat::applySoundFx(header, 0, restored);
    TEST_ASSERT_FALSE(restored.normalize);
    TEST_ASSERT_EQUAL_INT16(1000, SoundSlotOps::getPlaybackSample(restored, 0));
}

int main() { UNITY_BEGIN(); RUN_TEST(test_normalize_toggle_roundtrips_without_changing_fx_or_pcm); RUN_TEST(test_current_header_roundtrip); RUN_TEST(test_legacy_header_without_fx); RUN_TEST(test_pre_bit_depth_header); RUN_TEST(test_invalid_and_truncated_headers); return UNITY_END(); }
