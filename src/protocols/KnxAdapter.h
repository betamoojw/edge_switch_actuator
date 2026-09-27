#pragma once
#include "device/Actuator.h"

namespace actuator
{
class KnxAdapter
{
    public:
        explicit KnxAdapter(Actuator &d);
        ~KnxAdapter();
        bool begin();
        void stop();
        void loop();
        void programming(bool active);
        bool isProgramming() const;
        bool configured() const;
        void status(uint8_t channel, bool value);
        void snapshot(JsonObject o);
        bool configure(JsonObjectConst o, String &error);
        void erase();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
};
} // namespace actuator
