// Runs the exact expression the C++ builds, rather than a hand-written copy of it, so the real
// generated stylesheet and config are what gets exercised.
//
// Usage: node tests/generated_test.js <install-script.js>
'use strict';
const fs = require('fs');
const dom = require('./dom');
const { section, check, done } = require('./assert');

const { document, flush, post, widget } = dom;
dom.window.innerWidth = 1920;
dom.window.innerHeight = 1080;

// The compass of a 270px minimap, which is the size drawn at 1920 by 1080.
const compass = widget('compassW');
compass._rect = { left: 315.2, top: 1000, width: 54, height: 40 };

const script = fs.readFileSync(process.argv[2], 'utf8');
const result = eval(script);
flush();

const root = () => document.getElementById('gtaw-oldhud-root');
const LINES = ['cash', 'bank', 'admin', 'tester', 'els', 'elsState', 'avi', 'speed', 'fuel',
               'odo', 'sdiv', 'card', 'cdiv', 'zone', 'street', 'footer'];
const line = (name) => root().childNodes[LINES.indexOf(name)];
const text = (name) => line(name).textContent;

section('the generated expression runs');
check('install', result, 'installed 1.1.0');

const css = document.getElementById('gtaw-oldhud-style').textContent;

section('the generated stylesheet');
check('braces balance', (css.match(/{/g) || []).length === (css.match(/}/g) || []).length, 'true');
check('root rule present', css.includes('#gtaw-oldhud-root{'), 'true');
check('the alpha the game drew this text at', /--ohud-alpha:0\.784;/.test(css), 'true');
// The decimal separator follows the process locale in C, and a comma here would silently break
// every number in the sheet.
check('decimal separator is a dot', /:\s*\d+,\d+(vh|vw|px|;)/.test(css) === false, 'true');

