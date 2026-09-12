// Drives the injected payload against the stub DOM with the messages the HUD receives in game.
// Checks behaviour, text and placement.
//
// Usage: node tests/payload_test.js <extracted-script.js>
'use strict';
const fs = require('fs');
const dom = require('./dom');
const { section, check, done } = require('./assert');

const { window, document, body, flush, postLikeGtaw: post, widget } = dom;

const W = window.innerWidth, H = window.innerHeight;   // 1600 x 900

// ---- the current HUD, still on the page because it is only made transparent ----------------
const cashSlot = widget('rightBlockSlot rightBlockSlot--cash');
widget('trVal', '$190', cashSlot);
const bankSlot = widget('rightBlockSlot rightBlockSlot--bank');
widget('trVal', '$189,256', bankSlot);

const locBar = widget('locBar');
widget('locZone', 'Davis', locBar);
widget('locStreet', 'Brouge Avenue / Carson Avenue', locBar);
widget('wxTime', '14:07', locBar);

// The compass sits at the minimap's right edge plus max(10, width * 0.06), and is a fifth of
// the minimap wide. A 270px minimap therefore sits with its right edge at 299, and the compass
// at 315.2 by 54 wide.
const compass = widget('compassW');
compass._rect = { left: 315.2, top: 900, width: 54, height: 40 };

const brand = widget('brandBlock');
widget('brandVersion', 'v3.8.2', brand);
widget('brandPlayers', '188', brand);

// ---- run the payload -------------------------------------------------------------------------
const js = fs.readFileSync(process.argv[2], 'utf8');
const CFG = {
  version: '1.0.0', css: '/* css */',
  font: '"Roboto Condensed",sans-serif',
  placeholder: '—',
  serverName: 'GTA.WORLD', gamemodeName: 'Roleplay', maxPlayers: 1500,
  fuelLabel: 'Fuel', units: 'imperial',
  showCash: true, showBank: true, showLocation: true,
  showSpeed: true, showFuel: true, showOdometer: true, showAviation: true,
  showStaff: true, showEls: true, elsHoldMs: 5000,
  showFooter: true, showPlayers: true, showTime: true,
  fontScale: 1.0, fontWeight: 400, dockX: 0.0, dockY: 0.0,
};
const install = eval('(' + js + ')');
const installResult = install(CFG);
flush();

const root = () => document.getElementById('gtaw-oldhud-root');

// The lines carry no class of their own, so they are picked out by index in creation order.
const LINES = ['cash', 'bank', 'admin', 'tester', 'els', 'elsState', 'avi', 'speed', 'fuel',
               'odo', 'sdiv', 'card', 'cdiv', 'zone', 'street', 'footer'];
const line = (name) => root().childNodes[LINES.indexOf(name)];
const text = (name) => line(name).textContent;
const px = (v) => Math.round(parseFloat(v) * 100) / 100;
// Cap height the game would have drawn at this text scale, in pixels.
const cap0 = (s) => 0.03444 * s * H;
// Pushes a render through without changing any state, so a test can see what the HUD draws
// after moving the clock.
const tick = () => { post({ action: 'SET_HUD_VISIBLE', data: true }); flush(); };

section('install');
check('return value', installResult, 'installed 1.0.0');

section('seeded from what was already on screen');
check('cash',    text('cash'), '$190');
check('bank',    text('bank'), '$189,256');
check('zone',    text('zone'), 'Davis');
check('street',  text('street'), 'Brouge Avenue / Carson Avenue');
check('the compass letter is not seeded, it is localised', text('card'), '');
check('unknown compass is hidden', line('card').style.display, 'none');
check('footer', text('footer'), 'GTA.WORLD — Roleplay v3.8.2 — 188/1500 — 14:07');

section('the old HUD text, not the current HUD text');
post({ action: 'HUD::SET_STATE', data: { cardinal: 'N' } });
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State',
       data: { speed: 47.4, kms: 1.0823, measure: false, fuel: 85, maxFuel: 100 } });
