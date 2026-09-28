#include "device/HomeAssistant.cpp"
#include <cassert>
#include <iostream>

int main()
{
    actuator::Actuator device;
    ESP32SvelteKit framework;
    auto &mqtt = framework.mqtt;
    auto &settings = framework.settings.value;
    actuator::HomeAssistant ha(device, framework);
    ha.begin();
    ha.loop(1000);
    assert(mqtt.messages.empty()); // Existing installations have no HA traffic.
    settings.enabled = settings.homeAssistantDiscovery = true;
    ha.loop(2000);
    assert(mqtt.messages.size() == 20); // 19 discovered entities and one snapshot.
    assert(mqtt.subscriptions.size() == 3);
    JsonDocument doc;
    assert(!deserializeJson(doc, mqtt.messages[0].payload));
    assert(doc["availability_mode"] == "all");
    assert(doc["availability"].size() == 2);
    assert(doc["command_topic"] == "edge_switch/edge_001122334455/relay/1/set");
    assert(doc["retain"] == false);
    for (const auto &m : mqtt.messages)
    {
        assert(m.retain && m.qos == 1);
        assert(!deserializeJson(doc, m.payload));
        if (m.topic.find("/config") != String::npos)
        {
            assert(doc["unique_id"].is<const char *>());
            assert(doc["device"]["identifiers"][0] == "edge_001122334455");
        }
    }
    const String command = "edge_switch/edge_001122334455/relay/1/set";
    mqtt.receive(command, "ON", 1);
    ha.loop(2001);
    assert(device.writes == 0);
    mqtt.receive(command, "ON");
    assert(device.writes == 0); // Callback only queues; actuator task owns hardware.
    ha.loop(2002);
    assert(device.outputs[0] && device.source == "homeassistant");
    device.blocked[0] = true;
    mqtt.receive(command, "OFF");
    ha.loop(2003);
    assert(device.outputs[0] && device.writes == 1);
    assert(!deserializeJson(doc, mqtt.messages.back().payload));
    assert(doc["audit"].isNull());
    mqtt.receive("edge_switch/other/relay/1/set", "ON");
    mqtt.receive(command, "TOGGLE");
    ha.loop(2004);
    assert(device.writes == 1);
    ha.event(2);
    assert(!mqtt.messages.back().retain);
    assert(mqtt.messages.back().payload == "{\"event_type\":\"double\"}");
    const auto count = mqtt.messages.size();
    ha.event(4);
    device.config.button = false;
    ha.event(1);
    assert(mqtt.messages.size() == count);
    mqtt.receive("homeassistant/status", "online");
    ha.loop(3000);
    assert(mqtt.messages.size() == count + 20);
    device.config.relays[0].name = "Kitchen";
    ha.loop(4000);
    assert(mqtt.messages[mqtt.messages.size() - 20].payload.find("Kitchen") != String::npos);
    // Commands pending at disable/disconnect never survive into a later session.
    mqtt.receive(command, "OFF");
    settings.homeAssistantDiscovery = false;
    ha.loop(4001);
    assert(device.writes == 1);
    for (size_t i = mqtt.messages.size() - 20; i < mqtt.messages.size(); ++i)
    {
        assert(mqtt.messages[i].payload.empty() && mqtt.messages[i].retain);
    }
    settings.homeAssistantDiscovery = true;
    mqtt.fail = true;
    auto failedCount = mqtt.messages.size();
    ha.loop(5000);
    assert(mqtt.messages.size() == failedCount);
    mqtt.fail = false;
    ha.loop(6000);
    assert(mqtt.messages.size() == failedCount + 20); // Retry rejected enqueue.
    mqtt.online = false;
    mqtt.disconnectCallback(false);
    ha.loop(6001);
    mqtt.receive(command, "ON");
    mqtt.online = true;
    mqtt.connectCallback(false);
    ha.loop(7000);
    assert(device.writes == 1);
    assert(mqtt.messages.size() == failedCount + 40);
    mqtt.receive("edge_switch/edge_001122334455/identify/set", "PRESS");
    ha.loop(7001);
    assert(device.identified == 1);
    mqtt.receive("edge_switch/edge_001122334455/identify/set", "PRESS");
    mqtt.disconnectCallback(false);
    mqtt.connectCallback(false); // A full reconnect can occur between actuator ticks.
    ha.loop(7002);
    assert(device.identified == 1);
    device.config.relays[1].enabled = false;
    mqtt.receive("edge_switch/edge_001122334455/relay/2/set", "ON");
    ha.loop(7003);
    assert(!device.outputs[1] && device.writes == 1);
    // A marker persists cleanup across a reboot with discovery disabled.
    assert(device.fs.exists("/config/home-assistant-discovery"));
    ESP32SvelteKit rebooted;
    rebooted.settings.value.enabled = true;
    actuator::HomeAssistant afterReboot(device, rebooted);
    afterReboot.begin();
    afterReboot.loop(1000);
    assert(rebooted.mqtt.messages.size() == 20);
    for (const auto &message : rebooted.mqtt.messages)
    {
        assert(message.payload.empty() && message.retain);
    }
    assert(!device.fs.exists("/config/home-assistant-discovery"));
    std::cout << "PASS: Home Assistant discovery, state, commands, interlocks, events, cleanup and reconnect\n";
}
