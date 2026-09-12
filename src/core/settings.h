#pragma once

#include <string>

// One plain-text settings file, written next to the .asi on first run with every option listed,
// explained and set to its default.
//
// There are no size or position settings beyond an overall scale and a pair of nudges. The old
// HUD's geometry — where each line sat relative to the minimap and what text scale it used — is
// what makes it the old HUD, so it lives in the layout table in the injected script rather than
// here, where changing it would quietly stop the result being the thing it is copying.
struct Settings
{
    bool off  = false;
    int  port = 13172;   // FiveM's CEF remote debugging port

    // Which parts of the old HUD to draw.
    bool showCash     = true;
    bool showBank     = true;
    bool showLocation = true;
    bool showSpeed    = true;
    bool showFuel     = true;
    bool showOdometer = true;
    bool showAviation = true;   // the ALT / HDG / ATC line, in a plane or helicopter
    bool showStaff    = true;   // the Admin-Duty and Tester-Duty lines under the balances
    bool showEls      = true;   // the ELS light and siren state, in an emergency vehicle
    bool showFooter   = true;
    bool showPlayers  = true;   // the player count in the bottom line
    bool showTime     = true;   // the clock in the bottom line

    // Which parts of the current HUD to make invisible. These are made transparent rather than
    // display:none on purpose: the widgets stay laid out, so their text can still be read for
    // the initial values and the compass can still be measured for docking.
    std::string hideNew = "cash,bank,location,compass,brand,speedo,weather,tips,minimapbar,speedsign";

    // The bottom line reads "<server> — <gamemode> v<version> — <players>/<max> — <clock>".
    std::string serverName   = "GTA.WORLD";
    std::string gamemodeName = "Roleplay";
    int         maxPlayers   = 1500;

    // The word in front of the fuel bar. The old HUD took it from the server, which sent
    // "Fuel" for a tank and the fuel type for anything else.
    std::string fuelLabel = "Fuel";

    // How long the ELS state stays on screen after it changes. The old HUD showed it for five
    // seconds and then hid it until the next change. 0 keeps it up for as long as you are in the
    // vehicle.
    int elsHoldMs = 5000;

    // A font file next to the .asi, loaded ahead of everything else. The old HUD was drawn in
    // the game's font 4, Chalet Comprime Cologne Sixty, which is licensed and is not shipped
    // here: put your own copy at this name to get the original face. Empty disables it.
    std::string fontFile = "gtaw-oldhud.font.ttf";

    // imperial | metric | auto. "auto" follows the in-game speedometer's own unit setting.
    std::string units = "imperial";

    // Shown in place of a value that has not arrived yet. Deliberately not "0": a real balance
    // of zero and an unknown balance must not look the same.
    std::string placeholder = "\xE2\x80\x94";   // U+2014 em dash

    // Multiplies every text scale. 1.0 is the size the old HUD drew at.
    double fontScale = 1.0;

    // How heavy to draw. The face built into the plugin is Oswald, a variable font whose weight
    // axis runs 200 to 700, so anything in that range is a real instance of it rather than a
    // synthesised one. 400 is regular, and is what the old HUD looked closest to.
    int fontWeight = 400;

    // Nudges for the whole block docked beside the minimap, as a fraction of the screen.
    // Positive x moves it right, positive y moves it down. The old HUD had the same pair for
    // players whose minimap was not where the safe zone said it was.
    double dockX = 0.0;
    double dockY = 0.0;

    // GTA's own HUD palette, which is what the old HUD drew in. textAlpha is the alpha the game
    // gave every one of these strings, 200 of 255.
    std::string textColor    = "#ffffff";
    std::string cashColor    = "#73ba83";
    std::string bankColor    = "#ffffff";
    std::string blueColor    = "#5db6e5";   // speed, altitude and heading numbers
    std::string yellowColor  = "#f0c850";   // the gamemode and version
    std::string redColor     = "#e03232";   // the first two fuel bars
    std::string orangeColor  = "#ff8555";   // the next three
    std::string greenColor   = "#72cc72";   // the last five, and ATC ONLINE
    std::string adminColor   = "#1c8000";   // Admin-Duty
    std::string testerColor  = "#731010";   // Tester-Duty
    std::string outlineColor = "#000000";
    double      outlineWidth = 1.0;         // px
    double      textAlpha    = 0.784;       // 200 / 255

    int reconnectMs = 2000;
    int heartbeatMs = 4000;
};

extern Settings g_set;

// Reads the file if present. Never writes and never blocks; safe to call under the loader lock.
void loadSettings();

// Creates the documented default file when it does not exist. Runs on the worker thread.
void writeDefaultSettingsIfMissing();
