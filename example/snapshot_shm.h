#pragma once

#include <iostream>
#include <atomic>
#include <cstdint>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "structs.h"

// Forward declaration to avoid conflicts
uint64_t get_ns_since_epoch();

// Separate shared memory structures for TOB and FOD
struct TOBSHM {
    std::atomic<uint64_t> last_seq{0};
    std::atomic<uint32_t> active_consumers{0};
    SnapshotTOB latest_snapshot;
};

struct FODSHM {
    std::atomic<uint64_t> last_seq{0};
    std::atomic<uint32_t> active_consumers{0};
    SnapshotFOD latest_snapshot;
};

// Create TOB shared memory
inline TOBSHM* create_tob_shm(const char* name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for TOB: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    if (ftruncate(fd, sizeof(TOBSHM)) == -1) {
        std::cerr << "ftruncate failed for TOB: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    TOBSHM* ret = (TOBSHM*)mmap(0, sizeof(TOBSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for TOB: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Create FOD shared memory
inline FODSHM* create_fod_shm(const char* name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for FOD: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    if (ftruncate(fd, sizeof(FODSHM)) == -1) {
        std::cerr << "ftruncate failed for FOD: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    FODSHM* ret = (FODSHM*)mmap(0, sizeof(FODSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for FOD: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Open existing TOB shared memory
inline TOBSHM* open_tob_shm(const char* name) {
    int fd = shm_open(name, O_RDONLY, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for TOB read: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    TOBSHM* ret = (TOBSHM*)mmap(0, sizeof(TOBSHM), PROT_READ, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for TOB read: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Open existing FOD shared memory
inline FODSHM* open_fod_shm(const char* name) {
    int fd = shm_open(name, O_RDONLY, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for FOD read: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    FODSHM* ret = (FODSHM*)mmap(0, sizeof(FODSHM), PROT_READ, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for FOD read: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Open existing TOB shared memory for read-write access (for consumers)
inline TOBSHM* open_tob_shm_rw(const char* name) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for TOB read-write: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    TOBSHM* ret = (TOBSHM*)mmap(0, sizeof(TOBSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for TOB read-write: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Open existing FOD shared memory for read-write access (for consumers)
inline FODSHM* open_fod_shm_rw(const char* name) {
    int fd = shm_open(name, O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for FOD read-write: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    FODSHM* ret = (FODSHM*)mmap(0, sizeof(FODSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for FOD read-write: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Consumer class for reading TOB snapshots
class TOBConsumer {
public:
    TOBConsumer(const char* shm_name) : shm(open_tob_shm_rw(shm_name)) {
        if (shm) {
            shm->active_consumers.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    ~TOBConsumer() {
        if (shm) {
            shm->active_consumers.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    // Check if new TOB snapshot is available
    bool has_new_snapshot(uint64_t& last_seen_seq) const {
        if (!shm) return false;
        uint64_t current_seq = shm->last_seq.load(std::memory_order_acquire);
        if (current_seq > last_seen_seq) {
            last_seen_seq = current_seq;
            return true;
        }
        return false;
    }
    
    // Get latest TOB snapshot
    const SnapshotTOB* get_latest_snapshot() const {
        return shm ? &shm->latest_snapshot : nullptr;
    }
    
    bool is_valid() const { return shm != nullptr; }
    
private:
    TOBSHM* shm;
};

// Consumer class for reading FOD snapshots
class FODConsumer {
public:
    FODConsumer(const char* shm_name) : shm(open_fod_shm_rw(shm_name)) {
        if (shm) {
            shm->active_consumers.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    ~FODConsumer() {
        if (shm) {
            shm->active_consumers.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    // Check if new FOD snapshot is available
    bool has_new_snapshot(uint64_t& last_seen_seq) const {
        if (!shm) return false;
        uint64_t current_seq = shm->last_seq.load(std::memory_order_acquire);
        if (current_seq > last_seen_seq) {
            last_seen_seq = current_seq;
            return true;
        }
        return false;
    }
    
    // Get latest FOD snapshot
    const SnapshotFOD* get_latest_snapshot() const {
        return shm ? &shm->latest_snapshot : nullptr;
    }
    
    bool is_valid() const { return shm != nullptr; }
    
private:
    FODSHM* shm;
}; 