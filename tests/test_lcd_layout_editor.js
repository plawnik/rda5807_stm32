"use strict";

const assert = require("assert");
const fs = require("fs");
const path = require("path");

const editorPath = path.join(__dirname, "..", "tools", "lcd-layout-editor", "index.html");
const html = fs.readFileSync(editorPath, "utf8");
const scripts = [...html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi)];

assert.strictEqual(scripts.length, 1, "editor must have one self-contained inline script");
assert(!/<script[^>]+src=/i.test(html), "editor must not load external scripts");
assert(!/<link[^>]+rel=["']stylesheet/i.test(html), "editor must not load external stylesheets");
assert(!/\bfetch\s*\(/.test(scripts[0][1]), "editor must not require network access");
assert(/const WIDTH = 84;/.test(scripts[0][1]), "display width must remain 84 pixels");
assert(/const HEIGHT = 48;/.test(scripts[0][1]), "display height must remain 48 pixels");
assert(/const FORMAT_NAME = "rda5807-pcd8544-layout";/.test(scripts[0][1]), "export format identifier is missing");
assert(/pcd8544PageBytes/.test(scripts[0][1]), "packed framebuffer export is missing");
assert(/bitmap\.rows/.test(scripts[0][1]), "human-readable bitmap import is missing");

const ids = [...html.matchAll(/\bid="([^"]+)"/g)].map(match => match[1]);
assert.strictEqual(new Set(ids).size, ids.length, "HTML contains duplicate element IDs");
for (const required of [
  "screen", "regionList", "regionName", "regionDescription", "regionX",
  "regionY", "regionWidth", "regionHeight", "exportButton", "importButton",
  "fileInput"
]) {
  assert(ids.includes(required), `required editor control #${required} is missing`);
}

new Function(scripts[0][1]);
console.log("LCD layout editor static tests passed.");
