#ifndef SecuritySettingsService_h
#define SecuritySettingsService_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <FSPersistence.h>
#include <Features.h>
#include <HttpEndpoint.h>
#include <SecurityManager.h>
#include <SettingValue.h>

#ifndef FACTORY_JWT_SECRET
#define FACTORY_JWT_SECRET "#{random}-#{random}"
#endif

#ifndef FACTORY_ADMIN_USERNAME
#define FACTORY_ADMIN_USERNAME "admin"
#endif

#ifndef FACTORY_ADMIN_PASSWORD
#define FACTORY_ADMIN_PASSWORD "admin"
#endif

#ifndef FACTORY_GUEST_USERNAME
#define FACTORY_GUEST_USERNAME "guest"
#endif

#ifndef FACTORY_GUEST_PASSWORD
#define FACTORY_GUEST_PASSWORD "guest"
#endif

#define SECURITY_SETTINGS_FILE "/config/securitySettings.json"
#define SECURITY_SETTINGS_PATH "/rest/securitySettings"

#define GENERATE_TOKEN_PATH "/rest/generateToken"

#include "Password.h"
#include "SetupIdentity.h"

#if FT_ENABLED(FT_SECURITY)

class SecuritySettings
{
    public:
        String jwtSecret;
        std::list<User> users;

        static void read(SecuritySettings &settings, JsonObject &root)
        {
            // secret
            root["jwt_secret"] = settings.jwtSecret;

            // users
            JsonArray users = root["users"].to<JsonArray>();
            for (User user : settings.users)
            {
                JsonObject userRoot = users.add<JsonObject>();
                userRoot["username"] = user.username;
                userRoot["password"] = user.password;
                userRoot["admin"] = user.admin;
                userRoot["role"] = user.role;
                userRoot["channels"] = user.channels;
            }
        }

        static void publicRead(SecuritySettings &settings, JsonObject &root)
        {
            read(settings, root);
            root["jwt_secret"] = "";
            for (JsonObject user : root["users"].as<JsonArray>())
            {
                user["password"] = "";
            }
        }

        static StateUpdateResult update(JsonObject &root, SecuritySettings &settings, const String &originID)
        {
            bool loading = originID.startsWith("/config/");
            SecuritySettings candidate;
            candidate.jwtSecret = settings.jwtSecret;
            if (root["jwt_secret"].is<String>() && root["jwt_secret"].as<String>().length())
            {
                candidate.jwtSecret = root["jwt_secret"].as<String>();
            }
            if (candidate.jwtSecret.isEmpty())
            {
                candidate.jwtSecret = SettingValue::format(FACTORY_JWT_SECRET);
            }
            if (!root["users"].is<JsonArray>())
            {
                if (!loading && !settings.users.empty())
                {
                    return StateUpdateResult::ERROR;
                }
#ifdef ACTUATOR_BOARD
                String setupPassword = SetupIdentity::password();
                if (setupPassword.isEmpty())
                {
                    return StateUpdateResult::ERROR;
                }
                candidate.users.push_back(User(FACTORY_ADMIN_USERNAME, Password::hash(setupPassword), true));
#else
                candidate.users.push_back(User(FACTORY_ADMIN_USERNAME, Password::hash(FACTORY_ADMIN_PASSWORD), true));
                candidate.users.push_back(User(FACTORY_GUEST_USERNAME, Password::hash(FACTORY_GUEST_PASSWORD), false));
#endif
            }
            else
            {
                if (root["users"].size() > 16)
                {
                    return StateUpdateResult::ERROR;
                }
                bool hasAdmin = false;
                for (JsonObject user : root["users"].as<JsonArray>())
                {
                    String username = user["username"] | "", password = user["password"] | "", role = user["role"] | "viewer";
                    int channels = user["channels"] | 63;
                    bool admin = user["admin"] | false;
                    if (username.length() < 3 || username.length() > 32 || channels < 0 || channels > 63 ||
                        (role != "viewer" && role != "operator" && role != "installer" && role != "administrator"))
                    {
                        return StateUpdateResult::ERROR;
                    }
                    for (auto &existing : candidate.users)
                    {
                        if (existing.username == username)
                        {
                            return StateUpdateResult::ERROR;
                        }
                    }
                    if (!loading && password.isEmpty())
                    {
                        for (auto &existing : settings.users)
                        {
                            if (existing.username == username)
                            {
                                password = existing.password;
                            }
                        }
                    }
                    else if (!loading || !password.startsWith("pbkdf2$"))
                    {
                        password = Password::hash(password);
                    }
                    if (password.isEmpty())
                    {
                        return StateUpdateResult::ERROR;
                    }
                    candidate.users.push_back(User(username, password, admin, admin ? "administrator" : role, channels));
                    hasAdmin |= admin;
                }
                if (!hasAdmin)
                {
                    return StateUpdateResult::ERROR;
                }
            }
            // Rotate signing secret on every administrative edit, revoking old sessions.
            if (!loading && !settings.users.empty())
            {
                candidate.jwtSecret = SettingValue::format("#{random}-#{random}");
            }
            settings = candidate;
            return StateUpdateResult::CHANGED;
        }
};

class SecuritySettingsService: public StatefulService<SecuritySettings>, public SecurityManager
{
    public:
        SecuritySettingsService(PsychicHttpServer *server, FS *fs);

        void begin();

        // Functions to implement SecurityManager
        Authentication authenticate(const String &username, const String &password);
        Authentication authenticateRequest(PsychicRequest *request);
        String generateJWT(User *user);

        PsychicRequestFilterFunction filterRequest(AuthenticationPredicate predicate);
        PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate predicate);
        PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate predicate);

    private:
        PsychicHttpServer *_server;

        HttpEndpoint<SecuritySettings> _httpEndpoint;
        FSPersistence<SecuritySettings> _fsPersistence;
        ArduinoJsonJWT _jwtHandler;

        esp_err_t generateToken(PsychicRequest *request);

        void configureJWTHandler();

        /*
         * Lookup the user by JWT
         */
        Authentication authenticateJWT(String &jwt);

        /*
         * Verify the payload is correct
         */
        boolean validatePayload(JsonObject &parsedPayload, User *user);
};

#else

class SecuritySettingsService: public SecurityManager
{
    public:
        SecuritySettingsService(PsychicHttpServer *server, FS *fs);
        ~SecuritySettingsService();

        // minimal set of functions to support framework with security settings disabled
        Authentication authenticateRequest(PsychicRequest *request);
        PsychicRequestFilterFunction filterRequest(AuthenticationPredicate predicate);
        PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate predicate);
        PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate predicate);
};

#endif // end FT_ENABLED(FT_SECURITY)
#endif // end SecuritySettingsService_h
