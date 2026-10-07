#include "KnxAdapter.h"
#include "KnxAddress.h"
#include "generated/KnxProduct.h"
#include <NetworkSupport.h>
#include <NetworkUdp.h>
#include <algorithm>
#include <esp32_platform.h>
#include <knx/bau57B0.h>
#include <knx/datapoint_types.h>
#include <vector>

namespace actuator
{
// Access protected table objects without modifying the pinned upstream library.
class DeviceBau: public Bau57B0
{
  public:
    explicit DeviceBau(Platform &p) : Bau57B0(p)
    {
    }

    bool configured() override
    {
        _configured = true;
        return Bau57B0::configured();
    }

    AddressTableObject &addresses()
    {
        return _addrTable;
    }

    AssociationTableObject &associations()
    {
        return _assocTable;
    }
};

class StoredPlatform: public Esp32Platform
{
  public:
    Actuator &d;
    uint8_t bytes[8192];
    bool loaded = false, pending = false, ok = true, suspended = false;
    uint32_t revision = 1;
    uint32_t retryAt = 0;
    String owner = "ets";

    explicit StoredPlatform(Actuator &d) : d(d)
    {
        memset(bytes, 0xff, sizeof(bytes));
    }

    uint32_t currentIpAddress() override
    {
        return NetworkSupport::localIP();
    }

    uint32_t currentSubnetMask() override
    {
        auto *interface = NetworkSupport::uplink();
        return interface ? uint32_t(interface->subnetMask()) : 0;
    }

    uint32_t currentDefaultGateway() override
    {
        auto *interface = NetworkSupport::uplink();
        return interface ? uint32_t(interface->gatewayIP()) : 0;
    }

    void macAddress(uint8_t *address) override
    {
        auto *interface = NetworkSupport::uplink();
        if (interface)
        {
            interface->macAddress(address);
        }
        else
        {
            memset(address, 0, 6);
        }
    }

    void setupMultiCast(uint32_t address, uint16_t port) override
    {
        udp.stop();
        multicastReady = udp.beginMulticast(IPAddress(htonl(address)), port) == 1;
    }

    void closeMultiCast() override
    {
        udp.stop();
        multicastReady = false;
    }

    bool sendBytesMultiCast(uint8_t *buffer, uint16_t length) override
    {
        return multicastReady && udp.beginMulticastPacket() == 1 && udp.write(buffer, length) == length && udp.endPacket() == 1;
    }

    int readBytesMultiCast(uint8_t *buffer, uint16_t capacity, uint32_t &address, uint16_t &port) override
    {
        const int length = udp.parsePacket();
        if (length <= 0)
        {
            return 0;
        }
        if (length > capacity)
        {
            udp.clear();
            return 0;
        }
        _remoteIP = udp.remoteIP();
        _remotePort = udp.remotePort();
        address = htonl(uint32_t(_remoteIP));
        port = _remotePort;
        return udp.read(buffer, length);
    }

    bool sendBytesUniCast(uint32_t address, uint16_t port, uint8_t *buffer, uint16_t length) override
    {
        return multicastReady && udp.beginPacket(address ? IPAddress(htonl(address)) : _remoteIP, port ? port : _remotePort) == 1 &&
               udp.write(buffer, length) == length && udp.endPacket() == 1;
    }

    NetworkUDP udp;
    bool multicastReady = false;

    uint8_t *getEepromBuffer(uint32_t size) override
    {
        if (size > sizeof(bytes))
        {
            return nullptr;
        }
        if (!loaded)
        {
            loaded = true;
            String payload;
            JsonDocument doc;
            if (d.store.read("/config/knx", payload) && !deserializeJson(doc, payload))
            {
                String hex;
                if (doc["image"].is<JsonArray>())
                {
                    hex.reserve(sizeof(bytes) * 2);
                    for (auto chunk : doc["image"].as<JsonArrayConst>())
                    {
                        hex += chunk.as<String>();
                    }
                }
                else
                {
                    hex = doc["image"] | ""; // Legacy image.
                }
                if (hex.length() == sizeof(bytes) * 2)
                {
                    for (size_t i = 0; i < sizeof(bytes); ++i)
                    {
                        char text[3] = {hex[i * 2], hex[i * 2 + 1], 0};
                        bytes[i] = strtoul(text, nullptr, 16);
                    }
                    revision = doc["revision"] | 1;
                    owner = doc["owner"] | "ets";
                }
            }
        }
        return bytes;
    }

