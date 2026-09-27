#pragma once
#include <ArduinoJson.h>
#include <FS.h>

// Two complete generations, each independently checksummed. A failed write
// leaves the previously selected generation intact.
class DurableStore
{
    public:
        explicit DurableStore(FS &filesystem) : fs(filesystem)
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
            fs.mkdir("/config");
            const auto first = readSlot(String(base) + ".0");
            const auto second = readSlot(String(base) + ".1");
            const bool replaceFirst = !first.valid || (second.valid && first.generation <= second.generation);
            const String path = String(base) + (replaceFirst ? ".0" : ".1");
            JsonDocument document;
            document["generation"] = max(first.generation, second.generation) + 1;
            document["checksum"] = checksum(payload);
            document["payload"] = payload;

            File file = fs.open(path, "w");
            if (!file)
            {
                return false;
            }
            const size_t expected = measureJson(document);
            const size_t written = serializeJson(document, file);
            file.flush();
            file.close();
            const auto verified = readSlot(path);
            return written == expected && verified.valid && verified.payload == payload;
        }

    private:
        struct Slot
        {
            String payload;
            uint32_t generation = 0;
            bool valid = false;
        };

        FS &fs;

        static uint32_t checksum(const String &payload)
        {
            uint32_t hash = 2166136261u;
            for (size_t i = 0; i < payload.length(); ++i)
            {
                hash = (hash ^ uint8_t(payload[i])) * 16777619u;
            }
            return hash;
        }

        Slot readSlot(const String &path)
        {
            File file = fs.open(path, "r");
            JsonDocument document;
            if (!file || file.size() > 32768 || deserializeJson(document, file) || !document["payload"].is<String>() ||
                !document["generation"].is<uint32_t>())
            {
                return {};
            }
            const String payload = document["payload"].as<String>();
            if (checksum(payload) != document["checksum"].as<uint32_t>())
            {
                return {};
            }
            return {payload, document["generation"].as<uint32_t>(), true};
        }
};