const used = new Set([...css.matchAll(/var\((--[a-z0-9-]+)/g)].map(m => m[1]));
const defined = new Set([...css.matchAll(/(--[a-z0-9-]+)\s*:/g)].map(m => m[1]));
const setAtRuntime = new Set(['--ohud-sx']);
const missing = [...used].filter(v => !defined.has(v) && !setAtRuntime.has(v));
check('no undefined custom properties', missing.length ? missing.join(',') : 'none', 'none');

section('every rule is scoped so it cannot leak into the page');
const bare = css.replace(/\/\*[\s\S]*?\*\//g, '');
const hideBare = document.getElementById('gtaw-oldhud-hide').textContent;
const selectors = [...(bare + hideBare).matchAll(/(^|})\s*([^{}@]+){/g)].map(m => m[2].trim());
const unscoped = selectors.filter(sel =>
  !sel.split(',').every(part => part.trim().startsWith('#gtaw-oldhud-root') ||
                                part.trim().startsWith('.rightBlockSlot') ||
                                part.trim().startsWith('.locBar') ||
                                part.trim().startsWith('.compassW') ||
                                part.trim().startsWith('.brandBlock') ||
                                part.trim().startsWith('.wxBar') ||
                                part.trim().startsWith('.tips') ||
                                part.trim().startsWith('.mmb-') ||
                                part.trim().startsWith('.speedSign') ||
                                part.trim().startsWith('.spd-root')));
check('no stray global selectors', unscoped.length ? unscoped.join(' | ') : 'none', 'none');

section('the hide rules target the current HUD');
// They are a stylesheet of their own, so the script can switch them off while the layout
// editor is open.
const hide = document.getElementById('gtaw-oldhud-hide').textContent;
for (const sel of ['.rightBlockSlot--cash', '.rightBlockSlot--bank', '.locBar', '.compassW',
                   '.brandBlock', '.spd-root', '.wxBar', '.tips', '.mmb-bar', '.speedSign'])
  check(`hides ${sel}`, hide.includes(sel), 'true');
check('by transparency, not display:none', /opacity:0 !important/.test(hide), 'true');
check('so the widgets stay laid out and readable', /display:\s*none !important/.test(hide), 'false');
check('and not in the main sheet', /opacity:0 !important/.test(css), 'false');

section('the font');
const faces = [...css.matchAll(/@font-face\{font-family:"([^"]+)"/g)].map(m => m[1]);
check('one face is embedded', faces.join(','), 'GTAWOldHudCondensed');
check('compiled in as a data URI', /src:url\(data:font\/ttf;base64,[A-Za-z0-9+/=]{40000,}\) format\("truetype"\)/.test(css), 'true');
// A range rather than a single weight is what makes the browser instance the variable font at
// whatever font_weight asks for instead of synthesising a heavier one.
check('the weight axis is declared', /@font-face\{font-family:"GTAWOldHudCondensed";font-style:normal;font-weight:200 700;/.test(css), 'true');
check('and the default is regular', /--ohud-weight:400;/.test(css), 'true');
check('which the root draws at', /font-weight:var\(--ohud-weight\)/.test(css), 'true');
const stack = /--ohud-font:([^;]+);/.exec(css)[1];
check('the game\'s own face is asked for first', stack.startsWith('"ChaletComprime-CologneSixty"'), 'true');
check('then the embedded one', stack.indexOf('GTAWOldHudCondensed') > 0, 'true');
check('and it is measured, not assumed', /transform:scaleX\(var\(--ohud-sx,1\)\)/.test(css), 'true');

section('the script and the stylesheet name the same font stack');
check('the measured stack is the drawn stack', script.includes(JSON.stringify(stack)), 'true');

section('nothing is fetched, at build time or at run time');
// The plugin opens one socket, to loopback. A stylesheet or a script that reaches out for a
// font, an icon or anything else would be an outbound connection from inside the game's
// browser, which this must never make.
const urls = [...script.matchAll(/url\(\s*['"]?([^'")]+)/g)].map(m => m[1].trim());
const external = urls.filter(u => !u.startsWith('data:'));
check('every url() is a data: URI', external.length ? external.join(' | ') : 'none', 'none');
check('no @import', /@import/.test(script), 'false');
check('no http(s) anywhere in what gets injected', /https?:\/\//.test(script), 'false');
check('no protocol-relative URL either', /['"(]\/\/[a-z0-9.-]+\//i.test(script), 'false');
check('nothing is loaded at run time', /\b(fetch|XMLHttpRequest|WebSocket|EventSource|importScripts)\b/.test(script), 'false');

section('it draws the old HUD from real messages');
post({ action: 'HUD::SET_STATE', data: {
  cash: 190, bank: 189256, street: 'Brouge Avenue / Carson Avenue', zone: 'Davis',
  propertyName: null, cardinal: 'N', time: '14:07', version: '1.8.8a', players: 188 } });
post({ action: 'NUI::Speedometer::Show' });
post({ action: 'NUI::Speedometer::State',
       data: { speed: 0, kms: 1.08, measure: false, fuel: 85, maxFuel: 100 } });
flush();

check('cash',     text('cash'), '$190');
check('bank',     text('bank'), '$189,256');
check('cardinal', text('card'), 'N');
check('zone',     text('zone'), 'Davis');
check('street',   text('street'), 'Brouge Avenue / Carson Avenue');
check('speed',    text('speed'), 'MPH 0');
check('fuel',     text('fuel'), 'Fuel |||||||||');
check('odometer', text('odo'), '1.08 mi.');
check('footer',   text('footer'), 'GTA.WORLD — Roleplay v1.8.8a — 188/1500 — 14:07');

section('and places it where the old HUD placed it');
// At or above 1920 wide the old HUD kept measuring the balance inset against 1920.
check('balances centred at (1920 - 70) / 1920 of the width',
      Math.round(parseFloat(line('cash').style.left)), 1850);
check('the compass letter is centred 0.025 right of the minimap',
      Math.round(parseFloat(line('card').style.left)), Math.round(299 + 0.025 * 1920));

done();
