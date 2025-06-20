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

// Shared memory structure for snapshots
struct SnapshotSHM {
    std::atomic<uint64_t> last_tob_seq{0};
    std::atomic<uint64_t> last_fod_seq{0};
    std::atomic<uint64_t> last_consumer_read{0};
    std::atomic<uint32_t> active_consumers{0};
    SnapshotTOB latest_tob;
    SnapshotFOD latest_fod;
};

// Create snapshot shared memory
inline SnapshotSHM* create_snapshot_shm(const char* name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for snapshot: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    if (ftruncate(fd, sizeof(SnapshotSHM)) == -1) {
        std::cerr << "ftruncate failed for snapshot: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    SnapshotSHM* ret = (SnapshotSHM*)mmap(0, sizeof(SnapshotSHM), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for snapshot: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Open existing snapshot shared memory
inline SnapshotSHM* open_snapshot_shm(const char* name) {
    int fd = shm_open(name, O_RDONLY, 0666);
    if (fd == -1) {
        std::cerr << "shm_open failed for snapshot read: " << strerror(errno) << std::endl;
        return nullptr;
    }
    
    SnapshotSHM* ret = (SnapshotSHM*)mmap(0, sizeof(SnapshotSHM), PROT_READ, MAP_SHARED, fd, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "mmap failed for snapshot read: " << strerror(errno) << std::endl;
        close(fd);
        return nullptr;
    }
    
    close(fd);
    return ret;
}

// Consumer class for reading snapshots
class SnapshotConsumer {
public:
    SnapshotConsumer(const char* shm_name) : shm(open_snapshot_shm(shm_name)) {
        if (shm) {
            shm->active_consumers.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    ~SnapshotConsumer() {
        if (shm) {
            shm->active_consumers.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    
    // Check if new TOB snapshot is available
    bool has_new_tob(uint64_t& last_seen_seq) const {
        if (!shm) return false;
        uint64_t current_seq = shm->last_tob_seq.load(std::memory_order_acquire);
        if (current_seq > last_seen_seq) {
            last_seen_seq = current_seq;
            return true;
        }
        return false;
    }
    
    // Check if new FOD snapshot is available
    bool has_new_fod(uint64_t& last_seen_seq) const {
        if (!shm) return false;
        uint64_t current_seq = shm->last_fod_seq.load(std::memory_order_acquire);
        if (current_seq > last_seen_seq) {
            last_seen_seq = current_seq;
            return true;
        }
        return false;
    }
    
    // Get latest TOB snapshot
    const SnapshotTOB* get_latest_tob() const {
        return shm ? &shm->latest_tob : nullptr;
    }
    
    // Get latest FOD snapshot
    const SnapshotFOD* get_latest_fod() const {
        return shm ? &shm->latest_fod : nullptr;
    }
    
    // Update last read timestamp
    void update_read_timestamp() {
        if (shm) {
            shm->last_consumer_read.store(get_ns_since_epoch(), std::memory_order_relaxed);
        }
    }
    
    bool is_valid() const { return shm != nullptr; }
    
private:
    SnapshotSHM* shm;
}; 