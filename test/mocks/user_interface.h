#pragma once
// Mock SDK function
// system_get_free_heap_size
extern "C" {
    inline uint32_t system_get_free_heap_size() { return 40000; }
}
