#include "Actuator.h"
#include "HomeAssistant.h"
#include "protocols/KnxAdapter.h"
#include "protocols/Modbus.h"
#include <NetworkSupport.h>
#include <WiFi.h>
#include <esp32-hal-rgb-led.h>
#include <driver/gpio.h>

namespace actuator
{
static bool network()
{
    return NetworkSupport::online();
}

Actuator::Actuator(ESP32SvelteKit &f) : fs(*f.getFS()), store(fs), framework(f)
{
    mutex = xSemaphoreCreateRecursiveMutex();
    queue = xQueueCreate(8, sizeof(std::shared_ptr<Request> *));
}

Actuator::~Actuator() = default;

void Actuator::safePins()
{
    for (auto pin : board::RelayPins)
    {
        // Preload LOW before enabling output; Arduino rejects digitalWrite
        // until pinMode has registered the pin with its peripheral manager.
        gpio_set_level(static_cast<gpio_num_t>(pin), 0);
        pinMode(pin, OUTPUT);
    }
    pinMode(board::BootPin, INPUT_PULLUP);
    pinMode(board::BuzzerPin, OUTPUT);
    digitalWrite(board::BuzzerPin, LOW);
    rgbLedWrite(board::RgbPin, 0, 0, 0);
}

void Actuator::begin()
{
    networkLost = millis() ? millis() : 1;
    safePins();
    pwmReady = ledcAttach(board::BuzzerPin, 2000, 8);
    if (!pwmReady)
    {
        fault = 1;
        faultText = "Buzzer PWM allocation failed";
    }
    modbus.reset(new Modbus(*this));
    knx.reset(new KnxAdapter(*this));
    if (fs.exists("/reset.pending"))
    {
        reset();
        return;
    }
    String data, error;
    JsonDocument doc;
    if (store.read("/config/actuator", data))
    {
        if (deserializeJson(doc, data) || !Config::parse(doc.as<JsonObjectConst>(), config, error))
        {
            fault = 2;
            faultText = "Invalid actuator configuration: " + error;
            config = Config();
        }
    }
    for (uint8_t i = 0; i < 6; ++i)
    {
        relay(i, config.relays[i].enabled && config.relays[i].startup, "startup");
    }
    framework.getSocket()->registerEvent("device.state");
    for (auto path : {"/rest/device/status", "/rest/device/config", "/rest/device/commands", "/rest/protocol/transition",
                      "/rest/knx/config", "/rest/knx/programming"})
    {
        endpoint(path);
    }
    FactoryResetService::setResetHandler(
        [this]()
        {
            auto r = std::make_shared<Request>();
            r->path = "reset";
            auto ptr = new std::shared_ptr<Request>(r);
            if (xQueueSend(queue, &ptr, 0) != pdTRUE)
            {
                delete ptr;
            }
        });
#if FT_ENABLED(FT_MQTT)
    homeAssistant.reset(new HomeAssistant(*this, framework));
    homeAssistant->begin();
#endif
    xTaskCreatePinnedToCore([](void *p) { static_cast<Actuator *>(p)->loop(); }, "Actuator", 16384, this, 2, nullptr, 1);
}

void Actuator::audit(const String &message)
{
    auditLog[auditHead++ % 16] = String(millis()) + ": " + message;
}

void Actuator::snapshot(JsonObject o)
{
    o["revision"] = config.revision;
    o["mode"] = modeName(config.mode);
    o["protocolState"] = protocolState;
    o["fault"] = fault;
    o["error"] = faultText;
    o["uptime"] = millis() / 1000;
    o["sta"] = WiFi.STA.hasIP();
    o["networkOnline"] = network();
    o["networkInterface"] = NetworkSupport::interfaceName();
    o["ip"] = NetworkSupport::localIP().toString();
    o["ap"] = (WiFi.getMode() & WIFI_MODE_AP) != 0;
    o["apClients"] = WiFi.softAPgetStationNum();
    o["pressed"] = gesture.pressed();
    o["lastGesture"] = lastGesture;
    o["gestureCount"] = gestureCount;
    o["resetArmed"] = gesture.armed(millis());
    auto a = o["relays"].to<JsonArray>();
    for (int i = 0; i < 6; ++i)
    {
        auto j = a.add<JsonObject>();
        j["on"] = outputs[i];
        j["enabled"] = config.relays[i].enabled;
        j["blocked"] = blocks[i];
        j["source"] = sources[i];
    }
    auto rgb = o["color"].to<JsonArray>();
    for (auto v : rgbNow)
    {
        rgb.add(v);
    }
    o["toneHz"] = toneNow;
    o["rgbEnabled"] = config.rgb;
    o["buzzerEnabled"] = config.buzzer;
    o["buttonEnabled"] = config.button;
    o["rs485Enabled"] = config.rs485;
    o["requests"] = modbus->requests;
    o["frameErrors"] = modbus->errors;
    o["rejected"] = modbus->rejected;
    o["configWindow"] = modbus->windowOpen();
    o["tcpClients"] = modbus->clients();
    o["programming"] = knx->isProgramming();
    o["knxConfigured"] = knx->configured();
    auto log = o["audit"].to<JsonArray>();
    for (int i = 0; i < 16; ++i)
    {
        auto &line = auditLog[(auditHead + i) % 16];
        if (line.length())
        {
            log.add(line);
        }
    }
}

bool Actuator::relay(uint8_t c, bool value, const char *origin, bool pulse)
{
    if (c >= 6 || !config.relays[c].enabled || blocks[c])
    {
        return false;
    }
    outputs[c] = value;
    sources[c] = origin;
    pulseUntil[c] = pulse ? millis() + config.relays[c].pulseMs : 0;
    digitalWrite(board::RelayPins[c], value ? HIGH : LOW);
    if (knx)
    {
        knx->status(c, value);
    }
    return true;
}

void Actuator::disableConfiguredRelays(bool publishStatus)
{
    for (uint8_t channel = 0; channel < board::RelayPins.size(); ++channel)
    {
        if (config.relays[channel].enabled)
        {
            continue;
        }
        outputs[channel] = false;
        pulseUntil[channel] = 0;
        digitalWrite(board::RelayPins[channel], LOW);
        if (publishStatus)
        {
            knx->status(channel, false);
        }
    }
}

void Actuator::identify()
{
    manualRgb = true;
    manualColor[0] = 255;
    manualColor[1] = 255;
    manualColor[2] = 255;
    rgbUntil = millis() + 5000;
}

void Actuator::tone(uint16_t hz, uint16_t ms, uint8_t duty)
{
    if (config.buzzer && pwmReady)
    {
        toneDuty = duty;
        toneNow = hz;
        toneUntil = millis() + ms;
    }
}

void Actuator::programming(bool value)
{
    if (config.mode == Knx)
    {
        knx->programming(value);
    }
}

void Actuator::binding(Binding b)
{
    for (uint8_t c = 0; c < 6; ++c)
    {
        if (b.action < 1 || b.action > 4 || (b.target && b.target != c + 1))
        {
            continue;
        }
        if (b.action == 1)
        {
            relay(c, true, "button");
        }
        if (b.action == 2)
        {
            relay(c, false, "button");
        }
        if (b.action == 3)
        {
            relay(c, !outputs[c], "button");
        }
        if (b.action == 4)
        {
            relay(c, true, "button", true);
        }
    }
    if (b.action == 5)
    {
        for (uint8_t i = 0; i < 6; ++i)
        {
            relay(i, false, "button");
        }
    }
    if (b.action == 6)
    {
        identify();
    }
    if (b.action == 7)
    {
        toneUntil = 0;
        toneNow = 0;
    }
    if (b.action == 8)
    {
        programming(!knx->isProgramming());
    }
}

void Actuator::stopProtocol()
{
    modbus->stop();
    knx->stop();
    protocolState = "off";
}

void Actuator::startProtocol()
{
    protocolIp = NetworkSupport::localIP();
    protocolInterface = NetworkSupport::interfaceIndex();
    if (config.mode == Off)
    {
        protocolState = "off";
        return;
    }
    if (config.mode != Rtu && !network())
    {
        protocolState = "waiting_network";
        return;
    }
    bool ok = config.mode == Knx ? knx->begin() : modbus->begin();
    protocolState = ok ? "running" : "failed";
    if (!ok)
    {
        fault = 3;
        faultText = "Protocol initialization failed";
    }
    lastBus = millis();
}

bool Actuator::apply(Config candidate, String &error)
{
    if (candidate.revision != config.revision)
    {
        error = "Configuration revision conflict";
        return false;
    }
    if (config.mode == Knx && candidate.mode == Knx && knx->configured())
    {
        for (int c = 0; c < 6; ++c)
        {
            auto &a = config.relays[c];
            auto &b = candidate.relays[c];
            if (a.enabled != b.enabled || a.startup != b.startup || a.disconnectOff != b.disconnectOff || a.timeout != b.timeout ||
                a.pulseMs != b.pulseMs)
            {
                error = "Edit commissioned relay parameters in the KNX section";
                return false;
            }
        }
    }
    auto previous = config;
    candidate.revision++;
    // Stop before opening any replacement listener or UART; saved state is not yet changed.
    stopProtocol();
    config = candidate;
    startProtocol();
    if (protocolState == "failed")
    {
        stopProtocol();
        config = previous;
        startProtocol();
        error = "Protocol failed; restored previous profile";
        return false;
    }
    JsonDocument doc;
    config.write(doc.to<JsonObject>());
    String data;
    serializeJson(doc, data);
    if (!store.write("/config/actuator", data))
    {
        stopProtocol();
        config = previous;
        startProtocol();
        error = "Configuration persistence failed";
        return false;
    }
    disableConfiguredRelays(true);
    if (!config.rgb)
    {
        manualRgb = false;
        rgbUntil = 0;
    }
    if (!config.buzzer)
    {
        toneUntil = 0;
        toneNow = 0;
    }
    audit("Configuration applied revision " + String(config.revision));
    return true;
}

void Actuator::reset()
{
    for (auto pin : board::RelayPins)
    {
        digitalWrite(pin, LOW);
    }
    ledcWriteTone(board::BuzzerPin, 0);
    stopProtocol();
    File marker = fs.open("/reset.pending", "w");
    if (!marker)
    {
        fault = 4;
        faultText = "Cannot persist reset marker";
        return;
    }
    marker.print("reset");
    marker.flush();
    marker.close();
    knx->erase();
    File dir = fs.open("/config");
    File entry;
    bool ok = true;
    while ((entry = dir.openNextFile()))
    {
        String path = entry.path();
        bool directory = entry.isDirectory();
        entry.close();
        if (directory || !fs.remove(path))
        {
            ok = false;
        }
    }
    dir.close();
    if (!ok)
    {
        fault = 4;
        faultText = "Reset incomplete; will retry at restart";
        return;
    }
    fs.remove("/reset.pending");
    ESP.restart();
}

void Actuator::indicators(uint32_t now)
{
    bool connected = network();
    if (connected && !previousNetwork)
    {
        connectedAt = now;
        networkLost = 0;
        if (!lastChirp || uint32_t(now - lastChirp) > 30000)
        {
            tone(2000, 100);
            lastChirp = now;
        }
    }
    if (!connected && previousNetwork)
    {
        networkLost = now;
    }
    previousNetwork = connected;
    if (connectedAt && uint32_t(now - connectedAt) >= 200 && uint32_t(now - connectedAt) < 210 && lastChirp == connectedAt)
    {
        tone(2600, 100);
    }
    if ((fault || (networkLost && uint32_t(now - networkLost) > 30000)) && uint32_t(now - lastErrorTone) > 60000)
    {
        tone(800, 400);
        lastErrorTone = now;
    }
    uint8_t r = 0, g = 0, b = 0;
    bool on = (now % 1000) < 500;
    if (gesture.armed(now))
    {
        r = (now % 200) < 100 ? 255 : 0;
    }
    else if (fault)
    {
        r = on ? 255 : 0;
    }
    else if (knx->isProgramming())
    {
        r = 255;
    }
    else if (manualRgb && int32_t(rgbUntil - now) > 0)
    {
        r = manualColor[0];
        g = manualColor[1];
        b = manualColor[2];
    }
    else if (connected)
    {
        g = 255;
    }
    else if (WiFi.getMode() & WIFI_MODE_AP)
    {
        b = on ? 255 : 0;
        g = WiFi.softAPgetStationNum() && on ? 255 : 0;
    }
    else
    {
        r = on ? 255 : 0;
        g = on ? 100 : 0;
    }
    if (manualRgb && int32_t(rgbUntil - now) <= 0)
    {
        manualRgb = false;
    }
    uint8_t brightness =
        config.rgb ? (manualRgb && !fault && !knx->isProgramming() && !gesture.armed(now) ? manualBrightness : config.brightness) : 0;
    r = uint16_t(r) * brightness / 100;
    g = uint16_t(g) * brightness / 100;
    b = uint16_t(b) * brightness / 100;
    if (r != rgbNow[0] || g != rgbNow[1] || b != rgbNow[2])
    {
        rgbNow[0] = r;
        rgbNow[1] = g;
        rgbNow[2] = b;
        rgbLedWrite(board::RgbPin, r, g, b);
    }
    if (!config.buzzer || !toneUntil || int32_t(toneUntil - now) <= 0)
    {
        toneNow = 0;
        toneUntil = 0;
    }
    static uint16_t lastTone = 65535;
    static uint8_t lastDuty = 0;
    if (pwmReady && (lastTone != toneNow || lastDuty != toneDuty))
    {
        ledcWriteTone(board::BuzzerPin, toneNow);
        if (toneNow)
        {
            ledcWrite(board::BuzzerPin, uint32_t(255) * toneDuty / 100);
        }
        lastTone = toneNow;
        lastDuty = toneDuty;
    }
}

void Actuator::loop()
{
    xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
    startProtocol();
    xSemaphoreGiveRecursive(mutex);
    for (;;)
    {
        xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
        std::shared_ptr<Request> *ptr = nullptr;
        if (xQueueReceive(queue, &ptr, 0) == pdTRUE)
        {
            auto r = *ptr;
            delete ptr;
            execute(*r);
            xSemaphoreGive(r->done);
        }
        uint32_t now = millis();
        auto event = gesture.update(digitalRead(board::BootPin) == LOW, now);
        if (event != Gesture::None)
        {
            lastGesture = event;
            ++gestureCount;
#if FT_ENABLED(FT_MQTT)
            homeAssistant->event(event);
#endif
            if (event == Gesture::Reset)
            {
                reset();
            }
            else if (config.button)
            {
                binding(config.clicks[event - 1]);
            }
        }
        for (uint8_t i = 0; i < 6; ++i)
        {
            if (pulseUntil[i] && int32_t(now - pulseUntil[i]) >= 0)
            {
                pulseUntil[i] = 0;
                outputs[i] = false;
                digitalWrite(board::RelayPins[i], LOW);
                knx->status(i, false);
            }
            auto &r = config.relays[i];
            bool lost =
                (config.mode == Rtu && config.busWatchdog && uint32_t(now - lastBus) > uint32_t(r.timeout) * 1000) ||
                ((config.mode == Tcp || config.mode == Knx) && networkLost && uint32_t(now - networkLost) > uint32_t(r.timeout) * 1000);
            if (r.disconnectOff && lost && outputs[i])
            {
                outputs[i] = false;
                digitalWrite(board::RelayPins[i], LOW);
                knx->status(i, false);
                sources[i] = "disconnect policy";
            }
        }
        if (config.mode == Tcp || config.mode == Knx)
        {
            bool ready = network();
            if ((ready && protocolState == "waiting_network") ||
                (ready && (protocolIp != NetworkSupport::localIP() || protocolInterface != NetworkSupport::interfaceIndex())))
            {
                stopProtocol();
                startProtocol();
            }
            else if (!ready && protocolState != "waiting_network")
            {
                stopProtocol();
                protocolState = "waiting_network";
            }
        }
        if (protocolState == "running")
        {
            if (config.mode == Knx)
            {
                knx->loop();
            }
            else
            {
                modbus->loop();
            }
        }
        indicators(now);
#if FT_ENABLED(FT_MQTT)
        homeAssistant->loop(now);
#endif
        if (uint32_t(now - lastPublish) >= 1000)
        {
            JsonDocument doc;
            snapshot(doc.to<JsonObject>());
            auto obj = doc.as<JsonObject>();
            framework.getSocket()->emitEvent("device.state", obj);
            lastPublish = now;
        }
        xSemaphoreGiveRecursive(mutex);
        vTaskDelay(1);
    }
}
} // namespace actuator