flush();
check('speed reads label then number', text('speed'), 'MPH 47');
check('the number is the blue one',    line('speed').childNodes[1].className, 'ohud-b');
check('odometer', text('odo'), '1.08 mi.');
check('the divider above the compass', text('sdiv'), '|');
check('the divider beside it',         text('cdiv'), '|');

section('the fuel bar, one segment per ten percent');
const fuelBar = () => text('fuel');
check('85%', fuelBar(), 'Fuel |||||||||');
post({ action: 'NUI::Speedometer::State', data: { fuel: 100 } }); flush();
check('full',  fuelBar(), 'Fuel ||||||||||');
check('two red',    line('fuel').childNodes[1].textContent, '||');
check('three orange', line('fuel').childNodes[2].textContent, '|||');
check('five green',   line('fuel').childNodes[3].textContent, '|||||');
post({ action: 'NUI::Speedometer::State', data: { fuel: 90 } }); flush();
check('90 is not more than 90', fuelBar(), 'Fuel |||||||||');
post({ action: 'NUI::Speedometer::State', data: { fuel: 45 } }); flush();
check('45%', fuelBar(), 'Fuel |||||');
check('no green left', line('fuel').childNodes[3].textContent, '');
post({ action: 'NUI::Speedometer::State', data: { fuel: 5 } }); flush();
check('one red', fuelBar(), 'Fuel |');
post({ action: 'NUI::Speedometer::State', data: { fuel: 0 } }); flush();
check('empty', fuelBar(), 'Fuel EMPTY');
post({ action: 'NUI::Speedometer::State', data: { fuel: 48, maxFuel: 60 } }); flush();
check('a smaller tank is still a percentage', fuelBar(), 'Fuel ||||||||');

section('a fuel reading that has not arrived is not drawn as a guess');
install(CFG);
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { speed: 30 } });
flush();
check('no fuel line', line('fuel').style.display, 'none');
check('but the speed is there', line('speed').style.display, '');
post({ action: 'NUI::Speedometer::State', data: { fuel: 85, maxFuel: 100 } });
flush();
check('and it appears once it arrives', line('fuel').style.display, '');

section('on duty, the way the old HUD drew it');
check('nothing by default',   line('admin').style.display, 'none');
check('nor tester',           line('tester').style.display, 'none');
post({ action: 'NUI::Hud::ToggleTip', data: { tip: 'admin', visible: true } });
post({ action: 'NUI::Hud::ToggleTip', data: { tip: 'support', visible: true } });
flush();
check('Admin-Duty',  text('admin'),  'Admin-Duty');
check('Tester-Duty', text('tester'), 'Tester-Duty');
check('admin shown',  line('admin').style.display, '');
check('tester shown', line('tester').style.display, '');
check('centred with the balances', px(line('admin').style.left), px(line('cash').style.left));
check('0.140 down', px(line('admin').style.top),
      px(0.140 * H + 0.4123 * cap0(0.5) - (cap0(0.5) / 0.711) * 0.1305));
check('0.175 down', px(line('tester').style.top),
      px(0.175 * H + 0.4123 * cap0(0.5) - (cap0(0.5) / 0.711) * 0.1305));
check('fully opaque, unlike the rest', line('admin').className.includes('ohud-solid'), true);
post({ action: 'NUI::Hud::ToggleTip', data: { tip: 'admin', visible: false } });
post({ action: 'NUI::Hud::ToggleTip', data: { tip: 'support', visible: false } });
flush();
check('off again', line('admin').style.display, 'none');
post({ action: 'NUI::Hud::ToggleTip', data: { tip: 'nonsense', visible: true } });
flush();
check('an unknown tip is ignored', line('admin').style.display, 'none');