    void commitToEeprom() override
    {
        pending = true;
    }

    bool commit()
    {
        if (suspended)
        {
            return false;
        }
        String payload;
        {
            JsonDocument doc;
            const char *digits = "0123456789abcdef";
            auto chunks = doc["image"].to<JsonArray>();
            for (size_t offset = 0; offset < sizeof(bytes); offset += 512)
            {
                char hex[1025];
                for (size_t i = 0; i < 512; ++i)
                {
                    hex[i * 2] = digits[bytes[offset + i] >> 4];
                    hex[i * 2 + 1] = digits[bytes[offset + i] & 15];
                }
                hex[1024] = 0;
                chunks.add(JsonString(hex, size_t(1024)));
            }
            doc["revision"] = revision + 1;
            doc["owner"] = owner;
            const size_t expected = measureJson(doc);
            ok = !doc.overflowed() && payload.reserve(expected) && serializeJson(doc, payload) == expected;
        }
        if (!ok)
        {
            retryAt = millis() + 5000;
            d.fault = 5;
            d.faultText = "KNX persistence failed: image allocation failed";
            return false;
        }
        // Release the hex image and JSON document before durable verification.
        ok = d.store.write("/config/knx", payload);
        if (ok)
        {
            ++revision;
            pending = false;
            retryAt = 0;
            if (d.fault == 5)
            {
                d.fault = 0;
                d.faultText = "";
            }
        }
        else
        {
            d.fault = 5;
            d.faultText = String("KNX persistence failed: ") + d.store.lastError();
            retryAt = millis() + 5000;
        }
        return ok;
    }
};

// Non-owning facade: upstream KnxFacade(B&) deletes the supplied platform on
// destruction. This adapter has explicit member ownership instead.
class DeviceStack
{
    DeviceBau &b;

  public:
    explicit DeviceStack(DeviceBau &b) : b(b)
    {
    }

    void manufacturerId(uint16_t v)
    {
        b.deviceObject().manufacturerId(v);
    }

    void hardwareType(const uint8_t *v)
    {
        b.deviceObject().hardwareType(v);
    }

    void version(uint16_t v)
    {
        b.deviceObject().version(v);
    }

    void readMemory()
    {
        b.readMemory();
    }

    void writeMemory()
    {
        b.writeMemory();
    }

    void enabled(bool v)
    {
        b.enabled(v);
    }

    void loop()
    {
        b.loop();
    }

    void progMode(bool v)
    {
        b.deviceObject().progMode(v);
    }

    bool progMode()
    {
        return b.deviceObject().progMode();
    }

    uint16_t individualAddress()
    {
        return b.deviceObject().individualAddress();
    }

