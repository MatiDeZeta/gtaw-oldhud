#include "core/settings.h"

#include "core/logger.h"
#include "core/util.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

Settings g_set;

namespace {

const wchar_t* kSettingsLeaf = L"gtaw-oldhud.settings.txt";

const char* kDefaultFile =
    "# gtaw-oldhud settings\r\n"
    "#\r\n"
    "# Lines starting with # are ignored. Change the value after the = and restart FiveM.\r\n"
    "# Delete this file to get it back with every option at its default.\r\n"
    "#\r\n"
    "# Colours must be written as #rgb or #rrggbb; anything else is ignored and the default is\r\n"
    "# kept. The defaults are GTA's own HUD palette, which is what the old HUD drew in.\r\n"
    "\r\n"
    "# Set to yes to disable the plugin without removing the file.\r\n"
    "off = no\r\n"
    "\r\n"
    "# FiveM's local CEF debugging port. Only change this if you know it moved.\r\n"
    "port = 13172\r\n"
    "\r\n"
    "# ---- which parts of the old HUD to draw -------------------------------------------\r\n"
    "show_cash      = yes\r\n"
    "show_bank      = yes\r\n"
    "show_location  = yes\r\n"
    "show_speed     = yes\r\n"
    "show_fuel      = yes\r\n"
    "show_odometer  = yes\r\n"
    "\r\n"
    "# The ALT / HDG / ATC line, drawn only in a plane or a helicopter.\r\n"
    "show_aviation  = yes\r\n"
    "\r\n"
    "# The Admin-Duty and Tester-Duty lines under the balances, and the ELS light and siren\r\n"
    "# state in an emergency vehicle.\r\n"
    "show_staff     = yes\r\n"
    "show_els       = yes\r\n"
    "\r\n"
    "# How long the ELS state stays up after it changes, in milliseconds. The old HUD showed it\r\n"
    "# for five seconds and then hid it until the next change. 0 keeps it up the whole time.\r\n"
    "els_hold_ms    = 5000\r\n"
    "\r\n"
    "show_footer    = yes\r\n"
    "show_players   = yes\r\n"
    "show_time      = yes\r\n"
    "\r\n"
    "# ---- which parts of the current HUD to make invisible -------------------------------\r\n"
    "# Comma separated, any of: cash, bank, location, compass, brand, speedo, weather, tips,\r\n"
    "# minimapbar, speedsign. Use \"none\" to leave the current HUD alone (both HUDs then draw at\r\n"
    "# once, which is useful for lining things up). The last four are parts of the current HUD\r\n"
    "# the old one had no equivalent of at all: the weather bar; the row of status icons beside\r\n"
    "# the balances (a new notification, bleeding, drugged, and the staff duty badge); the\r\n"
    "# minimap tab strip; and the speed limit sign. Take \"tips\" out of the list to keep those\r\n"
    "# status icons.\r\n"
    "#\r\n"
    "# These are made transparent, not removed. Leave GTAW's own /settings toggles switched ON:\r\n"
    "# turning a widget off in /settings stops it being rendered, and it is not established that\r\n"
    "# the game keeps sending a field whose widget is off. Hiding it here keeps the data coming.\r\n"
    "# The compass is also what the plugin measures to find the minimap before the game sends\r\n"
    "# the minimap rectangle, so removing it from this list does not stop it being read.\r\n"
    "hide_new = cash,bank,location,compass,brand,speedo,weather,tips,minimapbar,speedsign\r\n"
    "\r\n"
    "# ---- the bottom line ----------------------------------------------------------------\r\n"
    "# Reads: <server> - <gamemode> v<version> - <players>/<max_players> - <clock>, with the\r\n"
    "# separators drawn as em dashes. Everything but the two names comes from the game.\r\n"
    "server_name   = GTA.WORLD\r\n"
    "gamemode_name = Roleplay\r\n"
    "max_players   = 1500\r\n"
    "\r\n"
    "# ---- the fuel bar ---------------------------------------------------------------------\r\n"
    "# The word in front of the bars. The old HUD took it from the server, which sent \"Fuel\"\r\n"
    "# for a tank and the fuel type for anything else.\r\n"
    "fuel_label = Fuel\r\n"
    "\r\n"
    "# ---- the font --------------------------------------------------------------------------\r\n"
    "# The old HUD was drawn in the game's font 4, Chalet Comprime Cologne Sixty. That face is\r\n"
    "# licensed and is not shipped with this plugin. Three things are tried, in order:\r\n"
    "#\r\n"
    "#   1. the file named here, if it is next to the .asi  (.ttf .otf .woff .woff2)\r\n"
    "#   2. Chalet Comprime Cologne Sixty, if it is installed on this machine\r\n"
    "#   3. the condensed face built into the plugin\r\n"
    "#\r\n"
    "# So to get the original face exactly, put your own copy of it next to the .asi under this\r\n"
    "# name. Whichever is used, the text is measured at run time and corrected so it has the old\r\n"
    "# HUD's proportions. Leave empty to skip step 1.\r\n"
    "font_file = gtaw-oldhud.font.ttf\r\n"
    "\r\n"
    "# ---- units ---------------------------------------------------------------------------\r\n"
    "# imperial (MPH and mi.), metric (KMH and km.), or auto to follow the in-game speedometer.\r\n"
    "units = imperial\r\n"
    "\r\n"
    "# ---- size and placement ----------------------------------------------------------------\r\n"
    "# The old HUD's own geometry is built in, so there is only one size control. font_scale\r\n"
    "# multiplies every text scale; 1.0 is the size the old HUD drew at.\r\n"
    "font_scale = 1.0\r\n"
    "\r\n"
    "# How heavy to draw it. The face built into the plugin is Oswald, a variable font whose\r\n"
    "# weight axis runs from 200 to 700, so anything in that range is a real instance of it and\r\n"
    "# not a synthesised one: 200 extra light, 300 light, 400 regular, 500 medium, 600 semi\r\n"
    "# bold, 700 bold. 400 is what the old HUD looked closest to. Widths are re-measured\r\n"
    "# whenever this changes, so a heavier weight does not come out wider than the original.\r\n"
    "font_weight = 400\r\n"
    "\r\n"
    "# Nudges for the whole block beside the minimap, as a fraction of the screen: 0.01 is one\r\n"
    "# percent of the width or height. Positive x moves it right, positive y moves it down. The\r\n"
    "# old HUD had the same pair, for players whose minimap was not where the safe zone said.\r\n"
    "# Everything else is placed with the game's own /hudlayout: while it is open the old HUD's\r\n"
    "# blocks can be dragged, scaled with the wheel and hidden with their cross, the same as the\r\n"
    "# current widgets. The result is kept in gtaw-oldhud.layout.txt next to the plugin.\r\n"
    "dock_x = 0.0\r\n"
    "dock_y = 0.0\r\n"
    "\r\n"
    "# ---- colour ----------------------------------------------------------------------------\r\n"
    "# text_color is everything not listed separately. blue is the speed, altitude and heading\r\n"
    "# numbers; yellow is the gamemode and version; red, orange and green are the fuel bars, in\r\n"
    "# that order, and green is also ATC ONLINE.\r\n"
    "text_color    = #ffffff\r\n"
    "cash_color    = #73ba83\r\n"
    "bank_color    = #ffffff\r\n"
    "blue_color    = #5db6e5\r\n"
    "yellow_color  = #f0c850\r\n"
    "red_color     = #e03232\r\n"
    "orange_color  = #ff8555\r\n"
    "green_color   = #72cc72\r\n"
    "admin_color   = #1c8000\r\n"
    "tester_color  = #731010\r\n"
    "outline_color = #000000\r\n"
    "outline_width = 1.0\r\n"
    "\r\n"
    "# How opaque the whole thing is. The game drew this text at an alpha of 200 out of 255.\r\n"
    "text_alpha = 0.784\r\n"
    "\r\n"
    "# ---- timing --------------------------------------------------------------------------------\r\n"
    "# How long to wait before retrying a failed connection, and how often to check that the\r\n"
    "# injected HUD is still alive. Both in milliseconds.\r\n"
    "reconnect_ms = 2000\r\n"
    "heartbeat_ms = 4000\r\n";

// Settings text ends up as a JSON string inside an injected script and is written to the page
// with textContent, so it cannot become markup. Control characters are still dropped: they have
// no business in a server name and they make the log unreadable.
std::string sanitizeText(const std::string& raw, size_t maxLen)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (c < 0x20 || c == 0x7f) continue;
        out.push_back(char(c));
        if (out.size() >= maxLen) break;
    }
    return util::trim(out);
}

