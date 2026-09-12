# Changelog

## v1.0.0

First release. Draws GTA World's old HUD in FiveM from the data the current HUD is already being
sent, as a single `.asi` in the `plugins` folder.

- **Everything the old HUD drew.** Green cash and white bank at the top right; the compass
  letter, area and street docked beside the minimap with both dividers; speed, the ten-segment
  fuel bar and the odometer stacked above them; the `ALT / HDG / ATC` line in an aircraft; the
  `ELS` state that appears on a change and goes away five seconds later; `Admin-Duty` and
  `Tester-Duty` under the balances; and the single `GTA.WORLD — Roleplay v… — n/1500 — hh:mm`
  line along the bottom.

- **Placed the way the old HUD placed it.** Every line sits at its own offset from the minimap's
  corner, as a fraction of the screen, at the game's own text scale. Cap heights are matched with
  canvas text metrics and the face is squeezed to the old HUD's proportions, so the text is the
  right size and the right width at any resolution. The big map slides the block across, as it
  did before.

- **The font.** The game's own HUD face is licensed and is not shipped. A copy you put next to
  the `.asi` is used first, then an installed copy, then **Oswald** (SIL OFL), compiled into the
  plugin as a variable font so `font_weight` picks a real instance from 200 to 700.

- **Listens only.** It attaches to FiveM's own local debugging endpoint on loopback, installs a
  stylesheet and one message listener in a private execution context, and draws from the messages
  the HUD is already receiving. It never sends a message, never calls a game native, never reads
  or writes game memory, hooks nothing, and makes no outbound connection of any kind.

- **No dependencies.** WinHTTP for the socket, a JSON reader written for this and fuzzed under
  AddressSanitizer and UndefinedBehaviorSanitizer, and nothing else. Nothing is downloaded at
  build time or at run time, and there is no update check.

- **Configurable** from a plain text settings file written next to the plugin on first run: which
  parts to draw, which current widgets to hide, units, server and gamemode names, colours, font
  weight and scale, and a nudge for the docked block.

- **Reproducible builds.** `build.bat` needs only the Visual Studio Build Tools, produces the same
  bytes every time, and CI verifies that and attaches signed build provenance to every release.
