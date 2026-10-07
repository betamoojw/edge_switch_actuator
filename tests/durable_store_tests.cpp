#include "device/DurableStore.h"
#include <FS.h>
#include <cassert>
#include <iostream>

// Reproduce a fragmented heap: no JSON allocation may require a 32 KB block.
struct BoundedAllocator: ArduinoJson::Allocator
{
    size_t limit = 8192;

    void *allocate(size_t n) override
    {
        return n <= limit ? malloc(n) : nullptr;
    }

    void deallocate(void *p) override
    {
        free(p);
    }

    void *reallocate(void *p, size_t n) override
    {
        return n <= limit ? realloc(p, n) : nullptr;
    }
};

int main()
{
    FS fs;
    BoundedAllocator allocator;
    DurableStore store(fs, &allocator);
    const String first = "{\"image\":\"" + String(16384, 'a') + "\",\"revision\":1,\"owner\":\"ets\"}";
    const String second = "{\"image\":\"" + String(16384, 'b') + "\",\"revision\":2,\"owner\":\"web\"}";
    String actual;
    assert(store.write("/config/knx", first));
    assert(store.write("/config/knx", second));
    assert(store.write("/config/knx", first));
    assert(store.read("/config/knx", actual) && actual == first);
    const auto beforeAllocationFailure = fs.files;
    allocator.limit = 0;
    assert(!store.write("/config/knx", second));
    assert(fs.files == beforeAllocationFailure);
    allocator.limit = 8192;
    fs.writeLimit = 100;
    assert(!store.write("/config/knx", second));
    assert(String(store.lastError()) == "storage slot write incomplete");
    assert(store.read("/config/knx", actual) && actual == first);
    fs.writeLimit = SIZE_MAX;
    fs.failOpen = true;
    assert(!store.write("/config/knx", second));
    assert(String(store.lastError()) == "storage slot open failed");
    fs.failOpen = false;
    assert(store.write("/config/knx", second));
    DurableStore restarted(fs);
    assert(restarted.read("/config/knx", actual) && actual == second);
    // Corrupt the newest slot: restart must select the older valid generation.
    auto &newest = fs.files["/config/knx.1"];
    newest[newest.find("bbbb")] = 'c';
    assert(restarted.read("/config/knx", actual) && actual == first);
    // Legacy string envelopes still load and migrate. The same large legacy
    // string cannot be parsed under the bounded allocator (the original bug).
    FS legacyFs;
    JsonDocument legacy;
    legacy["generation"] = 7;
    uint32_t hash = 2166136261u;
    for (auto c : first)
    {
        hash = (hash ^ uint8_t(c)) * 16777619u;
    }
    legacy["checksum"] = hash;
    legacy["payload"] = first;
    serializeJson(legacy, legacyFs.files["/config/knx.0"]);
    JsonDocument constrained(&allocator);
    assert(deserializeJson(constrained, legacyFs.files["/config/knx.0"]) == DeserializationError::NoMemory);
    DurableStore legacyStore(legacyFs);
    assert(legacyStore.read("/config/knx", actual) && actual == first);
    assert(legacyStore.write("/config/knx", second));
    assert(legacyStore.read("/config/knx", actual) && actual == second);
    std::cout << "Durable store: large KNX images, retry, restart, short write and corruption passed\n";
}