section('the ELS state');
install(CFG);
post({ action: 'NUI::Speedometer::Show' });
flush();
check('nothing in an ordinary vehicle', line('els').style.display, 'none');
post({ action: 'NUI::Speedometer::State', data: { elsVisible: true, elsCode: 1 } });
flush();
check('the label', text('els'), 'ELS');
check('off',       text('elsState'), 'OFF');
check('in red',    line('elsState').className.includes('ohud-r'), true);
check('shown',     line('els').style.display, '');
post({ action: 'NUI::Speedometer::State', data: { elsCode: 2 } });
flush();
check('lights',        text('elsState'), 'LIGHTS');
check('in yellow',     line('elsState').className.includes('ohud-y'), true);
post({ action: 'NUI::Speedometer::State', data: { elsCode: 3 } });
flush();
check('siren',     text('elsState'), 'SIREN');
check('in green',  line('elsState').className.includes('ohud-g'), true);
check('the state sits one line under the label',
      px(parseFloat(line('elsState').style.top) - parseFloat(line('els').style.top)),
      px(0.07 * 0.6 * H));
check('centred with the balances', px(line('els').style.left), px(line('cash').style.left));

section('the ELS state goes away again, as it did before');
install(Object.assign({}, CFG, { elsHoldMs: 0 }));
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { elsVisible: true, elsCode: 3 } });
flush();
check('a zero hold keeps it up', line('els').style.display, '');
install(CFG);
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { elsVisible: true, elsCode: 3 } });
flush();
check('up on a change', line('els').style.display, '');
dom.advanceClock(6000); tick();
check('gone five seconds later', line('els').style.display, 'none');
post({ action: 'NUI::Speedometer::State', data: { elsCode: 2 } });
flush();
check('back on the next change', line('els').style.display, '');
dom.advanceClock(6000);
post({ action: 'NUI::Speedometer::State', data: { elsCode: 2 } });
flush();
check('an update that changes nothing does not revive it', line('els').style.display, 'none');
dom.resetClock();

section('the aviation line');
install(CFG);
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State',
       data: { speed: 47.4, kms: 1.0823, measure: false, fuel: 85, maxFuel: 100 } });
flush();
check('hidden on the ground', line('avi').style.display, 'none');
post({ action: 'NUI::Speedometer::State',
       data: { isPlane: true, altitude: 412.4, heading: 271, atcOnline: false } });
flush();
check('shown in the air', line('avi').style.display, '');
check('text', text('avi'), 'ALT: 412 ft. / HDG: 271');
post({ action: 'NUI::Speedometer::State', data: { atcOnline: true } });
flush();
check('with ATC', text('avi'), 'ALT: 412 ft. / HDG: 271 / ATC: ONLINE');
check('ONLINE is green', line('avi').childNodes[5].className, 'ohud-g');

section('HUD::SET_STATE merges, it does not replace');
post({ action: 'HUD::SET_STATE', data: { cash: 250 } });
flush();
check('cash updated', text('cash'), '$250');
check('bank kept',    text('bank'), '$189,256');

section('a batch, whose children are also delivered again on their own');
post({ action: 'NUI::Global::Batch', data: [
  { action: 'HUD::SET_STATE', data: { cash: 1000 } },
  { action: 'HUD::SET_STATE', data: { version: 'v3.8.3' } },
] });
flush();
check('cash after double delivery', text('cash'), '$1,000');
check('version updates without a duplicate v',
      text('footer'), 'GTA.WORLD — Roleplay v3.8.3 — 188/1500 — 14:07');

section('leaving the vehicle');
post({ action: 'NUI::Speedometer::Hide' });
flush();
for (const n of ['speed', 'fuel', 'odo', 'sdiv', 'avi'])
  check(`${n} hidden`, line(n).style.display, 'none');

section('units = auto follows the in-game speedometer');
install(Object.assign({}, CFG, { units: 'auto' }));
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { speed: 76, kms: 1.0823, measure: true } });
flush();
check('the speed already arrived in km/h', text('speed'), 'KMH 76');
check('the odometer arrives in miles and is converted', text('odo'), '1.74 km.');

section('units = metric converts a speed that arrived in mph');
install(Object.assign({}, CFG, { units: 'metric' }));
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { speed: 47, kms: 1.0823, measure: false } });
flush();
check('converted', text('speed'), 'KMH 76');

section('the odometer is drawn only inside the range the old HUD drew it in');
install(CFG);
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State', data: { kms: 0 } });
flush();
check('a standing-still zero is not drawn', line('odo').style.display, 'none');
post({ action: 'NUI::Speedometer::State', data: { kms: 50000 } });
flush();
check('nor is a nonsense reading', line('odo').style.display, 'none');

