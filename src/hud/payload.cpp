#include "hud/payload.h"

#include "cdp/json.h"
#include "hud/font.h"
#include "core/logger.h"
#include "core/settings.h"
#include "core/util.h"
#include "core/version.h"

#include <cstdio>
#include <string>
#include <vector>

namespace hud {

namespace {

// ---------------------------------------------------------------------------------------------
// Number formatting
//
// These numbers are pasted straight into a stylesheet. The C runtime's decimal separator follows
// the process locale, and this DLL is a guest inside a game that may have set one where the
// separator is a comma. "3,40vh" is not a length, so the separator is normalised after
// formatting rather than hoped about.
// ---------------------------------------------------------------------------------------------
std::string num(double v, int decimals = 3)
{
    char buf[64];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.*f", decimals, v);

    std::string out(buf);
    for (char& c : out)
        if (c == ',') c = '.';
    return out;
}

// ---------------------------------------------------------------------------------------------
// Which parts of the current HUD to make invisible.
//
// Transparency rather than display:none, deliberately. The widgets stay laid out, which is what
// lets the injected code read the values that were already on screen when it attached, and
// measure the compass to work out where the minimap is.
// ---------------------------------------------------------------------------------------------
struct HidePart { const char* token; const char* selector; };

const HidePart kHideParts[] = {
    { "cash",     ".rightBlockSlot--cash" },
    { "bank",     ".rightBlockSlot--bank" },
    { "location", ".locBar"               },
    { "compass",  ".compassW"             },
    { "brand",    ".brandBlock"           },
    { "speedo",   ".spd-root"             },
    { "weather",  ".wxBar"                },
    { "tips",     ".tips"                 },
    { "minimapbar", ".mmb-bar,.mmb-overlay" },
    { "speedsign", ".speedSign"           },
};

std::string buildHideRules()
{
    std::vector<std::string> wanted = util::split(g_set.hideNew, ',');

    std::string selectors;
    for (const HidePart& part : kHideParts) {
        bool on = false;
        for (const std::string& token : wanted)
            if (token == part.token) { on = true; break; }
        if (!on) continue;

        if (!selectors.empty()) selectors += ",";
        selectors += part.selector;
    }

    if (selectors.empty()) return std::string();
    return selectors + "{opacity:0 !important;pointer-events:none !important;}\n";
}

// The game draws this text with SET_TEXT_OUTLINE, which surrounds every glyph. Eight offset
// copies of the shadow is the closest CSS gets to that.
std::string buildOutline()
{
    const std::string& c = g_set.outlineColor;
    double w = g_set.outlineWidth;

    if (w <= 0.0) return "none";

    std::string p = num(w, 2) + "px";
    std::string n = "-" + p;

    std::string s;
    s += n + " " + n + " 0 " + c + ",";
    s += p + " " + n + " 0 " + c + ",";
    s += n + " " + p + " 0 " + c + ",";
    s += p + " " + p + " 0 " + c + ",";
    s += "0 " + n + " 0 " + c + ",";
    s += "0 " + p + " 0 " + c + ",";
    s += n + " 0 0 " + c + ",";
    s += p + " 0 0 " + c;
    return s;
}

// The family list, used verbatim both in the stylesheet and in the canvas font shorthand the
// script measures with, so the two cannot disagree about which face is being laid out.
//
// The old HUD was drawn in the game's font 4, Chalet Comprime Cologne Sixty. It is licensed and
// is not this project's to ship, so it is reached for in the two ways that do not require
// shipping it: a copy the user puts next to the .asi, then an installed system copy. After that
// comes the condensed face built into the plugin, and only then the ordinary fallbacks.
const char* kUserFontFamily = "GTAWOldHudUser";
const char* kFallbackFamily = "GTAWOldHudCondensed";

// The font the user dropped next to the .asi, if there was one. Read once, at install time.
std::string g_userFontBase64;
bool g_userFontTried = false;

// A licensed face cannot be shipped here, so the way to get the original one is to put a copy
// of it next to the .asi. It is read whole and pasted into the stylesheet as a data URI, which
// is the only way a page in the game's browser can be given a font off the local disk.
//
// The name is a leaf, never a path: the settings file is trusted to configure this plugin, not
// to point it at somewhere else on the disk and publish what it finds there.
void loadUserFontOnce()
{
    if (g_userFontTried) return;
    g_userFontTried = true;

    const std::string& leaf = g_set.fontFile;
    if (leaf.empty()) return;
    if (leaf.find('\\') != std::string::npos || leaf.find('/') != std::string::npos ||
        leaf.find(':') != std::string::npos) {
        LOG_WARN("font_file: '%s' is not a plain file name; ignoring it", leaf.c_str());
        return;
    }

    std::wstring path = util::modulePath(util::widen(leaf).c_str());
    std::string bytes;
    if (!util::readBinaryFile(path, 4 * 1024 * 1024, bytes)) return;

    g_userFontBase64 = util::base64(bytes);
    LOG_INFO("Loaded %s as the HUD font (%zu bytes)", leaf.c_str(), bytes.size());
}

std::string buildFontStack()
{
    std::string stack;
    if (!g_userFontBase64.empty()) { stack += "\""; stack += kUserFontFamily; stack += "\","; }
    stack += "\"ChaletComprime-CologneSixty\",\"Chalet Comprime Cologne Sixty\","
             "\"ChaletComprimeCologneSixty\",";
    stack += "\""; stack += kFallbackFamily; stack += "\",";
    stack += "\"Roboto Condensed\",\"Arial Narrow\",Archivo,Roboto,\"Segoe UI\",sans-serif";
    return stack;
}

std::string buildFontFaces()
{
    std::string css;
    if (!g_userFontBase64.empty()) {
        // The whole 100 to 900 range, because what the file holds is not knowable from here: a
        // variable font then gets instanced at whatever font_weight asks for, and a static one
        // is drawn as it is, or synthesised heavier, which is what asking for that means.
        css += "@font-face{font-family:\"";
        css += kUserFontFamily;
        css += "\";font-style:normal;font-weight:100 900;font-display:block;"
               "src:url(data:font/ttf;base64,";
        css += g_userFontBase64;
        css += ");}\n";
    }
    css += "@font-face{font-family:\"";
    css += kFallbackFamily;
    css += "\";font-style:normal;font-weight:";
    css += kFallbackFontWeights;
    css += ";font-display:block;src:url(data:";
    css += kFallbackFontMime;
    css += ";base64,";
    for (int i = 0; kFallbackFontBase64Parts[i]; ++i) css += kFallbackFontBase64Parts[i];
    css += ") format(\"";
    css += kFallbackFontFormat;
    css += "\");}\n";
    return css;
}

// ---------------------------------------------------------------------------------------------
// The stylesheet
//
// It carries colour and nothing else. Every position and every font size is computed in the
// script from the minimap rectangle and the screen size, because that is how the old HUD was
// laid out: offsets from the minimap's corner, as a fraction of the screen, at the game's own
// text scales.
//
// Every rule is prefixed with the root id, which is not tidiness. The reset below has to be able
// to beat a stray element rule in the page's own stylesheet, so it is written with the id in it;
// that gives it a higher specificity than a bare class, and the class rules would lose to their
// own reset unless they carry the id too.
// ---------------------------------------------------------------------------------------------
const char* kBaseCss = R"CSS(
#gtaw-oldhud-root{
  position:fixed; left:0; top:0; right:0; bottom:0;
  margin:0; padding:0; border:0;
  pointer-events:none;
  z-index:4550;
  font-family:var(--ohud-font);
  font-weight:var(--ohud-weight);
  -webkit-font-smoothing:antialiased;
  opacity:1;
  transition:opacity 200ms linear;
  contain:layout style;
}
#gtaw-oldhud-root *{
  margin:0; padding:0; border:0; box-sizing:border-box;
  font:inherit; color:inherit; text-align:left; background:none;
}
/* One drawn string. The game gives every one of these an alpha of 200, which covers the
   outline as well as the glyph, so it is applied here rather than in each colour. */
