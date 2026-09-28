#include "HomeAssistant.h"
#include "HomeAssistantCommand.h"
#include <device/Actuator.h>

#if FT_ENABLED(FT_MQTT)
namespace actuator
{
static constexpr const char *Marker = "/config/home-assistant-discovery";

HomeAssistant::HomeAssistant(Actuator &device, ESP32SvelteKit &framework)
    : device(device), settings(*framework.getMqttSettingsService()), mqtt(*framework.getMqttClient())
{
}

void HomeAssistant::begin()
{
    id = SettingValue::format("edge_#{unique_id}");
    base = "edge_switch/" + id;
    subscription = base + "/+/+/set";
    commands = xQueueCreate(16, sizeof(int));
    if (!commands)
    {
        return;
    }
    cleanupPending = device.fs.exists(Marker);
    mqtt.onConnect([this](bool) { refresh = true; });
    mqtt.onDisconnect(
        [this](bool)
        {
            accepting = false;
            xQueueReset(commands);
            refresh = true;
        });
    mqtt.onMessage(
        [this](char *topic, char *payload, int retain, int, bool)
        {
            if (!std::strcmp(topic, "homeassistant/status"))
            {
                if (!std::strcmp(payload, "online"))
                {
                    refresh = true;
                }
                return;
            }
            if (!accepting || !String(topic).startsWith(base + "/"))
            {
                return;
            }
            int command = homeAssistantCommand(topic + base.length() + 1, payload, retain != 0);
            if (command >= 0)
            {
                xQueueSend(commands, &command, 0);
            }
        });
}

bool HomeAssistant::publish(const String &topic, const String &payload, bool retain)
{
    return mqtt.connected() && mqtt.publish(topic.c_str(), 1, retain, payload.c_str()) >= 0;
}

bool HomeAssistant::discovery(bool remove)
{
    bool ok = true;
    auto entity = [&](const char *platform, const String &key, const String &name, const String &value, const String &command = String(),
                      const char *deviceClass = nullptr)
    {
        String topic = "homeassistant/" + String(platform) + "/" + id + "/" + key + "/config";
        if (remove)
        {
            ok = publish(topic, "") && ok;
            return;
        }
        JsonDocument doc;
        doc["name"] = name;
        doc["unique_id"] = id + "_" + key;
        doc["availability_topic"] = settings.getStatusTopic();
        if (!std::strcmp(platform, "switch"))
        {
            // A disabled or interlocked channel cannot accept a command.
            doc.remove("availability_topic");
            doc["availability_mode"] = "all";
            auto availability = doc["availability"].to<JsonArray>();
            availability.add<JsonObject>()["topic"] = settings.getStatusTopic();
            auto channel = availability.add<JsonObject>();
            channel["topic"] = base + "/state";
            String index = String(key.substring(6).toInt() - 1);
            channel["value_template"] = "{{ 'online' if value_json.relays[" + index + "].enabled and not value_json.relays[" + index +
                                        "].blocked else 'offline' }}";
        }
        auto dev = doc["device"].to<JsonObject>();
        dev["identifiers"].to<JsonArray>().add(id);
        dev["name"] = "Edge Switch " + id.substring(5);
        dev["manufacturer"] = "M-Tech";
        dev["model"] = "ESP32-S3 Relay 6CH";
        dev["sw_version"] = APP_VERSION;
        doc["origin"]["name"] = "Edge Switch Actuator";
        if (deviceClass)
        {
            doc["device_class"] = deviceClass;
        }
        if (command.length())
        {
            doc["command_topic"] = base + "/" + command;
            doc["retain"] = false;
        }
        if (value.length())
        {
            doc["state_topic"] = base + "/state";
            doc["value_template"] = value;
        }
        if (!std::strcmp(platform, "sensor") || key.startsWith("blocked_"))
        {
            doc["entity_category"] = "diagnostic";
        }
        if (key == "uptime")
        {
            doc["unit_of_measurement"] = "s";
        }
        if (key == "identify")
        {
            doc["entity_category"] = "diagnostic";
        }
        if (key == "gesture")
        {
            doc["state_topic"] = base + "/event";
            auto types = doc["event_types"].to<JsonArray>();
            for (auto type : {"single", "double", "triple"})
            {
                types.add(type);
            }
        }
        String payload;
        serializeJson(doc, payload);
        ok = publish(topic, payload) && ok;
    };
    for (int i = 0; i < 6; ++i)
    {
        String n = String(i + 1), index = String(i);
        entity("switch", "relay_" + n, device.config.relays[i].name, "{{ 'ON' if value_json.relays[" + index + "].on else 'OFF' }}",
               "relay/" + n + "/set");
        entity("binary_sensor", "blocked_" + n, "Channel " + n + " blocked",
               "{{ 'ON' if value_json.relays[" + index + "].blocked or not value_json.relays[" + index + "].enabled else 'OFF' }}");
    }
    entity("sensor", "uptime", "Uptime", "{{ value_json.uptime }}", "", "duration");
    entity("sensor", "mode", "Protocol", "{{ value_json.mode }}");
    entity("sensor", "protocol", "Protocol status", "{{ value_json.protocolState }}");
    entity("binary_sensor", "fault", "Fault", "{{ 'ON' if value_json.fault else 'OFF' }}", "", "problem");
    entity("binary_sensor", "pressed", "Button pressed", "{{ 'ON' if value_json.pressed else 'OFF' }}");
    entity("button", "identify", "Identify", "", "identify/set", "identify");
    entity("event", "gesture", "Button gesture", "", "", "button");
    return ok;
}

void HomeAssistant::state()
{
    JsonDocument doc;
    device.snapshot(doc.to<JsonObject>());
    // Audit entries belong to the authenticated UI, not MQTT telemetry.
    doc.remove("audit");
    String payload;
    serializeJson(doc, payload);
    publish(base + "/state", payload);
}

void HomeAssistant::event(uint8_t gesture)
{
    if (!accepting || gesture < 1 || gesture > 3 || !device.config.button)
    {
        return;
    }
    const char *names[] = {"single", "double", "triple"};
    publish(base + "/event", String("{\"event_type\":\"") + names[gesture - 1] + "\"}", false);
}

void HomeAssistant::loop(uint32_t now)
{
    if (!commands)
    {
        return;
    }
    // Read the service under its own lock; callbacks never acquire the actuator lock.
    bool wanted = false;
    settings.read([&](MqttSettings &s) { wanted = s.enabled && s.homeAssistantDiscovery; });
    bool connected = mqtt.connected();
    accepting = wanted && connected;
    int command;
    if (wanted != enabled || !connected)
    {
        xQueueReset(commands);
    }
    if (xQueueReceive(commands, &command, 0) == pdTRUE && accepting)
    {
        if (command == 12)
        {
            device.identify();
        }
        else
        {
            device.relay(command / 2, command % 2 != 0, "homeassistant");
        }
        state();
    }
    bool wasEnabled = enabled;
    bool changed = wanted != enabled || connected != wasConnected;
    enabled = wanted;
    wasConnected = connected;
    if (!connected || (!changed && uint32_t(now - lastSync) < 1000))
    {
        return;
    }
    lastSync = now;
    bool resync = refresh.exchange(false) || changed;
    if (!enabled)
    {
        bool hadDiscovery = cleanupPending || wasEnabled;
        if (cleanupPending && discovery(true) && publish(base + "/state", ""))
        {
            device.fs.remove(Marker);
            cleanupPending = false;
        }
        if (changed && hadDiscovery)
        {
            mqtt.unsubscribe(subscription.c_str());
            mqtt.unsubscribe((base + "/identify/set").c_str());
            mqtt.unsubscribe("homeassistant/status");
        }
        discoverySignature = "";
        return;
    }
    if (resync)
    {
        if (mqtt.subscribe(subscription.c_str(), 1) < 0 || mqtt.subscribe((base + "/identify/set").c_str(), 1) < 0 ||
            mqtt.subscribe("homeassistant/status", 1) < 0)
        {
            refresh = true;
        }
    }
    String signature = settings.getStatusTopic();
    for (const auto &relay : device.config.relays)
    {
        signature += "|" + String(relay.name.length()) + ":" + relay.name;
    }
    if (resync || signature != discoverySignature)
    {
        if (!cleanupPending)
        {
            File marker = device.fs.open(Marker, "w");
            if (marker)
            {
                marker.print("1");
                marker.close();
            }
            cleanupPending = true;
        }
        if (discovery(false))
        {
            discoverySignature = signature;
        }
        else
        {
            refresh = true;
        }
    }
    state();
}
} // namespace actuator
#endif
