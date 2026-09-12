# Changelog

## v1.0.1

- **`/hudlayout` places the old HUD.** The game's layout editor drags, scales and hides the
  current HUD's widgets and keeps the result between sessions; the plugin now reads that
  outcome off the widgets themselves and applies it to the old lines that stand in for them:
  cash, bank, the status icons (the duty lines), the compass, the location bar and the server
  block. While the editor is open the current widgets are shown again so there is something to
  drag, and the old lines follow live. Before, the plugin's transparency rule also stopped those
  widgets being dragged at all.

- **The location block no longer runs off the bottom of the screen.** The minimap rectangle the
  game reports runs down to the bottom of the safe zone, and at the default safe zone that is the
  bottom edge of the screen; the block was hung from it, and on a 1080p screen the street line
  was drawn half off the screen. The anchor now never sits below 0.985 of the screen height,
  which is where the old HUD's block was measured, while a minimap that really does sit higher is
  still followed.

- **The balances and the server line sit where the old HUD put them at every resolution.** Both
  are placed in the game's own normalised coordinates — 70px in from the right and 35px up from
  the bottom at 1920 by 1080, scaling with the screen — rather than a flat pixel inset below 1920
  wide. Nothing moves at 1920 by 1080 or above.

- **Getting into a vehicle starts from a clean reading.** Speed, altitude, heading and the ELS
  state reset on entering a vehicle, as they do in the current HUD, so the last patrol car's ELS
  state no longer follows you into the next car (visible with `els_hold_ms = 0`).

- **A heading is drawn as a bearing**, 0 to 359, so a negative reading or one past a full turn
  is not shown as such.

- **Balances seeded from the screen are read whatever the client's locale.** The current HUD
  writes them with the client's own thousands separator, and `$1.234.567` was read as `$1`.

- **The game's HUD nudge moves the whole of the old HUD**, as it moves the whole of the current
  one; before, only the balances followed it.

- **A window resize re-derives the minimap rectangle** from the game's numbers instead of keeping
  the one scaled for the old size.

- **The log no longer calls an install done when the interface's document was not built yet.**
  It waits and retries, once per `reconnect_ms`, and says so once.

- The settings file says what `tips` in `hide_new` covers: the row of status icons beside the
  balances — a new notification, bleeding, drugged — and the staff duty badge, not the badge
  alone.

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
