// Cat Box Pocket v2: product shots of the box, built from the layout (three.js r128, headless Chromium).
//   node render.js <scratch dir> <out dir> [w h] [views]
const { chromium } = require('playwright');
const fs = require('fs');
const S = process.argv[2], OUT = process.argv[3];
(async () => {
  const screen = 'data:image/png;base64,' + fs.readFileSync(S + '/prender/screen.png').toString('base64');
  const W = +(process.argv[4] || 2400), H = +(process.argv[5] || 1600), SS = 1.5;
  const b = await chromium.launch({ args: ['--use-gl=swiftshader', '--enable-webgl', '--ignore-gpu-blocklist'] });
  const p = await b.newPage({ viewport: { width: Math.round(W * SS), height: Math.round(H * SS) } });
  p.on('pageerror', e => console.log('ERR', e.message)); p.on('console', m => console.log('LOG', m.text()));
  await p.setContent('<html><body style="margin:0;background:#000"><script>' + fs.readFileSync(S + '/krabica2/node_modules/three/build/three.min.js', 'utf8') + '</script></body></html>');
  const views = process.argv[6] ? process.argv[6].split(',') : ['hero', 'top', 'back', 'hand'];
  for (const view of views) {
    const png = await p.evaluate(async ({ screen, view, w, h }) => {
      document.body.innerHTML = '';
      const img = await new Promise((ok) => { const im = new Image(); im.onload = () => ok(im); im.src = screen; });
      const r = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
      r.setSize(w, h); r.setPixelRatio(1); document.body.appendChild(r.domElement);
      r.outputEncoding = THREE.sRGBEncoding; r.toneMapping = THREE.ACESFilmicToneMapping; r.toneMappingExposure = 1.0;
      r.shadowMap.enabled = true; r.shadowMap.type = THREE.PCFSoftShadowMap;
      const lin = (m) => { if (m.color) m.color.convertSRGBToLinear(); if (m.emissive) m.emissive.convertSRGBToLinear(); return m; };
      const mat = (c, rough = 0.6, metal = 0, extra = {}) => lin(new THREE.MeshStandardMaterial(Object.assign({ color: c, roughness: rough, metalness: metal }, extra)));
      const sc = new THREE.Scene();
      // studio sweep
      const bg = document.createElement('canvas'); bg.width = 16; bg.height = 512;
      const bgc = bg.getContext('2d'); const gr = bgc.createLinearGradient(0, 0, 0, 512);
      gr.addColorStop(0, '#d9d6cf'); gr.addColorStop(0.6, '#c4c0b8'); gr.addColorStop(1, '#aaa59c');
      bgc.fillStyle = gr; bgc.fillRect(0, 0, 16, 512);
      const bgt = new THREE.CanvasTexture(bg); bgt.encoding = THREE.sRGBEncoding; sc.background = bgt;
      const floor = new THREE.Mesh(new THREE.PlaneGeometry(4000, 4000), mat(0xcfcbc3, 0.95));
      floor.receiveShadow = true; floor.position.set(62, 81, -0.05); sc.add(floor);
      const shc = document.createElement('canvas'); shc.width = 512; shc.height = 512; const shg = shc.getContext('2d');
      const rg = shg.createRadialGradient(256, 256, 60, 256, 256, 256); rg.addColorStop(0, 'rgba(0,0,0,0.55)'); rg.addColorStop(0.6, 'rgba(0,0,0,0.25)'); rg.addColorStop(1, 'rgba(0,0,0,0)');
      shg.fillStyle = rg; shg.fillRect(0, 0, 512, 512);
      const shadow = new THREE.Mesh(new THREE.PlaneGeometry(175, 215), new THREE.MeshBasicMaterial({ map: new THREE.CanvasTexture(shc), transparent: true, depthWrite: false }));
      shadow.position.set(62, 81, 0.02); sc.add(shadow);
      sc.add(new THREE.HemisphereLight(0xf4f1ff, 0x5a5048, 0.55));
      const key = new THREE.DirectionalLight(0xfff4e8, 1.25); key.position.set(-110, -150, 420); key.target.position.set(62, 81, 0);
      key.castShadow = true; key.shadow.mapSize.set(4096, 4096); key.shadow.bias = -0.0003; key.shadow.radius = 5;
      Object.assign(key.shadow.camera, { left: -150, right: 150, top: 150, bottom: -150, near: 10, far: 900 });
      sc.add(key, key.target);
      const rim = new THREE.DirectionalLight(0xcfe0ff, 0.55); rim.position.set(260, 380, 140); sc.add(rim);
      const fill = new THREE.DirectionalLight(0xffe0c0, 0.25); fill.position.set(380, -60, 90); sc.add(fill);

      const add = (parent, g, m, cast = true) => { const o = new THREE.Mesh(g, m); o.castShadow = cast; o.receiveShadow = true; parent.add(o); return o; };
      const rrect = (w, hh, rad) => {
        const s = new THREE.Shape(), x = -w / 2, y = -hh / 2;
        s.moveTo(x + rad, y); s.lineTo(x + w - rad, y); s.quadraticCurveTo(x + w, y, x + w, y + rad); s.lineTo(x + w, y + hh - rad);
        s.quadraticCurveTo(x + w, y + hh, x + w - rad, y + hh); s.lineTo(x + rad, y + hh); s.quadraticCurveTo(x, y + hh, x, y + hh - rad);
        s.lineTo(x, y + rad); s.quadraticCurveTo(x, y, x + rad, y); return s;
      };
      const slab = (w, hh, d, rad, bevel) => new THREE.ExtrudeGeometry(rrect(w - 2 * bevel, hh - 2 * bevel, Math.max(0.3, rad - bevel)),
        { depth: d - 2 * bevel, bevelEnabled: true, bevelThickness: bevel, bevelSize: bevel, bevelSegments: 4, curveSegments: 10 }).translate(0, 0, bevel);

      // ---- the box: 124 x 162 x 22 mm, black PLA (x right, y towards the back edge, z up)
      const BW = 124, BH = 162, BD = 22;
      const box = new THREE.Group(); sc.add(box);
      // FDM layer lines on the sides: a bump map of horizontal lines
      const lc = document.createElement('canvas'); lc.width = 8; lc.height = 512; const lg = lc.getContext('2d');
      for (let y = 0; y < 512; y += 4) { lg.fillStyle = '#808080'; lg.fillRect(0, y, 8, 2); lg.fillStyle = '#9a9a9a'; lg.fillRect(0, y + 2, 8, 2); }
      const lt = new THREE.CanvasTexture(lc); lt.wrapS = lt.wrapT = THREE.RepeatWrapping;
      const bodyG = slab(BW, BH, BD, 9, 1.6).translate(BW / 2, BH / 2, 0);
      { const pa = bodyG.attributes.position, uv = bodyG.attributes.uv; for (let i = 0; i < pa.count; i++) uv.setXY(i, (pa.getX(i) + pa.getY(i)) / 40, pa.getZ(i) / 22 * 55); }
      const bodyM = mat(0x1b1b1e, 0.72, 0.02, { bumpMap: lt, bumpScale: 0.25 });
      add(box, bodyG, bodyM);
      const Z = BD;       // the top face
      const at = (x, dist) => [x, BH - dist];   // place by distance from the back edge

      // ---- the printed labels on the top (a light layer of filament)
      const PX = 10;  // canvas pixels per mm
      const lab = document.createElement('canvas'); lab.width = BW * PX; lab.height = BH * PX; const L = lab.getContext('2d');
      L.fillStyle = 'rgba(0,0,0,0)'; L.fillRect(0, 0, lab.width, lab.height);
      L.fillStyle = '#e9e4da'; L.textAlign = 'center'; L.textBaseline = 'middle';
      const txt = (s, x, dist, size, weight = 600, align = 'center', spacing = 0) => { L.font = `${weight} ${size * PX}px "DejaVu Sans", Arial, sans-serif`; L.textAlign = align; if (L.letterSpacing !== undefined) L.letterSpacing = spacing + 'px'; L.fillText(s, x * PX, dist * PX); };
      txt('A', 64, 25.8, 2.6); txt('B', 80, 25.8, 2.6); txt('C', 96, 25.8, 2.6); txt('D', 112, 25.8, 2.6);
      txt('A', 59.3, 33.2, 2.2, 600); txt('B', 116.7, 33.2, 2.2, 600);
      txt('CAT BOX', 6.2, 38.6, 3.4, 800, 'left', 4); txt('POCKET', 31.4, 38.6, 3.4, 300, 'left', 4);
      txt('USB-C', 30, 3.2, 1.6, 500); txt('PHONES', 50, 3.2, 1.6, 500); txt('LINE IN', 66, 3.2, 1.6, 500); txt('microSD', 86, 3.2, 1.6, 500); txt('OFF  ON', 106, 3.2, 1.6, 500);
      // a thin rule around the pad grid and the label strip at the bottom
      L.strokeStyle = 'rgba(233,228,218,0.55)'; L.lineWidth = 0.25 * PX;
      L.beginPath(); L.roundRect ? L.roundRect(24.4 * PX, 68.6 * PX, 75.2 * PX, 73.8 * PX, 2.4 * PX) : L.rect(24.4 * PX, 68.6 * PX, 75.2 * PX, 73.8 * PX); L.stroke();
      txt('16 PADS · 64 SOUNDS · 8 VOICES', 62, 159.0, 1.7, 500);
      const labT = new THREE.CanvasTexture(lab); labT.encoding = THREE.sRGBEncoding; labT.anisotropy = 16;
      const labM = new THREE.MeshStandardMaterial({ map: labT, transparent: true, roughness: 0.8, polygonOffset: true, polygonOffsetFactor: -2 });
      const labels = new THREE.Mesh(new THREE.PlaneGeometry(BW, BH), labM); labels.position.set(BW / 2, BH / 2, Z + 0.02); box.add(labels);

      // ---- the OLED 1.3": a window with a black glass, the real screen
      { const [x, y] = at(27, 20);
        add(box, slab(44, 27, 0.8, 2.5, 0.3).translate(x, y, Z - 0.6), mat(0x0a0a0b, 0.3, 0.1));
        const tex = new THREE.Texture(img); tex.needsUpdate = true; tex.magFilter = THREE.NearestFilter; tex.minFilter = THREE.LinearFilter; tex.encoding = THREE.sRGBEncoding;
        const scr = new THREE.Mesh(new THREE.PlaneGeometry(29.4, 14.7), new THREE.MeshStandardMaterial({ color: 0x000000, emissiveMap: tex, emissive: 0xd8ecff, emissiveIntensity: 1.25, roughness: 0.2 }));
        scr.position.set(x, y + 1.5, Z + 0.25); box.add(scr);
        const glass = new THREE.Mesh(new THREE.PlaneGeometry(42, 25), new THREE.MeshPhysicalMaterial({ color: 0xffffff, roughness: 0.04, transparent: true, opacity: 0.1, clearcoat: 1 }));
        glass.position.set(x, y, Z + 0.3); box.add(glass); }

      // ---- 4 encoders (knurled black aluminium knobs with a white line)
      const knobM = mat(0x161618, 0.34, 0.55), lineM = mat(0xf0ece2, 0.5);
      [64, 80, 96, 112].forEach((ex, i) => {
        const [x, y] = at(ex, 15);
        add(box, new THREE.CylinderGeometry(4.2, 4.2, 0.8, 6).rotateX(Math.PI / 2).translate(x, y, Z + 0.4), mat(0x9fa3a8, 0.3, 0.9));
        const g = new THREE.CylinderGeometry(5.6, 6.0, 11, 48, 1);
        { const pa = g.attributes.position; for (let k = 0; k < pa.count; k++) { const px = pa.getX(k), pz = pa.getZ(k), a = Math.atan2(pz, px), rr = Math.hypot(px, pz); const f = 1 + 0.025 * Math.cos(a * 24) * (rr > 1 ? 1 : 0); pa.setX(k, px * f); pa.setZ(k, pz * f); } g.computeVertexNormals(); }
        g.rotateX(Math.PI / 2).translate(x, y, Z + 1 + 5.5);
        add(box, g, knobM);
        const ang = [-0.7, 0.4, 1.4, -1.9][i];
        const ln = add(box, new THREE.BoxGeometry(0.9, 3.4, 0.2), lineM, false);
        ln.position.set(x - Math.sin(ang) * 3.2, y + Math.cos(ang) * 3.2, Z + 12.05); ln.rotation.z = ang;
      });

      // ---- the crossfader
      { const [x0, y] = at(88, 33);
        add(box, slab(50, 2.6, 0.6, 1.3, 0.2).translate(x0, y, Z - 0.4), mat(0x050506, 0.9));
        add(box, new THREE.BoxGeometry(2, 1.4, 4).translate(x0 - 14, y, Z + 1.6), mat(0x2a2a2c, 0.5, 0.4));
        add(box, slab(6.5, 11, 5, 1.4, 0.8).translate(x0 - 14, y, Z + 3), mat(0xe9e5dc, 0.45)); }

      // ---- keys: function caps (tactile switches, 16 x 10 mm caps with printed names) and Choc pads
      const capTex = (name, color) => {
        const c = document.createElement('canvas'); c.width = 320; c.height = 200; const g = c.getContext('2d');
        g.fillStyle = color; g.fillRect(0, 0, 320, 200);
        g.fillStyle = name === 'REC' ? '#ff5a4f' : name === 'PLAY' ? '#7ee08f' : '#ece8de';
        g.font = `700 ${name.length > 5 ? 62 : 70}px "DejaVu Sans", Arial, sans-serif`; g.textAlign = 'center'; g.textBaseline = 'middle';
        g.fillText(name, 160, 104);
        const t = new THREE.CanvasTexture(c); t.encoding = THREE.sRGBEncoding; t.anisotropy = 8; return t;
      };
      const fnKey = (name, x, dist) => {
        const [cx, cy] = at(x, dist);
        const col = '#3b3c41';
        const side = mat(0x3b3c41, 0.55);
        const top = lin(new THREE.MeshStandardMaterial({ map: capTex(name, col), roughness: 0.55 }));
        const g = slab(16, 10, 3.2, 1.6, 0.6).translate(cx, cy, Z + 0.4);
        add(box, g, side);
        const t = new THREE.Mesh(new THREE.PlaneGeometry(14.6, 8.6), top); t.position.set(cx, cy, Z + 3.62); box.add(t);
      };
      const padX = [35, 53, 71, 89];
      ['UNDO', 'SONG', 'KIT', 'MENU'].forEach((n, i) => fnKey(n, padX[i], 48.5));
      ['SOUND', 'PATTERN', 'BPM', 'FX'].forEach((n, i) => fnKey(n, padX[i], 60.5));
      ['MUTE', 'COPY', 'PASTE', 'CLEAR'].forEach((n, i) => fnKey(n, 14, 79 + 18 * i));
      ['FILL', 'ACCENT', 'SCENE', 'LEN'].forEach((n, i) => fnKey(n, 110, 79 + 18 * i));
      ['SHIFT', 'REC', 'PLAY', 'WRITE'].forEach((n, i) => fnKey(n, padX[i], 151));

      // pads: Choc 1u caps, off-white, lit from below (a playing pattern)
      const glowC = document.createElement('canvas'); glowC.width = 128; glowC.height = 128; const gg = glowC.getContext('2d');
      const gr2 = gg.createRadialGradient(64, 64, 20, 64, 64, 64); gr2.addColorStop(0, 'rgba(255,255,255,1)'); gr2.addColorStop(1, 'rgba(255,255,255,0)');
      gg.fillStyle = gr2; gg.fillRect(0, 0, 128, 128); const glowT = new THREE.CanvasTexture(glowC);
      const lit = { 0: 0xff8a2a, 4: 0xff8a2a, 8: 0xff8a2a, 12: 0xff8a2a, 14: 0xff8a2a, 6: 0x2fd27a, 9: 0xffffff };
      for (let j = 0; j < 4; ++j) for (let i = 0; i < 4; ++i) {
        const n = j * 4 + i, [cx, cy] = at(padX[i], 79 + 18 * j);
        add(box, new THREE.BoxGeometry(14.5, 13.5, 0.8).translate(cx, cy, Z + 0.4), mat(0x101012, 0.6));   // the switch under it
        const c = lit[n];
        const m = lin(new THREE.MeshPhysicalMaterial({ color: 0xe2ded6, roughness: 0.38, clearcoat: 0.25,
          emissive: c || 0x000000, emissiveIntensity: c ? (c === 0xffffff ? 0.6 : 0.95) : 0 }));
        const g = slab(17, 16.4, 3.6, 2.2, 1.0).translate(cx, cy, Z + 2.6);
        add(box, g, m);
        if (c) {
          const halo = new THREE.Mesh(new THREE.PlaneGeometry(23, 22.4), new THREE.MeshBasicMaterial({ map: glowT, color: c, transparent: true, opacity: c === 0xffffff ? 0.6 : 0.85, blending: THREE.AdditiveBlending, depthWrite: false }));
          halo.position.set(cx, cy, Z + 0.06); box.add(halo);
        }
      }

      // ---- the back edge: USB-C, two 3.5 mm jacks, microSD, power switch
      const portM = mat(0x050505, 0.8), metalM = mat(0xb8bcc2, 0.25, 0.95);
      const zMid = BD / 2 + 1;
      { const g = slab(9.6, 3.6, 2, 1.7, 0.3).rotateX(-Math.PI / 2).translate(30, BH + 0.6, zMid); add(box, g, metalM);
        add(box, slab(8.4, 2.5, 2, 1.2, 0.2).rotateX(-Math.PI / 2).translate(30, BH + 0.9, zMid), portM); }
      for (const jx of [50, 66]) {
        add(box, new THREE.TorusGeometry(3.6, 0.7, 12, 40).rotateX(Math.PI / 2).translate(jx, BH + 0.2, zMid), metalM);
        add(box, new THREE.CylinderGeometry(3.1, 3.1, 1.2, 32).translate(jx, BH - 0.1, zMid), portM);
      }
      add(box, slab(12.5, 1.8, 1.6, 0.6, 0.2).rotateX(-Math.PI / 2).translate(86, BH + 0.5, zMid), portM);
      add(box, slab(9, 3.4, 1.4, 1, 0.2).rotateX(-Math.PI / 2).translate(106, BH + 0.4, zMid), portM);
      add(box, new THREE.BoxGeometry(3, 2.4, 2.6).translate(108, BH + 1.1, zMid), mat(0xd8d4cb, 0.5));
      // rubber feet
      for (const [fx, fy] of [[12, 12], [112, 12], [12, 150], [112, 150]]) add(box, new THREE.CylinderGeometry(4, 4, 1.2, 24).rotateX(Math.PI / 2).translate(fx, fy, -0.6), mat(0x111111, 0.9));
      box.position.z = 1.2;

      // ---- a hand-sized reference: a phone-sized card beside it for the "hand" view
      if (view === 'hand') {
        const card = add(sc, slab(71.5, 147, 7.8, 9, 1.2).translate(-20 - 36, 62, 0), mat(0x2d3036, 0.35, 0.3));
        const scrM = new THREE.Mesh(new THREE.PlaneGeometry(66, 141), mat(0x0d0f14, 0.12, 0.2)); scrM.position.set(-56, 62, 7.85); sc.add(scrM);
        void card;
      }

      // ---- the camera
      const cam = new THREE.PerspectiveCamera(24, w / h, 5, 5000); cam.up.set(0, 0, 1);
      const T = new THREE.Vector3(62, 78, 10);
      if (view === 'hero') { cam.position.set(-120, -230, 260); T.set(64, 78, 6); }
      if (view === 'top') { cam.fov = 12; cam.position.set(62, 79, 980); T.set(62, 81, 0); cam.up.set(0, 1, 0); }
      if (view === 'back') { cam.fov = 22; cam.position.set(150, 380, 120); T.set(66, 120, 12); }
      if (view === 'hand') { cam.fov = 26; cam.position.set(20, -260, 330); T.set(20, 78, 0); }
      cam.updateProjectionMatrix(); cam.lookAt(T);
      r.render(sc, cam);
      return r.domElement.toDataURL('image/png');
    }, { screen, view, w: Math.round(W * SS), h: Math.round(H * SS) });
    fs.writeFileSync(OUT + '/raw_' + view + '.png', Buffer.from(png.split(',')[1], 'base64'));
    console.log('done', view);
  }
  await b.close();
})();
