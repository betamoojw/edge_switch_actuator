#pragma once
#include "BoardProfile.h"
#include "DeviceConfig.h"
#include "DurableStore.h"
#include "Gesture.h"
#include <ESP32SvelteKit.h>
#include <HardwareSerial.h>
#include <NetworkServer.h>
#include <memory>

namespace actuator
{
class Modbus;
class KnxAdapter;

class Actuator
{
public:
    explicit Actuator(ESP32SvelteKit &framework);
    ~Actuator();
    static void safePins();
    void begin();
    void snapshot(JsonObject o);
    bool relay(uint8_t channel, bool value, const char *origin, bool pulse = false);
    bool apply(Config candidate, String &error);
    // Force disabled channels off regardless of their block state.
    void disableConfiguredRelays(bool publishStatus = false);
    void identify();
    void tone(uint16_t hz = 2000, uint16_t ms = 100, uint8_t duty = 25);
    void binding(Binding b);
    void reset();
    void programming(bool value);
    Config config;
    bool outputs[6] = {}, blocks[6] = {};
    uint32_t pulseUntil[6] = {};
    String sources[6];
    Gesture gesture;
    uint32_t gestureCount = 0, lastBus = 0;
    uint8_t lastGesture = 0;
    uint16_t fault = 0;
    String faultText, protocolState = "off";
    uint8_t rgbNow[3] = {};
    uint16_t toneNow = 0;
    uint8_t toneDuty = 25;
    uint32_t rgbUntil = 0, toneUntil = 0;
    bool manualRgb = false;
    uint8_t manualColor[3] = {255, 255, 255}, manualBrightness = 10;
    FS &fs;
    DurableStore store;
    std::unique_ptr<Modbus> modbus;
    std::unique_ptr<KnxAdapter> knx;

private:
    ESP32SvelteKit &framework;
    SemaphoreHandle_t mutex;
    QueueHandle_t queue;

    struct Request
    {
        String path, payload, role, user, result;
        uint8_t mask = 0;
        bool admin = false;
        int status = 200;
        SemaphoreHandle_t done = xSemaphoreCreateBinary();

        ~Request()
        {
            vSemaphoreDelete(done);
        }
    };

    uint32_t lastPublish = 0, networkLost = 0, lastChirp = 0, connectedAt = 0, lastErrorTone = 0;
    bool previousNetwork = false, pwmReady = false;
    IPAddress protocolIp;
    int protocolInterface = -1;
    void loop();
    void indicators(uint32_t now);
    void execute(Request &r);
    void endpoint(const char *path);
    void startProtocol();
    void stopProtocol();
    void audit(const String &message);

    struct Completed
    {
        String user, id, payload, result;
        int status = 200;
        uint32_t at = 0;
    };

    Completed completed[16];
    uint8_t completedHead = 0;
    String auditLog[16];
    uint8_t auditHead = 0;
};
} // namespace actuator
