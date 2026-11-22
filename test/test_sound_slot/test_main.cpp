#include <unity.h>
#include "core/sound_slot.h"
#include "config.h"

static SoundSlot slots[3];
static int mutationCalls;
static int16_t* observedPointer;
static int16_t observedSample;
static void observeMutation() {
    mutationCalls++;
    observedPointer = slots[1].samples;
    observedSample = slots[1].samples ? SoundSlotOps::getSample(slots[1], 0) : 0;
}

void setUp() {
    SoundSlotOps::setBeforeArenaMutation(nullptr);
    for (SoundSlot& slot : slots) {
        SoundSlotOps::free(slot);
        SoundSlotOps::init(slot);
    }
}

void tearDown() {
    SoundSlotOps::setBeforeArenaMutation(nullptr);
    for (SoundSlot& slot : slots) {
        SoundSlotOps::free(slot);
    }
}

void test_allocations_share_one_budget() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 1000));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 2000));
    TEST_ASSERT_EQUAL_UINT32(6000, SoundSlotOps::usedBytes());
    TEST_ASSERT_EQUAL_UINT32(
        SAMPLE_ARENA_BYTES - 6000, SoundSlotOps::freeBytes());
}

void test_free_compacts_and_updates_later_pointers() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 4));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 4));
    slots[1].samples[0] = 1234;
    int16_t* oldPointer = slots[1].samples;

    SoundSlotOps::free(slots[0]);

    TEST_ASSERT_NOT_EQUAL(oldPointer, slots[1].samples);
    TEST_ASSERT_EQUAL_INT16(1234, slots[1].samples[0]);
    TEST_ASSERT_EQUAL_UINT32(8, SoundSlotOps::usedBytes());
}

void test_shrink_compacts_without_a_second_allocation() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 100));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 10));
    slots[0].length = 25;
    slots[1].samples[0] = -4321;

    TEST_ASSERT_TRUE(SoundSlotOps::shrinkToFit(slots[0]));

    TEST_ASSERT_EQUAL_UINT32(70, SoundSlotOps::usedBytes());
    TEST_ASSERT_EQUAL_INT16(-4321, slots[1].samples[0]);
}

void test_failed_replacement_preserves_existing_sample() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 10));
    slots[0].samples[0] = 777;
    int16_t* oldPointer = slots[0].samples;

    TEST_ASSERT_FALSE(
        SoundSlotOps::allocate(slots[0], SAMPLE_ARENA_BYTES));

    TEST_ASSERT_EQUAL_PTR(oldPointer, slots[0].samples);
    TEST_ASSERT_EQUAL_INT16(777, slots[0].samples[0]);
}

void test_available_samples_includes_replaced_slot() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 100));
    TEST_ASSERT_EQUAL_UINT32(
        SAMPLE_ARENA_BYTES / sizeof(int16_t),
        SoundSlotOps::availableSamples(BIT_DEPTH_16, &slots[0]));
}

void test_8bit_samples_use_half_space_and_round_trip() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 101, BIT_DEPTH_8));
    SoundSlotOps::setSample(slots[0], 0, 16384);
    SoundSlotOps::setSample(slots[0], 1, -16384);

    TEST_ASSERT_EQUAL_UINT32(102, SoundSlotOps::usedBytes());
    TEST_ASSERT_EQUAL_INT16(16384, SoundSlotOps::getSample(slots[0], 0));
    TEST_ASSERT_EQUAL_INT16(-16384, SoundSlotOps::getSample(slots[0], 1));
}

void test_8bit_extremes_expand_without_overflow() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 2, BIT_DEPTH_8));
    ((int8_t*)slots[0].samples)[0] = -128;
    ((int8_t*)slots[0].samples)[1] = 127;

    TEST_ASSERT_EQUAL_INT16(-32768, SoundSlotOps::getSample(slots[0], 0));
    TEST_ASSERT_EQUAL_INT16(32512, SoundSlotOps::getSample(slots[0], 1));
}

void test_16bit_allocation_after_odd_8bit_sample_is_aligned() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 1, BIT_DEPTH_8));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 1, BIT_DEPTH_16));

    TEST_ASSERT_EQUAL_UINT32(
        0, (uintptr_t)slots[1].samples % alignof(int16_t));
}

void test_shrinking_8bit_slot_compacts_mixed_format_data() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 100, BIT_DEPTH_8));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 4, BIT_DEPTH_16));
    SoundSlotOps::setSample(slots[1], 0, 22222);
    slots[0].length = 25;

    TEST_ASSERT_TRUE(SoundSlotOps::shrinkToFit(slots[0]));

    TEST_ASSERT_EQUAL_UINT32(34, SoundSlotOps::usedBytes());
    TEST_ASSERT_EQUAL_INT16(22222, SoundSlotOps::getSample(slots[1], 0));
    TEST_ASSERT_EQUAL_UINT32(
        0, (uintptr_t)slots[1].samples % alignof(int16_t));
}