void applyColor(const std::string& key, const std::string& value, std::string& target)
{
    if (util::isHexColor(value)) target = value;
    else LOG_WARN("%s: '%s' is not a #rgb or #rrggbb colour; keeping %s",
                  key.c_str(), value.c_str(), target.c_str());
}

std::string readFileUtf8(const std::wstring& path, bool& ok)
{
    ok = false;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::string();

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart > 256 * 1024) { CloseHandle(h); return std::string(); }

    std::string buf(size_t(size.QuadPart), '\0');
    DWORD read = 0;
    if (!buf.empty() && !ReadFile(h, &buf[0], DWORD(buf.size()), &read, nullptr)) {
        CloseHandle(h);
        return std::string();
    }
    CloseHandle(h);
    buf.resize(read);

    // Strip a UTF-8 BOM if an editor added one.
    if (buf.size() >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF)
        buf.erase(0, 3);

    ok = true;
    return buf;
}

}  // namespace

void loadSettings()
{
    std::wstring path = util::modulePath(kSettingsLeaf);
    if (path.empty()) return;

    bool ok = false;
    std::string text = readFileUtf8(path, ok);
    if (!ok) return;

    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;

        line = util::trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = util::lower(util::trim(line.substr(0, eq)));
        std::string val = util::trim(line.substr(eq + 1));

        if      (key == "off")           g_set.off         = util::parseBool(val, g_set.off);
        else if (key == "port")          g_set.port        = util::parseInt(val, g_set.port, 1, 65535);

        else if (key == "show_cash")     g_set.showCash     = util::parseBool(val, g_set.showCash);
        else if (key == "show_bank")     g_set.showBank     = util::parseBool(val, g_set.showBank);
        else if (key == "show_location") g_set.showLocation = util::parseBool(val, g_set.showLocation);
        else if (key == "show_speed")    g_set.showSpeed    = util::parseBool(val, g_set.showSpeed);
        else if (key == "show_fuel")     g_set.showFuel     = util::parseBool(val, g_set.showFuel);
        else if (key == "show_odometer") g_set.showOdometer = util::parseBool(val, g_set.showOdometer);
        else if (key == "show_aviation") g_set.showAviation = util::parseBool(val, g_set.showAviation);
        else if (key == "show_staff")    g_set.showStaff    = util::parseBool(val, g_set.showStaff);
        else if (key == "show_els")      g_set.showEls      = util::parseBool(val, g_set.showEls);
        else if (key == "show_footer")   g_set.showFooter   = util::parseBool(val, g_set.showFooter);
        else if (key == "show_players")  g_set.showPlayers  = util::parseBool(val, g_set.showPlayers);
        else if (key == "show_time")     g_set.showTime     = util::parseBool(val, g_set.showTime);

        else if (key == "hide_new")      g_set.hideNew      = util::lower(sanitizeText(val, 200));

        else if (key == "server_name")   g_set.serverName   = sanitizeText(val, 64);
        else if (key == "gamemode_name") g_set.gamemodeName = sanitizeText(val, 64);
        else if (key == "max_players")   g_set.maxPlayers   = util::parseInt(val, g_set.maxPlayers, 0, 100000);
        else if (key == "fuel_label")    g_set.fuelLabel    = sanitizeText(val, 32);
        else if (key == "els_hold_ms")   g_set.elsHoldMs    = util::parseInt(val, g_set.elsHoldMs, 0, 600000);
        else if (key == "font_file")     g_set.fontFile     = sanitizeText(val, 120);

        else if (key == "units") {
            std::string u = util::lower(val);
            if (u == "imperial" || u == "metric" || u == "auto") g_set.units = u;
            else LOG_WARN("units: '%s' is not imperial, metric or auto; keeping %s",
                          val.c_str(), g_set.units.c_str());
        }

        else if (key == "font_scale")    g_set.fontScale    = util::parseDouble(val, g_set.fontScale, 0.2, 5.0);
        else if (key == "font_weight")   g_set.fontWeight   = util::parseInt(val, g_set.fontWeight, 100, 900);
        else if (key == "dock_x")        g_set.dockX        = util::parseDouble(val, g_set.dockX, -1.0, 1.0);
        else if (key == "dock_y")        g_set.dockY        = util::parseDouble(val, g_set.dockY, -1.0, 1.0);

        else if (key == "text_color")    applyColor(key, val, g_set.textColor);
        else if (key == "cash_color")    applyColor(key, val, g_set.cashColor);
        else if (key == "bank_color")    applyColor(key, val, g_set.bankColor);
        else if (key == "blue_color")    applyColor(key, val, g_set.blueColor);
        else if (key == "yellow_color")  applyColor(key, val, g_set.yellowColor);
        else if (key == "red_color")     applyColor(key, val, g_set.redColor);
        else if (key == "orange_color")  applyColor(key, val, g_set.orangeColor);
        else if (key == "green_color")   applyColor(key, val, g_set.greenColor);
        else if (key == "admin_color")   applyColor(key, val, g_set.adminColor);
        else if (key == "tester_color")  applyColor(key, val, g_set.testerColor);
        else if (key == "outline_color") applyColor(key, val, g_set.outlineColor);
        else if (key == "outline_width") g_set.outlineWidth = util::parseDouble(val, g_set.outlineWidth, 0.0, 8.0);
        else if (key == "text_alpha")    g_set.textAlpha    = util::parseDouble(val, g_set.textAlpha, 0.05, 1.0);

        else if (key == "reconnect_ms")  g_set.reconnectMs  = util::parseInt(val, g_set.reconnectMs, 250, 120000);
        else if (key == "heartbeat_ms")  g_set.heartbeatMs  = util::parseInt(val, g_set.heartbeatMs, 500, 120000);

        else LOG_WARN("Unknown setting '%s' ignored", key.c_str());
    }

    if (g_set.serverName.empty())   g_set.serverName   = "GTA.WORLD";
    if (g_set.gamemodeName.empty()) g_set.gamemodeName = "Roleplay";
    if (g_set.fuelLabel.empty())    g_set.fuelLabel    = "Fuel";
}

void writeDefaultSettingsIfMissing()
{
    std::wstring path = util::modulePath(kSettingsLeaf);
    if (path.empty()) return;
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return;

    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        LOG_WARN("Could not create %ls (err %lu)", kSettingsLeaf, GetLastError());
        return;
    }

    DWORD written = 0;
    DWORD len = DWORD(strlen(kDefaultFile));
    WriteFile(h, kDefaultFile, len, &written, nullptr);
    CloseHandle(h);

    LOG_INFO("Wrote default settings file %ls", kSettingsLeaf);
}
