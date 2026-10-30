// node out.js  -> CatBox3_blueprinty.pdf + sheet_NN.png (from sheets.html / sheet_NN.svg)
const { chromium } = require('playwright'); const fs = require('fs'), path = require('path');
(async () => { const dir = __dirname; const b = await chromium.launch({ args: ['--disable-gpu'] });
  const p = await b.newPage({ viewport: { width: 1587, height: 1123 }, deviceScaleFactor: 2 });
  await p.goto('file://' + dir + '/sheets.html', { waitUntil: 'load' });
  await p.pdf({ path: dir + '/CatBox3_blueprinty.pdf', width: '420mm', height: '297mm', printBackground: true, timeout: 120000 });
  const files = fs.readdirSync(dir).filter((f) => /^sheet_\d+\.svg$/.test(f)).sort();
  for (const f of files) { await p.setContent('<html><body style="margin:0">' + fs.readFileSync(path.join(dir, f), 'utf8').replace('width="420mm" height="297mm"', 'width="1587" height="1123"') + '</body></html>');
    await p.screenshot({ path: path.join(dir, f.replace('.svg', '.png')), clip: { x: 0, y: 0, width: 1587, height: 1123 }, timeout: 120000 }); }
  await b.close(); })();