/* --ohud-sx squeezes the text horizontally. Whichever face the client resolved, it is measured
   at run time and corrected to the old HUD's proportions, so a substitute does not come out
   half again as wide at the same height. */
#gtaw-oldhud-root .ohud-line{
  position:absolute;
  white-space:nowrap;
  line-height:1;
  color:var(--ohud-text);
  text-shadow:var(--ohud-outline);
  opacity:var(--ohud-alpha);
  transform:scaleX(var(--ohud-sx,1));
  transform-origin:0 50%;
}
#gtaw-oldhud-root .ohud-mid{
  transform:translateX(-50%) scaleX(var(--ohud-sx,1));
  transform-origin:50% 50%;
}
/* The staff lines were the one thing the old HUD drew fully opaque. */
#gtaw-oldhud-root .ohud-solid{ opacity:1; }
#gtaw-oldhud-root .ohud-admin{ color:var(--ohud-admin); }
#gtaw-oldhud-root .ohud-tester{ color:var(--ohud-tester); }

#gtaw-oldhud-root .ohud-cash{ color:var(--ohud-cash); }
#gtaw-oldhud-root .ohud-bank{ color:var(--ohud-bank); }
#gtaw-oldhud-root .ohud-b{ color:var(--ohud-blue); }
#gtaw-oldhud-root .ohud-y{ color:var(--ohud-yellow); }
#gtaw-oldhud-root .ohud-r{ color:var(--ohud-red); }
#gtaw-oldhud-root .ohud-o{ color:var(--ohud-orange); }
#gtaw-oldhud-root .ohud-g{ color:var(--ohud-green); }
)CSS";

std::string buildCss()
{
    std::string css;

    css += buildFontFaces();

    css += "#gtaw-oldhud-root{\n";
    css += "  --ohud-font:" + buildFontStack() + ";\n";
    css += "  --ohud-weight:" + std::to_string(g_set.fontWeight) + ";\n";
    css += "  --ohud-text:"   + g_set.textColor   + ";\n";
    css += "  --ohud-cash:"   + g_set.cashColor   + ";\n";
    css += "  --ohud-bank:"   + g_set.bankColor   + ";\n";
    css += "  --ohud-blue:"   + g_set.blueColor   + ";\n";
    css += "  --ohud-yellow:" + g_set.yellowColor + ";\n";
    css += "  --ohud-red:"    + g_set.redColor    + ";\n";
    css += "  --ohud-orange:" + g_set.orangeColor + ";\n";
    css += "  --ohud-green:"  + g_set.greenColor  + ";\n";
    css += "  --ohud-admin:"  + g_set.adminColor  + ";\n";
    css += "  --ohud-tester:" + g_set.testerColor + ";\n";
    css += "  --ohud-alpha:"  + num(g_set.textAlpha, 3) + ";\n";
    css += "  --ohud-outline:" + buildOutline() + ";\n";
    css += "}\n";

    css += kBaseCss;
    css += buildHideRules();
    return css;
}

