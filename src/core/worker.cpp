#include "core/worker.h"

#include "cdp/client.h"
#include "core/logger.h"
#include "core/settings.h"
#include "core/util.h"
#include "core/version.h"
#include "hud/payload.h"

#include <string>

namespace {

HANDLE g_stop = nullptr;

// Returns false when a shutdown was requested during the wait.
bool sleepOrStop(DWORD ms)
{
    if (!g_stop) { Sleep(ms); return true; }
    return WaitForSingleObject(g_stop, ms) == WAIT_TIMEOUT;
}

// Runtime.evaluate answers with {"type":...,"value":...}; this reads the value out of it.
bool resultIsTrue(const json::Value& result)
{
    const json::Value* value = result.member("value");
    return value && value->isBool() && value->boolean;
}

std::string resultText(const json::Value& result)
{
    const json::Value* value = result.member("value");
    if (value && value->isString()) return value->string;
    return "ok";
}

}  // namespace

bool createStopEvent()
{
    if (g_stop) return true;
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    return g_stop != nullptr;
}

void requestStop()
{
    if (g_stop) SetEvent(g_stop);
}

DWORD WINAPI HudWorker(LPVOID)
{
    logging::init();
    writeDefaultSettingsIfMissing();
    loadSettings();

    if (g_set.off) {
        LOG_INFO("off = yes in the settings file, so nothing will be drawn.");
        logging::shutdown();
        return 0;
    }

    LOG_INFO("gtaw-oldhud %s waiting for the interface on 127.0.0.1:%d",
             GTAW_OLDHUD_VERSION, g_set.port);

    cdp::Client client;
    bool        installed = false;

    // The interface is not up for the first stretch of every launch, so the same "not ready yet"
    // reason would otherwise be written every two seconds for minutes. Only changes are logged.
    std::string lastQuietReason;

    for (;;) {
        DWORD delay = (DWORD)g_set.heartbeatMs;

        if (!client.isConnected()) {
            std::string err;
            if (client.connect(g_set.port, err)) {
                LOG_INFO("Attached to %s", client.frameUrl().c_str());
                lastQuietReason.clear();
                installed = false;
            } else {
                if (err != lastQuietReason) {
                    LOG_INFO("Waiting: %s", err.c_str());
                    lastQuietReason = err;
                }
                installed = false;
                delay = (DWORD)g_set.reconnectMs;
            }
        }

        if (client.isConnected() && !installed) {
            std::string  err;
            json::Value  result;
            if (client.evaluate(hud::buildInstallScript(), result, err)) {
                LOG_INFO("Old HUD installed (%s)", resultText(result).c_str());
                installed = true;
            } else {
                LOG_WARN("Could not install the old HUD: %s", err.c_str());
                client.close();
                delay = (DWORD)g_set.reconnectMs;
            }
        } else if (client.isConnected()) {
            std::string  err;
            json::Value  result;
            if (!client.evaluate(hud::buildProbeScript(), result, err)) {
                // Usually the document was replaced: a reconnect, a resource restart, or the
                // interface reloading. The private world went with it, so start again.
                LOG_INFO("Lost the interface (%s); reattaching", err.c_str());
                client.close();
                installed = false;
                delay = (DWORD)g_set.reconnectMs;
            } else if (!resultIsTrue(result)) {
                LOG_INFO("The injected HUD is no longer on the page; reinstalling");
                installed = false;
            }
        }

        if (!sleepOrStop(delay)) break;
    }

    client.close();
    LOG_INFO("Stopping.");
    logging::shutdown();
    return 0;
}
