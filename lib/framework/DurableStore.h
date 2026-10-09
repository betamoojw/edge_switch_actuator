#pragma once
#include <ArduinoJson.h>
#include <FS.h>
#include <algorithm>

// Two complete generations, each independently checksummed. A failed write
// leaves the previously selected generation intact.
class DurableStore
{
  public:
    explicit DurableStore(FS &filesystem, ArduinoJson::Allocator *allocator = JsonDocument().allocator())
        : fs(filesystem), allocator(allocator)
    {
    }

    bool read(const char *base, String &payload)
    {
        const auto first = readSlot(String(base) + ".0");
        const auto second = readSlot(String(base) + ".1");
        if (!first.valid && !second.valid)
        {
            return false;
        }
        payload = second.valid && (!first.valid || second.generation > first.generation) ? second.payload : first.payload;
        return true;
    }

    bool write(const char *base, const String &payload)
    {
        error = nullptr;
        fs.mkdir("/config");
        // Retain only metadata while selecting the inactive slot. KNX payloads
        // exceed 16 KB; retaining both images through verification wastes heap.
        const auto first = readSlot(String(base) + ".0", false);
        const auto second = readSlot(String(base) + ".1", false);
        if (error)
        {
            return false;
        }
        const bool replaceFirst = !first.valid || (second.valid && first.generation <= second.generation);
        const String path = String(base) + (replaceFirst ? ".0" : ".1");
        JsonDocument document(allocator);
        document["generation"] = max(first.generation, second.generation) + 1;
        document["checksum"] = checksum(payload);
        // Bound the parser's string allocations on the next read. A single
        // 16 KB string makes ArduinoJson's stream parser grow to a 32 KB block.
        auto chunks = document["payload"].to<JsonArray>();
        for (size_t offset = 0; offset < payload.length(); offset += 1024)
        {
            const size_t length = std::min(size_t(1024), payload.length() - offset);
            chunks.add(JsonString(payload.c_str() + offset, length));
        }
        if (document.overflowed())
        {
            error = "storage envelope allocation failed";
            return false;
        }

        File file = fs.open(path, "w");
        if (!file)
        {
            error = "storage slot open failed";
            return false;
        }
        const size_t expected = measureJson(document);
        const size_t written = serializeJson(document, file);
        file.flush();
        file.close();
        document.clear();
        if (written != expected)
        {
            error = "storage slot write incomplete";
            return false;
        }
        const auto verified = readSlot(path);
        if (!verified.valid || verified.payload != payload)
        {
            error = "storage slot verification failed";
            return false;
        }
        return true;
    }

    const char *lastError() const
    {
        return error ? error : "unknown storage failure";
    }

  private:
    struct Slot
    {
        String payload;
        uint32_t generation = 0;
        bool valid = false;
    };

    FS &fs;
    ArduinoJson::Allocator *allocator;
    const char *error = nullptr;

    static uint32_t checksum(const String &payload)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < payload.length(); ++i)
        {
            hash = (hash ^ uint8_t(payload[i])) * 16777619u;
        }
        return hash;
    }

    Slot readSlot(const String &path, bool includePayload = true)
    {
        File file = fs.open(path, "r");
        if (!file || file.size() > 32768)
        {
            return {};
        }
        JsonDocument document(allocator);
        const auto parsed = deserializeJson(document, file);
        if (parsed == DeserializationError::NoMemory)
        {
            error = "storage parse allocation failed";
            return {};
        }
        if (parsed || !document["generation"].is<uint32_t>())
        {
            return {};
        }
        String result;
        size_t payloadLength = 0;
        uint32_t hash = 2166136261u;
        auto consume = [&](JsonVariantConst value)
        {
            if (!value.is<JsonString>())
            {
                return false;
            }
            const auto chunk = value.as<JsonString>();
            payloadLength += chunk.size();
            for (size_t i = 0; i < chunk.size(); ++i)
            {
                hash = (hash ^ uint8_t(chunk.c_str()[i])) * 16777619u;
                if (includePayload)
                {
                    result += chunk.c_str()[i];
                }
            }
            return true;
        };
        if (document["payload"].is<JsonArray>())
        {
            for (auto chunk : document["payload"].as<JsonArrayConst>())
            {
                if (!consume(chunk))
                {
                    return {};
                }
            }
        }
        else if (!consume(document["payload"]))
        {
            return {}; // Legacy string envelope.
        }
        if (hash != document["checksum"].as<uint32_t>())
        {
            return {};
        }
        if (includePayload && result.length() != payloadLength)
        {
            error = "storage payload allocation failed";
            return {};
        }
        return {result, document["generation"].as<uint32_t>(), true};
    }
};