// ---------------------------------------------------------------------------------------------
// The injected script
//
// It only listens. It never posts a message, never calls an NUI callback and never touches the
// game's own state: it reads the messages the interface is already receiving and draws its own
// nodes from them.
// ---------------------------------------------------------------------------------------------
const char* kScript = R"JS(function (CFG) {
  'use strict';

  var NS = '__gtawOldHud', ROOT_ID = 'gtaw-oldhud-root', STYLE_ID = 'gtaw-oldhud-style';

  // A previous install may still be running in an older private world, or may have left its
  // nodes behind when that world was torn down with the document. Clear both before rebuilding.
  if (window[NS] && typeof window[NS].destroy === 'function') {
    try { window[NS].destroy(); } catch (e) {}
  }
  var stale = [document.getElementById(ROOT_ID), document.getElementById(STYLE_ID)];
  for (var s = 0; s < stale.length; s++)
    if (stale[s] && stale[s].parentNode) stale[s].parentNode.removeChild(stale[s]);

  if (!document.body) return 'no-body';

  // ---- the old HUD's own layout table -----------------------------------------------------
  // x runs right from the minimap's right edge and y runs up from its bottom edge, both as a
  // fraction of the screen; s is the game's text scale. These are the numbers the old HUD drew
  // with, so the block keeps its shape at any resolution the way the original did.
  var L = {
    avi:    { x: 0.006, y: 0.205, s: 0.8 },
    speed:  { x: 0.006, y: 0.165, s: 0.8 },
    fuel:   { x: 0.006, y: 0.125, s: 0.8 },
    odo:    { x: 0.006, y: 0.085, s: 0.8 },
    sdiv:   { x: 0.005, y: 0.045, s: 1.0 },
    card:   { x: 0.025, y: 0.046, s: 1.0 },
    cdiv:   { x: 0.038, y: 0.045, s: 1.0 },
    zone:   { x: 0.045, y: 0.040, s: 0.5 },
    street: { x: 0.045, y: 0.016, s: 0.4 }
  };

  // Cap height of the game's HUD face, as a fraction of the screen height per unit of text
  // scale, and the drop from the top of the game's text cell to the top of the capitals,
  // measured in cap heights. Both were read off the old HUD screenshots.
  var CAP_PER_SCALE = 0.03444, CELL_TO_CAP = 0.4123;

  // The old HUD moved the whole docked block right by this much while the big map was open.
  var BIGMAP_SHIFT = 0.089;

  // Balances and the server line were positioned against the screen, not the minimap. The old
  // HUD only fell back to the real screen size below 1920 wide; above it, it kept using 1920 by
  // 1080 as the reference, which is why the inset grows with the resolution.
  var CASH_Y = 0.060, BANK_Y = 0.100, ADMIN_Y = 0.140, TESTER_Y = 0.175, FOOTER_LIFT = 35;
  var MONEY_INSET = 70;
  var CASH_SCALE = 0.8, BANK_SCALE = 0.7, STAFF_SCALE = 0.5, FOOTER_SCALE = 0.45;
  var REF_W = 1920, REF_H = 1080;

  // The ELS state sat 0.18 of the screen above the minimap's bottom edge, over on the right with
  // the balances, as two lines: the word ELS and the state under it.
  var ELS_LIFT = 0.18, ELS_SCALE = 0.6;

  // One line of the game's text, as a fraction of the screen per unit of text scale. This is
  // what ~n~ advanced by, and it is also the gap the old HUD left between its two staff lines.
  var LINE_PER_SCALE = 0.07;

  // The odometer was drawn only inside this range.
  var ODO_MIN = 0, ODO_MAX = 40000;

  var MI_PER_KM = 1.60934;

  // ---- state ---------------------------------------------------------------------------
  // null means "has not arrived yet" and renders as the placeholder. It is never shown as 0:
  // an unknown balance and a real balance of zero must not look alike.
  var hud = { cash: null, bank: null, street: null, zone: null, propertyName: null,
              cardinal: null, version: null, players: null, time: null };
  var spd = { visible: false, speed: null, kms: null, measure: false,
              fuel: null, maxFuel: 100, isElectric: false,
              isPlane: false, altitude: null, heading: null, atcOnline: false,
              elsVisible: false, elsCode: 1, elsTone: 0 };
  var tips = { admin: false, support: false };

  // The old HUD put the ELS state up for five seconds after it changed and then took it away
  // again until the next change. elsShownAt is when the current state arrived.
  var elsShownAt = 0, elsTimer = null;

  var radar = null, bigmap = false, offset = { x: 0, y: 0 };
  var hudVisible = true, pauseMenu = false, fadeOpaque = true, fadeMs = 200;

  // ---- nodes ---------------------------------------------------------------------------
  var style = document.createElement('style');
  style.id = STYLE_ID;
  style.textContent = CFG.css;
  (document.head || document.documentElement).appendChild(style);

  var root = document.createElement('div');
  root.id = ROOT_ID;

  // One drawn string, with its own place in the layout table.
  function line(spec, mid) {
    var n = document.createElement('div');
    n.className = 'ohud-line' + (mid ? ' ohud-mid' : '');
    n._spec = spec;
    root.appendChild(n);
    return n;
  }

  function part(parent, cls) {
    var n = document.createElement('span');
    if (cls) n.className = cls;
    parent.appendChild(n);
    return n;
  }

  var cashEl = line({ s: CASH_SCALE }, true); cashEl.className += ' ohud-cash';
  var bankEl = line({ s: BANK_SCALE }, true); bankEl.className += ' ohud-bank';

  var adminEl  = line({ s: STAFF_SCALE }, true);
  adminEl.className += ' ohud-solid ohud-admin';
  adminEl.textContent = 'Admin-Duty';
  var testerEl = line({ s: STAFF_SCALE }, true);
  testerEl.className += ' ohud-solid ohud-tester';
  testerEl.textContent = 'Tester-Duty';

  var elsEl      = line({ s: ELS_SCALE }, true);
  elsEl.textContent = 'ELS';
  var elsStateEl = line({ s: ELS_SCALE }, true);

  var aviEl    = line(L.avi);
  var aviLead  = part(aviEl, null);
  var aviAlt   = part(aviEl, 'ohud-b');
  var aviMid   = part(aviEl, null);
  var aviHdg   = part(aviEl, 'ohud-b');
  var aviAtc   = part(aviEl, 'ohud-b');
  var aviOn    = part(aviEl, 'ohud-g');

  var speedEl   = line(L.speed);
  var speedUnit = part(speedEl, null);
  var speedVal  = part(speedEl, 'ohud-b');

  var fuelEl    = line(L.fuel);
  var fuelLabel = part(fuelEl, null);
  var fuelRed   = part(fuelEl, 'ohud-r');
  var fuelOrg   = part(fuelEl, 'ohud-o');
  var fuelGrn   = part(fuelEl, 'ohud-g');

  var odoEl    = line(L.odo);
  var sdivEl   = line(L.sdiv);
  var cardEl   = line(L.card, true);
  var cdivEl   = line(L.cdiv);
  var zoneEl   = line(L.zone);
  var streetEl = line(L.street);

  var footEl  = line({ s: FOOTER_SCALE }, true);
  var fServer = part(footEl, null);
  var fMode   = part(footEl, 'ohud-y');
  var fTail   = part(footEl, null);

  sdivEl.textContent = '|';
  cdivEl.textContent = '|';

  document.body.appendChild(root);

  // ---- helpers -------------------------------------------------------------------------
  function show(node, on) { node.style.display = on ? '' : 'none'; }

  function toNum(v) {
    if (typeof v === 'number') return isFinite(v) ? v : null;
    if (typeof v === 'string') {
      var cleaned = v.replace(/[^0-9.\-]/g, '');
      if (!cleaned) return null;
      var n = Number(cleaned);
      return isFinite(n) ? n : null;
    }
    return null;
  }

  function toText(v) {
    if (typeof v === 'string') return v.trim();
    if (typeof v === 'number' && isFinite(v)) return String(v);
    return '';
  }

  // The old HUD's own thousands separator, rather than a locale-dependent one.
  function commas(n) {
    var parts = String(n).split('.');
    parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ',');
    return parts.join('.');
  }

  function money_(v) {
    var n = toNum(v);
    return n === null ? CFG.placeholder : '$' + commas(Math.round(n));
  }

  // ---- type -----------------------------------------------------------------------------
  // The game sizes this text by a scale factor, not by a pixel size, so the script works the
  // other way round: it asks for a cap height and finds the font size that produces it. Canvas
  // text metrics give that exactly, for whichever face in the stack the client actually has;
  // the ratios below stand in when they are unavailable.
  var FALLBACK_CAP = 0.716, FALLBACK_TOP = 0.1305;

  var mctx;
  function measurer() {
    if (mctx === undefined) {
      mctx = null;
      try {
        var c = document.createElement('canvas');
        if (c && c.getContext) mctx = c.getContext('2d');
      } catch (e) { mctx = null; }
    }
    return mctx;
  }

  function probe(c, size) {
    try {
      c.font = CFG.fontWeight + ' ' + size + 'px ' + CFG.font;
      var m = c.measureText('H');
      var cap = m.actualBoundingBoxAscent;
      var asc = m.fontBoundingBoxAscent, desc = m.fontBoundingBoxDescent;
      if (!(cap > 0) || !(asc > 0) || !(desc >= 0)) return null;
      // With line-height 1 the half leading is (1em - (ascent + descent)) / 2, which is how far
      // the baseline sits below the top of the box.
      return { cap: cap, topToCap: (size - (asc + desc)) / 2 + asc - cap };
    } catch (e) { return null; }
  }

  // ---- how wide the face is ---------------------------------------------------------------
  // Ink width over cap height for strings measured out of the old HUD screenshots. Whichever
  // face the client resolved gets measured the same way, and the ratio between the two is the
  // horizontal squeeze that gives it the old HUD's proportions. Four strings rather than one so
  // a single odd glyph cannot skew it.
  var WIDTHS = [
    ['Brouge Avenue / Carson Avenue', 13.952],
    ['$189,256',                       3.779],
    ['1.08 mi.',                       3.105],
    ['Davis',                          2.516]
  ];

  function measureSqueeze() {
    var c = measurer();
    if (!c) return 1;
    try {
      c.font = CFG.fontWeight + ' 100px ' + CFG.font;
      var cap = c.measureText('H').actualBoundingBoxAscent;
      if (!(cap > 0)) return 1;

      var total = 0, n = 0;
      for (var i = 0; i < WIDTHS.length; i++) {
        var m = c.measureText(WIDTHS[i][0]);
        var ink = (m.actualBoundingBoxLeft || 0) + (m.actualBoundingBoxRight || 0);
        if (!(ink > 0)) ink = m.width;
        if (!(ink > 0)) continue;
        total += WIDTHS[i][1] / (ink / cap);
        n++;
      }
      if (!n) return 1;

      var k = total / n;
      // A face that needs more than this is not a face this is going to rescue, so leave it be
      // rather than draw something unreadable.
      return (k > 0.35 && k < 1.8) ? k : 1;
    } catch (e) { return 1; }
  }

  var fontCache = {};
  // The font size that draws capitals capPx tall, and how far the cap top sits below the top of
  // the element at that size.
  function fontFor(capPx) {
    var key = Math.round(capPx * 100);
    if (fontCache[key]) return fontCache[key];

    var size = capPx / FALLBACK_CAP;
    var topToCap = size * FALLBACK_TOP;

    var c = measurer();
    if (c) {
      var first = probe(c, size);
      if (first) {
        size = size * capPx / first.cap;          // cap height is linear in the font size
        var second = probe(c, size);
        topToCap = second ? second.topToCap : size * FALLBACK_TOP;
      }
    }

    fontCache[key] = { size: size, topToCap: topToCap };
    return fontCache[key];
  }

  // ---- the minimap rectangle -----------------------------------------------------------
  // The rectangle arrives in the game's own screen space. Scale it to the window, then apply
  // the same inset and aspect correction the minimap is actually drawn with, so the old HUD
  // docks against the same edge the current one does, at any resolution or aspect.
  function setRadar(r) {
    if (!r || typeof r !== 'object' ||
        typeof r.width !== 'number' || typeof r.height !== 'number' ||
        !(r.width > 0) || !(r.height > 0) ||
        !isFinite(r.left) || !isFinite(r.top)) { radar = null; return; }

    var sx = r.screenW ? (window.innerWidth  || r.screenW) / r.screenW : 1;
    var sy = r.screenH ? (window.innerHeight || r.screenH) / r.screenH : 1;

    var left = r.left * sx, top = r.top * sy, w = r.width * sx, h = r.height * sy;

    var aspect = (r.screenW && r.screenH) ? r.screenW / r.screenH : 16 / 9;
    var wide   = aspect > 16 / 9 + 0.001;

    left += w * (wide ? 0.042 : 0.029);

    var drawnH = w / 1.4185;
    if (drawnH < h) { top += h - drawnH; h = drawnH; }
    if (wide) top += 2;

    radar = { left: left, top: top, width: w, height: h };
  }

  // The corner the whole docked block is measured from: the minimap's right edge and its bottom.
  function anchor() {
    if (radar) return { x: radar.left + radar.width, y: radar.top + radar.height };

    // Before the first rectangle arrives, work backwards from the current HUD's compass, which
    // sits at the minimap's right edge plus max(10, width * 0.06) and is a fifth of the
    // minimap's width. It is only made transparent, never removed, so it still reports a real
    // rectangle. The game sends no bottom edge anywhere else, so that one falls back to where
    // the default safe zone puts it.
    var compass = document.querySelector('.compassW');
    if (compass) {
      var b = compass.getBoundingClientRect();
      if (b.width > 0) {
        var w = b.width / 0.2;
        return { x: b.left - Math.max(10, w * 0.06), y: window.innerHeight * 0.985 };
      }
    }
    return null;
  }

  // ---- drawing -------------------------------------------------------------------------
  var pending = false;
  function schedule() {
    if (pending) return;
    pending = true;
    window.requestAnimationFrame(render);
  }

  // Places one string the way the game placed it: x and y name the top-left (or top-centre) of
  // the game's text cell, so the cap top is CELL_TO_CAP cap heights below y.
  function place(node, x, y, scale) {
    var capPx = CAP_PER_SCALE * scale * CFG.fontScale * (window.innerHeight || 1);
    var f = fontFor(capPx);
    node.style.fontSize = f.size + 'px';
    node.style.left = x + 'px';
    node.style.top  = (y + CELL_TO_CAP * capPx - f.topToCap) + 'px';
  }

  function render() {
    pending = false;

    var W = window.innerWidth || 0, H = window.innerHeight || 0;

    root.style.transitionDuration = fadeMs + 'ms';
    root.style.opacity = (hudVisible && !pauseMenu && fadeOpaque) ? '1' : '0';

    // ---- balances ----------------------------------------------------------------------
    // The reference resolution the old HUD measured the inset against.
    var refW = W < REF_W ? W : REF_W, refH = W < REF_W ? H : REF_H;
    var moneyX = W * (refW - MONEY_INSET) / (refW || 1) + (offset.x || 0) * W;
    var moneyDY = (offset.y || 0) * H;

    cashEl.textContent = money_(hud.cash);
    bankEl.textContent = money_(hud.bank);
    place(cashEl, moneyX, CASH_Y * H + moneyDY, CASH_SCALE);
    place(bankEl, moneyX, BANK_Y * H + moneyDY, BANK_SCALE);
    show(cashEl, CFG.showCash);
    show(bankEl, CFG.showBank);

    // ---- on duty -----------------------------------------------------------------------
    place(adminEl,  moneyX, ADMIN_Y  * H + moneyDY, STAFF_SCALE);
    place(testerEl, moneyX, TESTER_Y * H + moneyDY, STAFF_SCALE);
    show(adminEl,  CFG.showStaff && !!tips.admin);
    show(testerEl, CFG.showStaff && !!tips.support);

    // ---- the ELS state -----------------------------------------------------------------
    // Measured up from the minimap's bottom edge, but drawn over on the right with the
    // balances, which is where the old HUD put it.
    var elsBottom = radar ? (radar.top + radar.height) : H * 0.985;
    var elsY = elsBottom - ELS_LIFT * H;
    var code = toNum(spd.elsCode);
    elsStateEl.textContent = code === 3 ? 'SIREN' : code === 2 ? 'LIGHTS' : 'OFF';
    elsStateEl.className = 'ohud-line ohud-mid ' +
        (code === 3 ? 'ohud-g' : code === 2 ? 'ohud-y' : 'ohud-r');
    place(elsEl,      moneyX, elsY, ELS_SCALE);
    place(elsStateEl, moneyX, elsY + LINE_PER_SCALE * ELS_SCALE * H, ELS_SCALE);

    var elsOn = CFG.showEls && !!spd.visible && !!spd.elsVisible &&
                (CFG.elsHoldMs <= 0 || Date.now() - elsShownAt < CFG.elsHoldMs);
    show(elsEl,      elsOn);
    show(elsStateEl, elsOn);

    // ---- the server line ---------------------------------------------------------------
    var version = toText(hud.version).replace(/^v/i, '');
    var players = toNum(hud.players);
    var time    = toText(hud.time);

    var tail = '';
    // The old HUD wrote both counts plainly, with no thousands separator.
    if (CFG.showPlayers && players !== null)
      tail += ' \u2014 ' + Math.round(players) + '/' + CFG.maxPlayers;
    if (CFG.showTime && time) tail += ' \u2014 ' + time;

    fServer.textContent = CFG.serverName + ' \u2014 ';
    fMode.textContent   = CFG.gamemodeName + (version ? ' v' + version : '');
    fTail.textContent   = tail;
    place(footEl, W / 2, H * (refH - FOOTER_LIFT) / (refH || 1), FOOTER_SCALE);
    show(footEl, CFG.showFooter);

    // ---- everything docked against the minimap -----------------------------------------
    var a = anchor();
    if (!a) {
      var docked = [aviEl, speedEl, fuelEl, odoEl, sdivEl, cardEl, cdivEl, zoneEl, streetEl];
      for (var i = 0; i < docked.length; i++) show(docked[i], false);
      return;
    }

    // The old HUD had a pair of nudges for players whose minimap was not where the safe zone
    // said it was; dock_x and dock_y are the same thing.
    var ox = a.x + (bigmap ? BIGMAP_SHIFT * W : 0) + CFG.dockX * W;
    var oy = a.y + CFG.dockY * H;
    function put(node) {
      var spec = node._spec;
      place(node, ox + spec.x * W, oy - spec.y * H, spec.s);
    }

    // A property name replaces the whole location block with one line where the street goes,
    // the way the old HUD drew an interior's name.
    var property = toText(hud.propertyName);
    var zone     = toText(hud.zone);
    var street   = toText(hud.street);
    var card     = toText(hud.cardinal);

    cardEl.textContent   = card;
    zoneEl.textContent   = zone;
    streetEl.textContent = property || street;

    var loc = CFG.showLocation;
    show(cardEl,   loc && !property && !!card);
    show(cdivEl,   loc && !property && !!card);
    show(zoneEl,   loc && !property && !!zone);
    show(streetEl, loc && !!(property || street));
    put(cardEl); put(cdivEl); put(zoneEl); put(streetEl);

    // ---- the vehicle block --------------------------------------------------------------
    // kms arrives in miles despite its name, so only a metric reading needs converting. speed
    // arrives in whichever unit measure names, so it is only converted when the settings ask
    // for the other one.
    var metric  = CFG.units === 'metric' || (CFG.units === 'auto' && !!spd.measure);
    var speed   = toNum(spd.speed);
    var mileage = toNum(spd.kms);

    if (speed !== null && metric !== !!spd.measure)
      speed = metric ? speed * MI_PER_KM : speed / MI_PER_KM;

    speedUnit.textContent = metric ? 'KMH ' : 'MPH ';
    speedVal.textContent  = speed === null ? CFG.placeholder : String(Math.round(speed));

    // The old HUD rounded to two places first and then decided whether to draw it, so a
    // reading that rounds to 0.00 is not drawn at all.
    var odo = mileage === null ? null
                               : Math.round((metric ? mileage * MI_PER_KM : mileage) * 100) / 100;
    odoEl.textContent = odo === null ? CFG.placeholder
                                     : odo.toFixed(2) + (metric ? ' km.' : ' mi.');

    // The old HUD's fuel bar: two red bars, then three orange, then five green, one dropping
    // for every ten percent of the tank.
    var fuel = toNum(spd.fuel), maxFuel = toNum(spd.maxFuel);
    var pct = (fuel === null || maxFuel === null || maxFuel <= 0)
        ? null : Math.max(0, Math.min(100, fuel / maxFuel * 100));

    fuelLabel.textContent = CFG.fuelLabel + ' ';
    if (pct === null || pct <= 0) {
      fuelRed.textContent = 'EMPTY';
      fuelOrg.textContent = '';
      fuelGrn.textContent = '';
    } else {
      var bars = Math.min(10, Math.ceil(pct / 10));
      fuelRed.textContent = '|'.repeat(Math.min(2, bars));
      fuelOrg.textContent = '|'.repeat(Math.max(0, Math.min(3, bars - 2)));
      fuelGrn.textContent = '|'.repeat(Math.max(0, bars - 5));
    }

    var altitude = toNum(spd.altitude), heading = toNum(spd.heading);
    aviLead.textContent = 'ALT: ';
    aviAlt.textContent = altitude === null ? CFG.placeholder : String(Math.round(altitude));
    aviMid.textContent = ' ft. / HDG: ';
    aviHdg.textContent = heading === null ? CFG.placeholder : String(Math.round(heading));
    aviAtc.textContent = spd.atcOnline ? ' / ATC: ' : '';
    aviOn.textContent  = spd.atcOnline ? 'ONLINE' : '';

    var inVehicle = !!spd.visible;
    show(aviEl,    inVehicle && CFG.showAviation && !!spd.isPlane);
    show(speedEl,  inVehicle && CFG.showSpeed);
    show(fuelEl,   inVehicle && CFG.showFuel && pct !== null);
    show(sdivEl,   inVehicle && (CFG.showSpeed || CFG.showOdometer || CFG.showFuel));
    show(odoEl,    inVehicle && CFG.showOdometer && odo !== null && odo > ODO_MIN && odo < ODO_MAX);
    put(aviEl); put(speedEl); put(fuelEl); put(sdivEl); put(odoEl);
  }

  // ---- incoming messages ----------------------------------------------------------------
  var HUD_KEYS = ['cash', 'bank', 'street', 'zone', 'propertyName', 'cardinal', 'version',
                  'players', 'time'];
  var SPD_KEYS = ['speed', 'kms', 'measure', 'visible', 'fuel', 'maxFuel', 'isElectric',
                  'isPlane', 'altitude', 'heading', 'atcOnline',
                  'elsVisible', 'elsCode', 'elsTone'];

  // Merge, never replace: an update carries only the fields that changed, so a field left out
  // has to keep its previous value.
  function merge(target, keys, data) {
    if (!data || typeof data !== 'object') return;
    for (var i = 0; i < keys.length; i++)
      if (keys[i] in data) target[keys[i]] = data[keys[i]];
  }

  function handle(msg) {
    var action = msg.action, data = msg.data;

    // A batch carries its updates in an array. Each child is also delivered again as a message
    // of its own, so every child is seen twice; the handlers only ever merge fields, so
    // applying one twice lands on the same state.
    if (action === 'NUI::Global::Batch') {
      if (!Array.isArray(data)) return;
      for (var i = 0; i < data.length; i++) {
        var child = data[i];
        if (child && typeof child === 'object' && child.action) handle(child);
      }
      return;
    }

    switch (action) {
      case 'HUD::SET_STATE':          merge(hud, HUD_KEYS, data); break;
      case 'NUI::Speedometer::State': elsWatch(function () { merge(spd, SPD_KEYS, data); }); break;
      case 'NUI::Speedometer::Show':
        elsWatch(function () { spd.visible = true; merge(spd, SPD_KEYS, data); });
        break;
      case 'NUI::Speedometer::Hide':  spd.visible = false; break;
      case 'NUI::Hud::ToggleTip':
        if (data && typeof data === 'object' && (data.tip in tips)) tips[data.tip] = !!data.visible;
        break;
      case 'NUI::Hud::RadarRect':     setRadar(data); break;
      case 'NUI::Hud::ToggleBigmap':  bigmap = !!(data && data.active); break;
      case 'NUI::Hud::Offset':
        offset = { x: (data && Number(data.x)) || 0, y: (data && Number(data.y)) || 0 };
        break;
      case 'SET_HUD_VISIBLE':         hudVisible = !!data; break;
      case 'NUI::Hud::PauseMenu':     pauseMenu = !!(data && data.active); break;
      case 'NUI::Hud::InactiveFade':
        fadeOpaque = !data || data.opaque !== false;
        var ms = data ? Number(data.ms) : NaN;
        fadeMs = isFinite(ms) && ms > 0 ? ms : 200;
        break;
      default: return;
    }
    schedule();
  }

  // Restarts the old HUD's five second window whenever the ELS state actually changes, so it
  // appears on a change and goes away again rather than sitting there.
  function elsWatch(apply) {
    var before = spd.elsCode + '/' + (spd.elsVisible ? 1 : 0);
    apply();
    if (before === spd.elsCode + '/' + (spd.elsVisible ? 1 : 0)) return;

    elsShownAt = Date.now();
    if (elsTimer) { clearTimeout(elsTimer); elsTimer = null; }
    if (CFG.elsHoldMs > 0 && spd.elsVisible)
      elsTimer = setTimeout(function () { elsTimer = null; schedule(); }, CFG.elsHoldMs + 50);
  }

  function onMessage(ev) {
    var data = ev.data;
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch (e) { return; }
    }
    if (!data || typeof data !== 'object' || !data.action) return;
    handle(data);
  }

  // ---- what was already on screen ---------------------------------------------------------
  // The plugin attaches partway through a session, so the first update for a field may be a long
  // way off. The current HUD's own widgets are still laid out, just transparent, so their text
  // covers the gap. The compass letter is skipped because it is localised before display, and
  // the odometer because its text carries whichever unit that HUD is set to.
  function readText(selector) {
    var node = document.querySelector(selector);
    if (!node) return '';
    var text = node.textContent;
    return text ? text.trim() : '';
  }

  function seedFromScreen() {
    if (hud.cash === null)    { var c = toNum(readText('.rightBlockSlot--cash .trVal')); if (c !== null) hud.cash = c; }
    if (hud.bank === null)    { var b = toNum(readText('.rightBlockSlot--bank .trVal')); if (b !== null) hud.bank = b; }
    if (hud.zone === null)    { var z = readText('.locZoneInner') || readText('.locZone'); if (z) hud.zone = z; }
    if (hud.street === null)  { var t = readText('.locStreet'); if (t) hud.street = t; }
    if (hud.version === null) { var v = readText('.brandVersion'); if (v) hud.version = v.replace(/^v/i, ''); }
    if (hud.players === null) { var p = toNum(readText('.brandPlayers')); if (p !== null) hud.players = p; }
    if (hud.time === null)    { var m = readText('.wxTime') || readText('.locMetaTime'); if (m) hud.time = m; }
  }

  function recalibrate() {
    fontCache = {};
    root.style.setProperty('--ohud-sx', String(measureSqueeze()));
    schedule();
  }

  function onResize() { fontCache = {}; schedule(); }

  // ---- lifetime ---------------------------------------------------------------------------
  window.addEventListener('message', onMessage);
  window.addEventListener('resize', onResize);

  seedFromScreen();
  recalibrate();
  render();

  // The stylesheet carries its fonts as data URIs, so nothing is fetched, but they are still
  // parsed asynchronously: the first measurement can land before the real face is ready. Take
  // it again once it is.
  if (document.fonts) {
    try {
      if (document.fonts.load)
        document.fonts.load(CFG.fontWeight + ' 40px ' + CFG.font).then(recalibrate, function () {});
      if (document.fonts.ready) document.fonts.ready.then(recalibrate, function () {});
    } catch (e) {}
  }

  window[NS] = {
    version: CFG.version,

    // Also repairs the HUD: if something cleared the page's nodes, put them back.
    alive: function () {
      if (!document.getElementById(STYLE_ID))
        (document.head || document.documentElement).appendChild(style);
      if (!document.getElementById(ROOT_ID)) {
        if (!document.body) return false;
        document.body.appendChild(root);
        seedFromScreen();
        schedule();
      }
      return true;
    },

    destroy: function () {
      window.removeEventListener('message', onMessage);
      window.removeEventListener('resize', onResize);
      if (elsTimer) { clearTimeout(elsTimer); elsTimer = null; }
      if (root.parentNode)  root.parentNode.removeChild(root);
      if (style.parentNode) style.parentNode.removeChild(style);
      try { delete window[NS]; } catch (e) { window[NS] = undefined; }
    }
  };

  return 'installed ' + CFG.version;
})JS";

}  // namespace