    GroupObject &getGroupObject(uint16_t n)
    {
        return b.groupObjectTable().get(n);
    }
};

static uint16_t read16(const uint8_t *p)
{
    return uint16_t(p[0]) << 8 | p[1];
}

static void write16(uint8_t *p, uint16_t n)
{
    p[0] = n >> 8;
    p[1] = n;
}

static uint32_t tableAddress(TableObject &t)
{
    uint8_t b[4] = {}, n = 1;
    t.readProperty(PID_TABLE_REFERENCE, 1, n, b);
    return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
}

static uint32_t tableSize(TableObject &t)
{
    uint8_t b[8] = {}, n = 1;
    t.readProperty(PID_MCB_TABLE, 1, n, b);
    return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
}

static bool loadTable(DeviceBau &bau, TableObject &table, const std::vector<uint8_t> &bytes)
{
    uint8_t event[10] = {4}, count = 1;
    table.writeProperty(PID_LOAD_STATE_CONTROL, 1, event, count); // unload
    event[0] = 1;
    count = 1;
    table.writeProperty(PID_LOAD_STATE_CONTROL, 1, event, count);
    event[0] = 3;
    event[1] = 0x0b;
    event[2] = 0;
    event[3] = 0;
    event[4] = bytes.size() >> 8;
    event[5] = bytes.size();
    event[6] = 1;
    event[7] = 0;
    count = 1;
    table.writeProperty(PID_LOAD_STATE_CONTROL, 1, event, count);
    uint32_t address = tableAddress(table);
    if (table.loadState() != LS_LOADING || !address || address + bytes.size() > 8192)
    {
        return false;
    }
    bau.memory().writeMemory(address, bytes.size(), const_cast<uint8_t *>(bytes.data()));
    event[0] = 2;
    count = 1;
    table.writeProperty(PID_LOAD_STATE_CONTROL, 1, event, count);
    return table.loadState() == LS_LOADED;
}

struct KnxAdapter::Impl
{
    Actuator &d;
    StoredPlatform platform;
    DeviceBau bau;
    DeviceStack stack;
    bool initialized = false, running = false, callbacks = false, wasConfigured = false;
    uint32_t deadline = 0;

    explicit Impl(Actuator &d) : d(d), platform(d), bau(platform), stack(bau)
    {
    }

    bool valid()
    {
        if (!bau.configured() || bau.groupObjectTable().entryCount() != product::ObjectCount ||
            tableSize(bau.parameters()) != product::ParameterBytes || bau.parameters().getByte(0) != 1)
        {
            return false;
        }
        uint8_t identity[5] = {}, count = 1;
        bau.parameters().readProperty(PID_PROG_VERSION, 1, count, identity);
        if (read16(identity) != product::Manufacturer || read16(identity + 2) != product::Application || identity[4] != product::Version)
        {
            return false;
        }
        auto &p = bau.parameters();
        for (unsigned c = 0; c < product::Channels; ++c)
        {
            unsigned off = product::channelOffset(c);
            if (p.getByte(off) > 1 || p.getByte(off + 1) > 1 || p.getByte(off + 2) > 1 || !p.getWord(off + 4) ||
                p.getWord(off + 4) > 3600 || p.getWord(off + 6) < 10 || p.getWord(off + 6) > 60000)
            {
                return false;
            }
        }
        return true;
    }

    bool busy()
    {
        return bau.parameters().loadState() == LS_LOADING || bau.groupObjectTable().loadState() == LS_LOADING ||
               bau.addresses().loadState() == LS_LOADING || bau.associations().loadState() == LS_LOADING;
    }

    void attach()
    {
        if (!valid())
        {
            return;
        }
        for (uint8_t c = 0; c < 6; ++c)
        {
            auto &sw = stack.getGroupObject(1 + 3 * c);
            auto &block = stack.getGroupObject(2 + 3 * c);
            auto &status = stack.getGroupObject(3 + 3 * c);
            sw.dataPointType(Dpt(1, 1));
            block.dataPointType(Dpt(1, 3));
            status.dataPointType(Dpt(1, 2));
            sw.callback(
                [this, c](GroupObject &go)
                {
                    if (valid())
                    {
                        d.relay(c, bool(go.value()), "knx");
                    }
                });
            block.callback(
                [this, c](GroupObject &go)
                {
                    if (valid())
                    {
                        d.blocks[c] = bool(go.value());
                    }
                });
            status.valueNoSend(d.outputs[c]);
        }
        callbacks = true;
    }

