#include "core/settings.h"
#include "core/util.h"

#include <cstdio>
#include <string>
#include <vector>

extern std::wstring g_stubModulePath;

// Warnings the parser emitted during the last load, so a test can assert that a file it expects
// to be clean really is.
static std::vector<std::string> g_warnings;

namespace logging {
void init() {}
void shutdown() {}
void write(const char* level, const char* fmt, ...)
{
    if (std::string(level) != "WARN") return;
    char body[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    g_warnings.push_back(body);
}
}  // namespace logging

static int checks = 0, failures = 0;

static void eq(const char* label, const std::string& got, const std::string& want)
{
    ++checks;
    if (got != want) { ++failures; printf("  FAIL %s\n       got:      %s\n       expected: %s\n", label, got.c_str(), want.c_str()); }
    else printf("  ok   %s  =  %s\n", label, got.c_str());
}
static void eq(const char* label, double got, double want)
{
    ++checks;
    if (got != want) { ++failures; printf("  FAIL %s\n       got:      %g\n       expected: %g\n", label, got, want); }
    else printf("  ok   %s  =  %g\n", label, got);
}
static void ok(const char* label, bool cond)
{
    ++checks;
    if (!cond) { ++failures; printf("  FAIL %s\n", label); }
    else printf("  ok   %s\n", label);
}

static std::string dir;

static void writeSettings(const std::string& text)
{
    FILE* f = std::fopen((dir + "/gtaw-oldhud.settings.txt").c_str(), "wb");
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

static void removeSettings() { std::remove((dir + "/gtaw-oldhud.settings.txt").c_str()); }

static std::string readSettings()
{
    FILE* f = std::fopen((dir + "/gtaw-oldhud.settings.txt").c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

static void reload()
{
    g_set = Settings();
    g_warnings.clear();
    loadSettings();
}

int main(int argc, char** argv)
{
    dir = (argc > 1) ? argv[1] : ".";
    std::wstring wdir = util::widen(dir);
    g_stubModulePath = wdir + L"\\gtaw-oldhud.asi";

    printf("\n== the default file this plugin writes is understood by its own parser ==\n");
    removeSettings();
    writeDefaultSettingsIfMissing();
    std::string defaults = readSettings();
    ok("a default file was written", !defaults.empty());

    reload();
    if (!g_warnings.empty())
        for (const std::string& w : g_warnings) printf("       warning: %s\n", w.c_str());
    ok("it parses with no warnings", g_warnings.empty());

    // Every value in the shipped file must equal the struct default, or the file is lying about
    // what the plugin will do.
    Settings fresh;
    eq("off matches",           g_set.off ? "yes" : "no", fresh.off ? "yes" : "no");
    eq("port matches",          double(g_set.port), double(fresh.port));
    eq("hide_new matches",      g_set.hideNew, fresh.hideNew);
    eq("server_name matches",   g_set.serverName, fresh.serverName);
    eq("gamemode_name matches", g_set.gamemodeName, fresh.gamemodeName);
    eq("max_players matches",   double(g_set.maxPlayers), double(fresh.maxPlayers));
    eq("units matches",         g_set.units, fresh.units);
    eq("fuel_label matches",    g_set.fuelLabel, fresh.fuelLabel);
    eq("font_scale matches",    g_set.fontScale, fresh.fontScale);
    eq("cash_color matches",    g_set.cashColor, fresh.cashColor);
    eq("blue_color matches",    g_set.blueColor, fresh.blueColor);
    eq("yellow_color matches",  g_set.yellowColor, fresh.yellowColor);
    eq("outline_width matches", g_set.outlineWidth, fresh.outlineWidth);
    eq("text_alpha matches",    g_set.textAlpha, fresh.textAlpha);
    eq("dock_x matches",        g_set.dockX, fresh.dockX);
    eq("dock_y matches",        g_set.dockY, fresh.dockY);
    eq("heartbeat_ms matches",  double(g_set.heartbeatMs), double(fresh.heartbeatMs));

    printf("\n== writeDefaultSettingsIfMissing does not clobber an existing file ==\n");
    writeSettings("# mine\r\nserver_name = Keep Me\r\n");
    writeDefaultSettingsIfMissing();
    ok("the file was left alone", readSettings() == "# mine\r\nserver_name = Keep Me\r\n");

    printf("\n== values are read ==\n");
    writeSettings(
        "off = yes\n"
        "dock_x = -0.01\n"
        "show_aviation = no\n"
        "port=9222\n"
        "  show_cash   =   no  \n"
        "units = metric\n"
        "server_name = My Server\n"
        "max_players = 900\n"
        "font_scale = 1.25\n"
        "cash_color = #0f0\n"
        "hide_new = cash , bank\n"
        "# a comment = ignored\n"
        "\n");
    reload();
    ok("off",          g_set.off == true);
    eq("dock_x",       g_set.dockX, -0.01);
    ok("show_aviation", g_set.showAviation == false);
    eq("port",         double(g_set.port), 9222.0);
    ok("show_cash",    g_set.showCash == false);
    eq("units",        g_set.units, "metric");
    eq("server_name",  g_set.serverName, "My Server");
    eq("max_players",  double(g_set.maxPlayers), 900.0);
    eq("font_scale",   g_set.fontScale, 1.25);
    eq("short colour", g_set.cashColor, "#0f0");
    eq("hide_new is trimmed and lowered", g_set.hideNew, "cash , bank");
    ok("no warnings for a valid file", g_warnings.empty());

    printf("\n== a colour is either a hex colour or it is not used ==\n");
    writeSettings("cash_color = red\nbank_color = #12345\ntext_color = rgb(1,2,3)\n"
                  "yellow_color = #abc; } body { display:none } .x {\n");
    reload();
    eq("a name is refused",        g_set.cashColor,   fresh.cashColor);
    eq("wrong length is refused",  g_set.bankColor,   fresh.bankColor);
    eq("a function is refused",    g_set.textColor,   fresh.textColor);
    eq("stylesheet injection is refused", g_set.yellowColor, fresh.yellowColor);
    ok("each one warned", g_warnings.size() == 4);

    printf("\n== out of range and malformed numbers keep the default ==\n");
    writeSettings("port = 70000\nfont_scale = 99\ntext_alpha = abc\n"
                  "max_players = -5\nheartbeat_ms = 10\noutline_width = 1e309\n");
    reload();
    eq("port out of range",    double(g_set.port),        double(fresh.port));
    eq("scale out of range",   g_set.fontScale,           fresh.fontScale);
    eq("not a number",         g_set.textAlpha,           fresh.textAlpha);
    eq("negative players",     double(g_set.maxPlayers),  double(fresh.maxPlayers));
    eq("heartbeat too small",  double(g_set.heartbeatMs), double(fresh.heartbeatMs));
    eq("infinity refused",     g_set.outlineWidth,        fresh.outlineWidth);

    printf("\n== odds and ends ==\n");
    writeSettings("\xEF\xBB\xBF" "server_name = After BOM\n");
    reload();
    eq("a byte order mark is stripped", g_set.serverName, "After BOM");

    writeSettings("server_name = \nunits = klingon\nnot_a_setting = 1\n");
    reload();
    eq("an empty server name falls back", g_set.serverName, fresh.serverName);
    eq("an unknown unit falls back",      g_set.units, fresh.units);
    ok("unknown units and unknown keys both warn", g_warnings.size() == 2);

    // Split so the \x02 escape cannot swallow the C that follows it.
    writeSettings("server_name = Ctrl\x01\x02" "Chars\n");
    reload();
    eq("control characters are dropped", g_set.serverName, "CtrlChars");

    removeSettings();

    printf("\n%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