section('a property name replaces the whole location block with one line');
install(CFG);
post({ action: 'HUD::SET_STATE',
       data: { zone: 'Davis', street: 'Grove Street', propertyName: 'Mirror Place 14',
               cardinal: 'N' } });
flush();
check('the property goes where the street went', text('street'), 'Mirror Place 14');
check('zone hidden',    line('zone').style.display, 'none');
check('compass hidden', line('card').style.display, 'none');
check('divider hidden', line('cdiv').style.display, 'none');
post({ action: 'HUD::SET_STATE', data: { propertyName: null } });
flush();
check('zone restored',   text('zone'), 'Davis');
check('street restored', text('street'), 'Grove Street');
check('compass restored', line('card').style.display, '');

section('nothing received yet renders the placeholder, never a zero');
body.removeChild(cashSlot); body.removeChild(bankSlot);
install(CFG);
flush();
check('unknown cash', text('cash'), '—');
check('unknown bank', text('bank'), '—');
post({ action: 'HUD::SET_STATE', data: { cash: 0 } });
flush();
check('a real zero', text('cash'), '$0');
body.appendChild(cashSlot); body.appendChild(bankSlot);

section('placement against the minimap');
install(CFG);
flush();
// No rectangle yet, so the minimap is worked back out of the compass: a 54px compass is a fifth
// of a 270px minimap, whose right edge is 315.2 - max(10, 270 * 0.06) = 299.
check('compass fallback puts the divider at the right edge + 0.038',
      px(line('cdiv').style.left), px(299 + 0.038 * W));

post({ action: 'NUI::Hud::RadarRect',
       data: { left: 20, top: 725, width: 325, height: 155, screenW: 1600, screenH: 900 } });
flush();
// 16:9, so the inset is width * 0.029 = 9.425, and the drawn height 325/1.4185 = 229.1 exceeds
// the reported 155, so the top is left alone. The right edge is therefore 20 + 9.425 + 325.
const right = 20 + 9.425 + 325, bottom = 725 + 155;
check('the compass letter is centred 0.025 right of the edge',
      px(line('card').style.left), px(right + 0.025 * W));
check('the street sits 0.045 right of it',
      px(line('street').style.left), px(right + 0.045 * W));
check('the speed sits 0.006 right of it',
      px(line('speed').style.left), px(right + 0.006 * W));

// Cap height at scale 1 is 0.03444 of the screen, the cap top sits 0.4123 cap heights below the
// top of the game's text cell, and the stub font puts the cap top 0.1305em below the element.
const cap = cap0;
const top = (spec, s) => bottom - spec * H + 0.4123 * cap(s) - (cap(s) / 0.711) * 0.1305;
check('the compass letter sits 0.046 above the bottom edge',
      px(line('card').style.top), px(top(0.046, 1.0)));
check('the zone sits 0.040 above it, at half the scale',
      px(line('zone').style.top), px(top(0.040, 0.5)));
check('the street 0.016 above it',
      px(line('street').style.top), px(top(0.016, 0.4)));
check('the font size draws capitals the right height',
      px(line('card').style.fontSize), px(cap(1.0) / 0.711));

section('the big map slides the whole block right');
post({ action: 'NUI::Hud::ToggleBigmap', data: { active: true } });
flush();
check('shifted', px(line('card').style.left), px(right + 0.025 * W + 0.089 * W));
post({ action: 'NUI::Hud::ToggleBigmap', data: { active: false } });
flush();

section('balances and the server line are placed against the screen');
// Below 1920 wide the old HUD used the real screen size, so the inset is a flat 70px.
check('balances are centred 70px in', px(line('cash').style.left), W - 70);
check('cash at 0.060 down', px(line('cash').style.top), px(0.060 * H + 0.4123 * cap(0.8) - (cap(0.8) / 0.711) * 0.1305));
check('bank at 0.100 down', px(line('bank').style.top), px(0.100 * H + 0.4123 * cap(0.7) - (cap(0.7) / 0.711) * 0.1305));
check('the server line is centred', px(line('footer').style.left), W / 2);
check('35px up',  px(line('footer').style.top), px(H - 35 + 0.4123 * cap(0.45) - (cap(0.45) / 0.711) * 0.1305));