    void syncParameters()
    {
        if (!valid())
        {
            return;
        }
        auto &params = bau.parameters();
        Config candidate = d.config;
        for (int c = 0; c < 6; ++c)
        {
            auto &r = candidate.relays[c];
            int offset = 4 + 8 * c;
            uint8_t enabled = params.getByte(offset), startup = params.getByte(offset + 1), policy = params.getByte(offset + 2);
            uint16_t timeout = params.getWord(offset + 4), pulse = params.getWord(offset + 6);
            if (enabled > 1 || startup > 1 || policy > 1 || !timeout || timeout > 3600 || pulse < 10 || pulse > 60000)
            {
                d.fault = 6;
                d.faultText = "Invalid KNX application parameters";
                return;
            }
            r.enabled = enabled;
            r.startup = startup;
            r.disconnectOff = policy;
            r.timeout = timeout;
            r.pulseMs = pulse;
        }
        // Canonical parameter bytes are stored with the KNX image. Do not create a
        // second independently persistent copy; project them into the live model.
        d.config.relays = candidate.relays;
        d.disableConfiguredRelays();
    }
};

KnxAdapter::KnxAdapter(Actuator &d) : impl(new Impl(d))
{
}

KnxAdapter::~KnxAdapter() = default;

bool KnxAdapter::begin()
{
    auto &x = *impl;
    if (!x.initialized)
    {
        ArduinoPlatform::SerialDebug = &Serial;
        // Development identity, deliberately not a commercial manufacturer claim.
        x.stack.manufacturerId(product::Manufacturer);
        const uint8_t hardware[6] = {'E', 'D', 'G', 'E', '0', '1'};
        x.stack.hardwareType(hardware);
        x.stack.version(product::Version);
        x.bau.deviceObject().bauNumber(x.platform.uniqueSerialNumber());
        x.stack.readMemory();
        x.initialized = true;
        x.syncParameters();
        x.attach();
        // Restoring a web-owned image is not a new ETS download.
        x.wasConfigured = x.valid();
    }
    x.syncParameters();
    x.stack.enabled(true);
    x.running = x.platform.multicastReady;
    if (!x.running)
    {
        x.stack.enabled(false);
    }
    return x.running;
}

void KnxAdapter::stop()
{
    auto &x = *impl;
    if (x.initialized)
    {
        x.stack.progMode(false);
        x.stack.enabled(false);
        x.platform.closeMultiCast();
    }
    x.running = false;
    x.deadline = 0;
}

void KnxAdapter::programming(bool active)
{
    auto &x = *impl;
    if (!x.running)
    {
        return;
    }
    x.stack.progMode(active);
    x.deadline = active ? millis() + 300000 : 0;
}

bool KnxAdapter::isProgramming() const
{
    return impl->initialized && impl->stack.progMode();
}

bool KnxAdapter::configured() const
{
    return impl->initialized && impl->valid();
}

void KnxAdapter::status(uint8_t c, bool value)
{
    auto &x = *impl;
    if (c < 6 && x.running && x.valid())
    {
        auto &go = x.stack.getGroupObject(3 + c * 3);
        if (bool(go.value()) != value)
        {
            go.value(value);
        }
    }
}

void KnxAdapter::loop()
{
    auto &x = *impl;
    if (!x.running)
    {
        return;
    }
    if (x.busy())
    {
        x.callbacks = false;
        x.wasConfigured = false;
        x.platform.owner = "ets";
    }
    x.stack.loop();
    bool configured = x.valid();
    if (configured && (!x.callbacks || !x.wasConfigured))
    {
        x.syncParameters();
        x.attach();
        x.platform.owner = "ets";
    }
    if (x.platform.pending && !x.busy() && configured && (!x.platform.retryAt || int32_t(millis() - x.platform.retryAt) >= 0))
    {
        x.platform.commit();
    }
    x.wasConfigured = configured;
    if (x.stack.progMode() && !x.deadline)
    {
        x.deadline = millis() + 300000;
    }
    if (!x.stack.progMode())
    {
        x.deadline = 0;
    }
    if (x.deadline && int32_t(millis() - x.deadline) >= 0 && !x.busy())
    {
        programming(false);
    }
}

void KnxAdapter::snapshot(JsonObject o)
{
    auto &x = *impl;
    o["configured"] = configured();
    o["active"] = isProgramming();
    o["owner"] = x.platform.owner;
    o["revision"] = x.platform.revision;
    o["busy"] = x.initialized && x.busy();
    o["developmentIdentity"] = true;
    uint16_t address = x.initialized ? x.stack.individualAddress() : 0;
    o["address"] = String(address >> 12) + "." + String((address >> 8) & 15) + "." + String(address & 255);
    auto objects = o["objects"].to<JsonArray>();
    uint32_t assocAddr = x.initialized ? tableAddress(x.bau.associations()) : 0;
    uint32_t size = x.initialized ? tableSize(x.bau.associations()) : 0;
    uint8_t *bytes = assocAddr && assocAddr + size <= 8192 ? x.platform.bytes + assocAddr : nullptr;
    uint16_t count = bytes && size >= 2 ? read16(bytes) : 0;
    if (uint32_t(count) * 4 + 2 > size)
    {
        count = 0;
    }
    for (int i = 1; i <= 18; ++i)
    {
        auto j = objects.add<JsonObject>();
        j["number"] = i;
        auto list = j["groups"].to<JsonArray>();
        for (int a = 0; a < count; ++a)
        {
            if (read16(bytes + 4 + a * 4) == i)
            {
                uint16_t ga = x.bau.addresses().getGroupAddress(read16(bytes + 2 + a * 4));
                list.add(String(ga >> 11) + "/" + String((ga >> 8) & 7) + "/" + String(ga & 255));
            }
        }
    }
    auto params = o["parameters"].to<JsonArray>();
    for (auto &r : x.d.config.relays)
    {
        auto j = params.add<JsonObject>();
        j["enabled"] = r.enabled;
        j["startup"] = r.startup;
        j["disconnectOff"] = r.disconnectOff;
        j["timeout"] = r.timeout;
        j["pulseMs"] = r.pulseMs;
    }
}

bool KnxAdapter::configure(JsonObjectConst o, String &error)
{
    auto &x = *impl;
    auto bad = [&](const char *message)
    {
        error = message;
        return false;
    };
    if (!x.running || x.busy())
    {
        return bad("KNX is inactive or ETS download is busy");
    }
    if (!o["revision"].is<uint32_t>() || o["revision"].as<uint32_t>() != x.platform.revision)
    {
        return bad("KNX revision conflict");
    }
    if (x.platform.owner == "ets" && o["takeover"] != true)
    {
        return bad("Explicit web takeover is required");
    }
    String text = o["address"] | "";
    uint16_t individualAddress;
    char tail;
    if (!o["address"].is<String>() || !parseIndividualAddress(text.c_str(), individualAddress))
    {
        return bad("Invalid individual address");
    }
    if (o["objects"].size() != 18 || o["parameters"].size() != 6)
    {
        return bad("Expected 18 objects and six parameter records");
    }
    std::vector<uint16_t> addresses;
    std::vector<std::pair<uint16_t, uint16_t>> associations;
    for (int i = 0; i < 18; ++i)
    {
        auto object = o["objects"][i];
        if (object["number"] != i + 1 || !object["groups"].is<JsonArrayConst>() || object["groups"].size() > 8)
        {
            return bad("Invalid object mapping");
        }
        std::vector<uint16_t> seen;
        for (auto item : object["groups"].as<JsonArrayConst>())
        {
            String value = item.as<String>();
            unsigned a, b, c;
            if (sscanf(value.c_str(), "%u/%u/%u%c", &a, &b, &c, &tail) != 3 || a > 31 || b > 7 || c > 255)
            {
                return bad("Invalid group address");
            }
            uint16_t ga = (a << 11) | (b << 8) | c;
            if (!ga || std::find(seen.begin(), seen.end(), ga) != seen.end())
            {
                return bad("Reserved or duplicate group address");
            }
            seen.push_back(ga);
            addresses.push_back(ga);
            associations.push_back({ga, uint16_t(i + 1)});
        }
    }
    std::sort(addresses.begin(), addresses.end());
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    if (addresses.size() > 64 || associations.size() > 96)
    {
        return bad("Group table capacity exceeded");
    }
    std::vector<uint8_t> params(52, 0), gos(38, 0), addr(2 + addresses.size() * 2), assoc(2 + associations.size() * 4);
    params[0] = 1;
    params[1] = 3;
    write16(gos.data(), 18);
    for (int c = 0; c < 6; ++c)
    {
        auto r = o["parameters"][c];
        if (!r["enabled"].is<bool>() || !r["startup"].is<bool>() || !r["disconnectOff"].is<bool>() || !r["timeout"].is<int>() ||
            !r["pulseMs"].is<int>())
        {
            return bad("Invalid KNX parameter types");
        }
        int timeout = r["timeout"], pulse = r["pulseMs"];
        if (timeout < 1 || timeout > 3600 || pulse < 10 || pulse > 60000)
        {
            return bad("Invalid parameter range");
        }
        int p = 4 + 8 * c;
        params[p] = r["enabled"].as<bool>();
        params[p + 1] = r["startup"].as<bool>();
        params[p + 2] = r["disconnectOff"].as<bool>();
        write16(params.data() + p + 4, timeout);
        write16(params.data() + p + 6, pulse);
    }
    for (int i = 1; i <= 18; ++i)
    {
        write16(gos.data() + i * 2, i % 3 == 0 ? 0x4c00 : 0x1400);
    }
    write16(addr.data(), addresses.size());
    for (size_t i = 0; i < addresses.size(); ++i)
    {
        write16(addr.data() + 2 + i * 2, addresses[i]);
    }
    write16(assoc.data(), associations.size());
    for (size_t i = 0; i < associations.size(); ++i)
    {
        auto pair = associations[i];
        write16(assoc.data() + 2 + i * 4, std::lower_bound(addresses.begin(), addresses.end(), pair.first) - addresses.begin() + 1);
        write16(assoc.data() + 4 + i * 4, pair.second);
    }
    // Preserve the full old stack image for in-memory rollback. The durable slot
    // remains unchanged until every table is loaded and serialized successfully.
    std::vector<uint8_t> backup(x.platform.bytes, x.platform.bytes + 8192);
    String previousOwner = x.platform.owner;
    x.stack.enabled(false);
    x.platform.suspended = true;
    bool ok = loadTable(x.bau, x.bau.parameters(), params) && loadTable(x.bau, x.bau.groupObjectTable(), gos) &&
              loadTable(x.bau, x.bau.addresses(), addr) && loadTable(x.bau, x.bau.associations(), assoc);
    if (ok)
    {
        uint8_t application[5] = {uint8_t(product::Manufacturer >> 8), uint8_t(product::Manufacturer), uint8_t(product::Application >> 8),
                                  uint8_t(product::Application), uint8_t(product::Version)};
        uint8_t count = 1;
        x.bau.parameters().writeProperty(PID_PROG_VERSION, 1, application, count);
        x.bau.deviceObject().individualAddress(individualAddress);
        x.platform.owner = "web";
        x.stack.writeMemory();
    }
    x.platform.suspended = false;
    const bool tablesLoaded = ok;
    ok = ok && x.platform.commit();
    if (!ok)
    {
        memcpy(x.platform.bytes, backup.data(), 8192);
        x.platform.owner = previousOwner;
        x.stack.readMemory();
        x.platform.pending = false;
        error = tablesLoaded ? x.d.faultText + "; previous image restored" : "KNX table loading failed; previous image restored";
    }
    x.stack.enabled(true);
    x.callbacks = false;
    x.syncParameters();
    x.attach();
    x.wasConfigured = x.valid();
    return ok;
}

void KnxAdapter::erase()
{
    auto &x = *impl;
    x.d.fs.remove("/config/knx.0");
    x.d.fs.remove("/config/knx.1");
    memset(x.platform.bytes, 0xff, 8192);
}
} // namespace actuator
