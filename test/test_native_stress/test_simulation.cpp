#include <unity.h>
#include <iostream>
#include <random>
#include <vector>

// Define Testing Mode
#define NATIVE_TEST 1

#include "NativeTestHelper.h"

// Include Application Logic
// IMPORTANT: We include .cpp files to link logic without complex build systems
// In a real repo this would be done via proper linking
#include "CacheManager.cpp"
// We map required externs here

// ============================================================================
// SIMULATION HELPERS
// ============================================================================
std::vector<uint8_t> generate_valid_payload(uint32_t id) {
    std::string payload = "{\"sensor\": \"temp\", \"val\": 25.5, \"id\": " + std::to_string(id) + "}";
    return std::vector<uint8_t>(payload.begin(), payload.end());
}

// ============================================================================
// TEST: PEAK LOAD SIMULATION
// ============================================================================
void test_peak_load_simulation(void) {
    printf("\n=== PEAK LOAD SIMULATION (10,000 CYCLES) ===\n");

    LittleFS.format();
    CacheManager cache;
    cache.init();
    
    // 1. SETUP: Fill Cache to 50%
    printf("[SIM] Pre-filling cache...\n");
    for(int i=0; i<50; i++) {
        auto data = generate_valid_payload(i);
        bool res = cache.write((const char*)data.data(), data.size());
        TEST_ASSERT_TRUE(res);
    }
    
    // 2. SIMULATION LOOP
    // We simulate: High Frequency Writes + Random Corruption + High Frequency Reads
    // This tests the "Ring" buffer wrap-around and the "Deep Recovery" logic.
    
    int cycles = 10000;
    int corruptions_injected = 0;
    int recoveries = 0;
    
    std::mt19937 rng(12345); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> op_dist(0, 100);
    
    for(int i=0; i<cycles; i++) {
        // A. WRITE (Sensor Sample)
        // -------------------------
        auto payload = generate_valid_payload(1000 + i);
        bool write_ok = cache.write((const char*)payload.data(), payload.size());
        
        if (!write_ok) {
            // If write fails, it might be full or corrupt. 
            // In a real device, we simply skip the sample.
            // But here we want to assert we are not stuck.
            // printf("[SIM] Write Skipped (Cache Full?)\n");
        } 
        
        // B. INJECT FAULT (Cosmic Ray / Flash failure)
        // --------------------------------------------
        if (op_dist(rng) < 5) { // 5% chance of bit rot per cycle
            // We corrupt the FILE directly via MockFS
            // We target an area near the 'head' or 'tail' to likely be hit
            // But since we don't know exact physical address easily, we just corrupt random byte
            FSInfo info;
            LittleFS.info(info);
            // Corrupt minimal area
             if (info.usedBytes > 100) {
                 LittleFS.corruptByte(Paths::CACHE_FILE, i % info.usedBytes);
                 corruptions_injected++;
             }
        }
        
        // C. READ/UPLOAD (Consumer)
        // -------------------------
        // Simulate Attempt to Read
        char buf[512];
        size_t len = 0;
        CacheReadError err = cache.read_one(buf, sizeof(buf), len);
        
        if (err == CacheReadError::NONE) {
            // Success! Pop it.
            bool pop_ok = cache.pop_one();
            TEST_ASSERT_TRUE(pop_ok);
        } else if (err == CacheReadError::CORRUPT_DATA) {
            // Deep Recovery worked! It identified corruption and skipped it.
            // Or it asked us to retry.
            recoveries++;
            // Retry immediately (ApiClient logic)
             err = cache.read_one(buf, sizeof(buf), len);
             if (err == CacheReadError::NONE) {
                 cache.pop_one(); // Success on retry
             }
        } else if (err == CacheReadError::CACHE_EMPTY) {
            // Fine
        }
    }
    
    printf("[SIM] Completed %d cycles.\n", cycles);
    printf("[SIM] Corruptions Injected: %d\n", corruptions_injected);
    printf("[SIM] Recoveries Triggered: %d\n", recoveries);
    
    // 3. FINAL INTEGRITY CHECK
    // Assert that the file is not deleted (Magic Byte still exists at Start)
    // Unless we cycled through it? No, verify header is valid.
    
    File f = LittleFS.open(Paths::CACHE_FILE, "r");
    TEST_ASSERT_TRUE(f);
    CacheHeader h;
    f.read((uint8_t*)&h, sizeof(h));
    
    TEST_ASSERT_EQUAL_HEX32(0xDEADBEEF, h.magic); // Magic should persist (Reset Nuke removed)
    printf("[SIM] Cache Header Alive. Magic: 0x%08X\n", h.magic);
    
    // Assert Head/Tail are within bounds (Invariant Check)
    TEST_ASSERT_LESS_THAN(MAX_CACHE_DATA_SIZE + CACHE_DATA_START, h.head);
    TEST_ASSERT_LESS_THAN(MAX_CACHE_DATA_SIZE + CACHE_DATA_START, h.tail);
    
    printf("=== SIMULATION PASS ===\n");
}