std::string buildInstallScript()
{
    loadUserFontOnce();

    std::string cfg = "{";
    cfg += "\"version\":"      + json::quote(GTAW_OLDHUD_VERSION);
    cfg += ",\"css\":"         + json::quote(buildCss());
    cfg += ",\"font\":"        + json::quote(buildFontStack());
    cfg += ",\"placeholder\":" + json::quote(g_set.placeholder);
    cfg += ",\"serverName\":"  + json::quote(g_set.serverName);
    cfg += ",\"gamemodeName\":"+ json::quote(g_set.gamemodeName);
    cfg += ",\"fuelLabel\":"   + json::quote(g_set.fuelLabel);
    cfg += ",\"units\":"       + json::quote(g_set.units);
    cfg += ",\"maxPlayers\":"  + std::to_string(g_set.maxPlayers);
    cfg += ",\"showCash\":"     + std::string(g_set.showCash     ? "true" : "false");
    cfg += ",\"showBank\":"     + std::string(g_set.showBank     ? "true" : "false");
    cfg += ",\"showLocation\":" + std::string(g_set.showLocation ? "true" : "false");
    cfg += ",\"showSpeed\":"    + std::string(g_set.showSpeed    ? "true" : "false");
    cfg += ",\"showFuel\":"     + std::string(g_set.showFuel     ? "true" : "false");
    cfg += ",\"showOdometer\":" + std::string(g_set.showOdometer ? "true" : "false");
    cfg += ",\"showAviation\":" + std::string(g_set.showAviation ? "true" : "false");
    cfg += ",\"showStaff\":"    + std::string(g_set.showStaff    ? "true" : "false");
    cfg += ",\"showEls\":"      + std::string(g_set.showEls      ? "true" : "false");
    cfg += ",\"elsHoldMs\":"    + std::to_string(g_set.elsHoldMs);
    cfg += ",\"showFooter\":"   + std::string(g_set.showFooter   ? "true" : "false");
    cfg += ",\"showPlayers\":"  + std::string(g_set.showPlayers  ? "true" : "false");
    cfg += ",\"showTime\":"     + std::string(g_set.showTime     ? "true" : "false");
    cfg += ",\"fontScale\":"    + num(g_set.fontScale, 4);
    cfg += ",\"fontWeight\":"   + std::to_string(g_set.fontWeight);
    cfg += ",\"dockX\":"        + num(g_set.dockX, 5);
    cfg += ",\"dockY\":"        + num(g_set.dockY, 5);
    cfg += "}";

    return std::string("(") + kScript + ")(" + cfg + ")";
}

std::string buildProbeScript()
{
    return "(function(){var a=window.__gtawOldHud;"
           "return !!(a && typeof a.alive === 'function' && a.alive());})()";
}

}  // namespace hud
