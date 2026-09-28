#pragma once
#include <ESP32SvelteKit.h>
#include <atomic>

namespace actuator
{
class Actuator;
#if FT_ENABLED(FT_MQTT)
class HomeAssistant
{
  public:
    HomeAssistant(Actuator &device, ESP32SvelteKit &framework);
    void begin();
    void loop(uint32_t now);
    void event(uint8_t gesture);

  private:
    Actuator &device;
    MqttSettingsService &settings;
    PsychicMqttClient &mqtt;
    String id, base, subscription, discoverySignature;
    QueueHandle_t commands = nullptr;
    std::atomic<bool> refresh {true}, accepting {false};
    bool enabled = false, wasConnected = false, cleanupPending = false;
    uint32_t lastSync = 0;
    bool publish(const String &topic, const String &payload, bool retain = true);
    bool discovery(bool remove);
    void state();
};
#endif
} // namespace actuator
