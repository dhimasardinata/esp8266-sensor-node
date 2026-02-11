#pragma once
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>

// ============================================================================
// Mock File Class
// ============================================================================
class File {
public:
    File() : m_valid(false), m_position(0), m_name("") {}
    
    // Construct valid file
    File(std::string name, std::shared_ptr<std::vector<uint8_t>> content) 
        : m_valid(true), m_position(0), m_name(name), m_content(content) {}

    operator bool() const { return m_valid; }

    void close() { m_valid = false; }
    
    size_t write(const uint8_t* buf, size_t size) {
        if (!m_valid) return 0;
        // Expand if needed
        if (m_position + size > m_content->size()) {
            m_content->resize(m_position + size);
        }
        std::copy(buf, buf + size, m_content->begin() + m_position);
        m_position += size;
        return size;
    }
    
    size_t write(uint8_t c) {
        return write(&c, 1);
    }
    
    size_t read(uint8_t* buf, size_t size) {
        if (!m_valid) return 0;
        if (m_position >= m_content->size()) return 0;
        
        size_t available = m_content->size() - m_position;
        size_t toRead = (size < available) ? size : available;
        
        std::copy(m_content->begin() + m_position, m_content->begin() + m_position + toRead, buf);
        m_position += toRead;
        return toRead;
    }
    
    int read() {
        if (!m_valid || m_position >= m_content->size()) return -1;
        return (*m_content)[m_position++];
    }
    
    bool seek(uint32_t pos, int mode = 0) { // mode: 0=Set, 1=Cur, 2=End
        if (!m_valid) return false;
        if (mode == 0) m_position = pos;
        else if (mode == 1) m_position += pos;
        else if (mode == 2) m_position = m_content->size() - pos;
        
        // Clamp logic often separate, but for mock, simple extend is enough or clamp?
        // Real LittleFS expands on write-past-end, but read-past-end is limited.
        return true;
    }
    
    size_t position() const { return m_position; }
    size_t size() const { return m_valid ? m_content->size() : 0; }
    void flush() {} // No-op for RAM disk

private:
    bool m_valid;
    size_t m_position;
    std::string m_name;
    std::shared_ptr<std::vector<uint8_t>> m_content;
};

// ============================================================================
// Mock Filesystem Class
// ============================================================================
struct FSInfo {
    size_t totalBytes;
    size_t usedBytes;
    size_t blockSize;
    size_t pageSize;
    size_t maxOpenFiles;
    size_t maxPathLength;
};

class MockFS {
public:
    MockFS(size_t size = 100 * 1024) : m_totalSize(size) {} // Default 100KB for cache

    bool begin() { return true; }
    void end() {}
    
    bool format() { 
        m_files.clear(); 
        return true; 
    }
    
    bool exists(const char* path) {
        return m_files.find(path) != m_files.end();
    }
    
    File open(const char* path, const char* mode) {
        // Mode logic: "w" truncates, "r" reads, "a" appends, "+" updates
        std::string sPath = path;
        
        if (std::string(mode).find("w") != std::string::npos) {
            // Create/Truncate
            m_files[sPath] = std::make_shared<std::vector<uint8_t>>();
        } else if (!exists(path)) {
            // Read/Update non-existent
            return File(); // Invalid
        }
        
        return File(sPath, m_files[sPath]);
    }
    
    bool remove(const char* path) {
        return m_files.erase(path) > 0;
    }

    bool rename(const char* from, const char* to) {
        if (!exists(from)) {
            return false;
        }
        m_files[to] = m_files[from];
        m_files.erase(from);
        return true;
    }
    
    // Test Helper: Inject Corruption
    void corruptByte(const char* path, size_t offset) {
        if (exists(path)) {
            auto& data = *m_files[path];
            if (offset < data.size()) {
                data[offset] ^= 0xFF; // Flip all bits
            }
        }
    }
    
    // Helper: Dump to console
    void dump(const char* path) {
         if (exists(path)) {
            auto& data = *m_files[path];
            printf("File %s (%zu bytes): ", path, data.size());
            for(size_t i=0; i<std::min((size_t)20, data.size()); i++) printf("%02X ", data[i]);
            printf("\n");
         }
    }
    
    bool info(FSInfo& info) {
        info.totalBytes = REDACTED
        info.usedBytes = 0;
        for (auto const& [name, content] : m_files) {
            info.usedBytes += content->size();
        }
        return true;
    }

private:
    size_t m_totalSize;
    std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> m_files;
};

inline MockFS LittleFS(200 * 1024); // Global instance