section('a nonsense rectangle falls back to the compass again');
post({ action: 'NUI::Hud::RadarRect', data: { left: 0, top: 0, width: 0, height: 0 } });
flush();
check('back to the compass', px(line('cdiv').style.left), px(299 + 0.038 * W));

section('with no minimap at all, the docked block is not drawn');
body.removeChild(compass);
install(CFG);
flush();
check('compass line hidden', line('card').style.display, 'none');
check('speed hidden',        line('speed').style.display, 'none');
check('but the balances stay', line('cash').style.display, '');
check('and the server line',   line('footer').style.display, '');
body.appendChild(compass);

section('the parts can be switched off');
install(Object.assign({}, CFG, { showPlayers: false, showTime: false }));
post({ action: 'HUD::SET_STATE', data: { version: '1.8.8a', players: 188, time: '14:07' } });
flush();
check('a short server line', text('footer'), 'GTA.WORLD — Roleplay v1.8.8a');

section('the weight reaches the measurement, not just the stylesheet');
let seenFont = '';
dom.setFontProbe((f) => { seenFont = f; });
install(Object.assign({}, CFG, { fontWeight: 600 }));
flush();
check('the canvas shorthand carries it', /^600 /.test(seenFont), true);
dom.setFontProbe(null);
install(CFG);
flush();

section('without canvas metrics it falls back to the built-in ratios');
dom.setFontMetrics(null);
install(CFG);
flush();
check('still sized', px(line('card').style.fontSize), px(cap(1.0) / 0.716));
dom.setFontMetrics({ cap: 0.711, ascent: 0.927, descent: 0.244 });

section('junk is ignored');
install(CFG);
post({ action: 'HUD::SET_STATE', data: { cash: 7 } });
flush();
const before = text('cash');
post({ action: 'HUD::SET_STATE', data: null });
post({ action: 'Some::Other::Thing', data: { cash: 999999 } });
post('not json at all');
post(JSON.stringify({ action: 'HUD::SET_STATE', data: { bank: 5 } }));
post(null);
post({ action: 'NUI::Global::Batch', data: 'not an array' });
flush();
check('cash untouched by junk',        text('cash'), before);
check('a JSON string is still parsed', text('bank'), '$5');

section('visibility');
post({ action: 'NUI::Hud::PauseMenu', data: { active: true } });
flush();
check('hidden under the pause menu', root().style.opacity, '0');
post({ action: 'NUI::Hud::PauseMenu', data: { active: false } });
flush();
check('back again', root().style.opacity, '1');

post({ action: 'SET_HUD_VISIBLE', data: false });
flush();
check('hidden with the HUD', root().style.opacity, '0');
post({ action: 'SET_HUD_VISIBLE', data: true });
flush();

post({ action: 'NUI::Hud::InactiveFade', data: { opaque: false, ms: 400 } });
flush();
check('faded out',     root().style.opacity, '0');
check('fade duration', root().style.transitionDuration, '400ms');
post({ action: 'NUI::Hud::InactiveFade', data: { opaque: true, ms: 400 } });
flush();
check('faded back in', root().style.opacity, '1');

section('alive() repairs a HUD that was removed from the page');
const api = window.__gtawOldHud;
body.removeChild(root());
check('gone', document.getElementById('gtaw-oldhud-root'), 'null');
check('alive() reports true', api.alive(), 'true');
flush();
check('back on the page', document.getElementById('gtaw-oldhud-root') !== null, 'true');

section('destroy() leaves nothing behind');
api.destroy();
check('root removed',   document.getElementById('gtaw-oldhud-root'), 'null');
check('style removed',  document.getElementById('gtaw-oldhud-style'), 'null');
check('global removed', window.__gtawOldHud, 'undefined');
check('message listener removed', (dom.listeners.message || []).length, 0);
check('resize listener removed',  (dom.listeners.resize  || []).length, 0);

done();