void test_mutations_notify_before_compaction() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 8));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[1], 8));
    SoundSlotOps::setSample(slots[1], 0, 1234);
    int16_t* original = slots[1].samples;
    mutationCalls = 0;
    SoundSlotOps::setBeforeArenaMutation(observeMutation);
    SoundSlotOps::free(slots[0]);
    TEST_ASSERT_EQUAL(1, mutationCalls);
    TEST_ASSERT_EQUAL_PTR(original, observedPointer);
    TEST_ASSERT_EQUAL_INT16(1234, observedSample);
    TEST_ASSERT_NOT_EQUAL(original, slots[1].samples);
}

void test_no_notification_for_failed_or_nonmoving_operations() {
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slots[0], 8));
    slots[0].length = slots[0].allocLength;
    mutationCalls = 0;
    SoundSlotOps::setBeforeArenaMutation(observeMutation);
    TEST_ASSERT_TRUE(SoundSlotOps::shrinkToFit(slots[0]));
    TEST_ASSERT_FALSE(SoundSlotOps::allocate(slots[0], SAMPLE_ARENA_BYTES));
    SoundSlotOps::free(slots[1]);
    TEST_ASSERT_EQUAL(0, mutationCalls);
    slots[0].length = 4;
    TEST_ASSERT_TRUE(SoundSlotOps::shrinkToFit(slots[0]));
    TEST_ASSERT_EQUAL(1, mutationCalls);
}

void test_normalization_is_reversible_and_preserves_level_and_pcm() {
    auto& slot = slots[0];
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slot, 3));
    slot.length = 3;
    slot.level = 65;
    slot.samples[0] = 1000; slot.samples[1] = -2000; slot.samples[2] = 0;
    slot.normalize = true;
    SoundSlotOps::updateNormalizationGain(slot);
    TEST_ASSERT_EQUAL_INT16(16384, SoundSlotOps::getPlaybackSample(slot, 0));
    TEST_ASSERT_EQUAL_INT16(-32768, SoundSlotOps::getPlaybackSample(slot, 1));
    TEST_ASSERT_EQUAL_INT16(1000, SoundSlotOps::getSample(slot, 0));
    TEST_ASSERT_EQUAL_INT16(-2000, SoundSlotOps::getSample(slot, 1));
    TEST_ASSERT_EQUAL(65, slot.level);
    slot.normalize = false;
    TEST_ASSERT_EQUAL_INT16(-2000, SoundSlotOps::getPlaybackSample(slot, 1));
}

void test_normalization_handles_silence_full_scale_and_8bit() {
    auto& slot = slots[0];
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slot, 2));
    slot.length = 2; slot.normalize = true;
    SoundSlotOps::updateNormalizationGain(slot);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, slot.normalizationGain);
    TEST_ASSERT_EQUAL_INT16(0, SoundSlotOps::getPlaybackSample(slot, 0));
    slot.samples[0] = 32767; slot.samples[1] = -32768;
    SoundSlotOps::updateNormalizationGain(slot);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, slot.normalizationGain);
    TEST_ASSERT_EQUAL_INT16(32767, SoundSlotOps::getPlaybackSample(slot, 0));
    TEST_ASSERT_EQUAL_INT16(-32768, SoundSlotOps::getPlaybackSample(slot, 1));
    TEST_ASSERT_TRUE(SoundSlotOps::allocate(slot, 2, BIT_DEPTH_8));
    TEST_ASSERT_FALSE(slot.normalize);
    slot.length = 2; slot.normalize = true;
    SoundSlotOps::setSample(slot, 0, 256); SoundSlotOps::setSample(slot, 1, -256);
    SoundSlotOps::updateNormalizationGain(slot);
    TEST_ASSERT_EQUAL_INT16(32767, SoundSlotOps::getPlaybackSample(slot, 0));
    TEST_ASSERT_EQUAL_INT16(-32767, SoundSlotOps::getPlaybackSample(slot, 1));
    TEST_ASSERT_EQUAL_INT16(256, SoundSlotOps::getSample(slot, 0));
    SoundSlotOps::free(slot);
    TEST_ASSERT_FALSE(slot.normalize);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_normalization_is_reversible_and_preserves_level_and_pcm);
    RUN_TEST(test_normalization_handles_silence_full_scale_and_8bit);
    RUN_TEST(test_mutations_notify_before_compaction);
    RUN_TEST(test_no_notification_for_failed_or_nonmoving_operations);
    RUN_TEST(test_allocations_share_one_budget);
    RUN_TEST(test_free_compacts_and_updates_later_pointers);
    RUN_TEST(test_shrink_compacts_without_a_second_allocation);
    RUN_TEST(test_failed_replacement_preserves_existing_sample);
    RUN_TEST(test_available_samples_includes_replaced_slot);
    RUN_TEST(test_8bit_samples_use_half_space_and_round_trip);
    RUN_TEST(test_8bit_extremes_expand_without_overflow);
    RUN_TEST(test_16bit_allocation_after_odd_8bit_sample_is_aligned);
    RUN_TEST(test_shrinking_8bit_slot_compacts_mixed_format_data);
    return UNITY_END();
}
