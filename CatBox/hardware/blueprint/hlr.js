// Hidden-line projection of STL meshes for technical drawings.
//   node hlr.js jobs.json out.json
// jobs: [{ name, angle (feature edge, deg), res (px/mm for the depth buffer), meshes: [{ file, m: [12] (3x4 row-major) }], views: ['front', ...] }]
// out:  { name: { view: { b: [umin, umax, vmin, vmax], vis: [[u, v, u, v, ...], ...], hid: [...] } } }
'use strict';
const fs = require('fs');
const [, , jobsFile, outFile] = process.argv;
const jobs = JSON.parse(fs.readFileSync(jobsFile, 'utf8'));

function readStl(file) {
  const b = fs.readFileSync(file);
  const n = b.readUInt32LE(80);
  const v = new Float32Array(n * 9);
  for (let i = 0; i < n; i++) for (let k = 0; k < 9; k++) v[i * 9 + k] = b.readFloatLE(84 + i * 50 + 12 + k * 4);
  return v;
}
const norm = (a) => { const l = Math.hypot(a[0], a[1], a[2]); return [a[0] / l, a[1] / l, a[2] / l]; };
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

function viewBasis(name) {
  const V = {
    front: [[0, 1, 0], [0, 0, 1]], back: [[0, -1, 0], [0, 0, 1]], top: [[0, 0, -1], [0, 1, 0]], bottom: [[0, 0, 1], [0, -1, 0]],
    left: [[1, 0, 0], [0, 0, 1]], right: [[-1, 0, 0], [0, 0, 1]], iso: [norm([1, 1.6, -1.1]), null], iso2: [norm([-1, 1.6, -1.1]), null],
  }[name];
  const d = V[0];
  let u = V[1];
  if (!u) { const z = [0, 0, 1]; const k = dot(z, d); u = norm([z[0] - k * d[0], z[1] - k * d[1], z[2] - k * d[2]]); }
  const r = cross(d, u);
  return { d, u, r };
}

const out = {};
for (const job of jobs) {
  // ---- gather triangles (world) and feature-edge candidates per mesh
  const tris = [];      // Float64Array chunks
  const edges = [];     // { a: [x,y,z], b: [x,y,z], n1, n2 (normals or null), feat }
  const cosA = Math.cos((job.angle || 25) * Math.PI / 180);
  for (const mesh of job.meshes) {
    const raw = readStl(mesh.file), m = mesh.m || [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0];
    const n = raw.length / 9, T = new Float64Array(n * 9);
    for (let i = 0; i < n * 3; i++) {
      const x = raw[i * 3], y = raw[i * 3 + 1], z = raw[i * 3 + 2];
      T[i * 3] = m[0] * x + m[1] * y + m[2] * z + m[3];
      T[i * 3 + 1] = m[4] * x + m[5] * y + m[6] * z + m[7];
      T[i * 3 + 2] = m[8] * x + m[9] * y + m[10] * z + m[11];
    }
    tris.push(T);
    const vid = new Map(), P = [];
    const id = (i) => { const k = Math.round(T[i * 3] * 1000) + ',' + Math.round(T[i * 3 + 1] * 1000) + ',' + Math.round(T[i * 3 + 2] * 1000); let q = vid.get(k); if (q === undefined) { q = P.length; vid.set(k, q); P.push([T[i * 3], T[i * 3 + 1], T[i * 3 + 2]]); } return q; };
    const em = new Map();
    for (let t = 0; t < n; t++) {
      const a = id(t * 3), b = id(t * 3 + 1), c = id(t * 3 + 2);
      if (a === b || b === c || a === c) continue;
      const nn = cross([P[b][0] - P[a][0], P[b][1] - P[a][1], P[b][2] - P[a][2]], [P[c][0] - P[a][0], P[c][1] - P[a][1], P[c][2] - P[a][2]]);
      const l = Math.hypot(nn[0], nn[1], nn[2]); if (l < 1e-9) continue;
      const nrm = [nn[0] / l, nn[1] / l, nn[2] / l];
      for (const [p, q] of [[a, b], [b, c], [c, a]]) {
        const k = p < q ? p * 4194304 + q : q * 4194304 + p;
        const el = Math.hypot(P[p][0] - P[q][0], P[p][1] - P[q][1], P[p][2] - P[q][2]);
        // a stair riser of the rasterised slope: a narrow strip, long along this edge (not an ear sliver of a big face)
        const sides = [[a, b], [b, c], [c, a]].map(([i, j]) => Math.hypot(P[i][0] - P[j][0], P[i][1] - P[j][1], P[i][2] - P[j][2]));
        const thin = job.sliver && el > 1.0 && l / el < job.sliver && Math.min(...sides) < 0.25 && Math.max(...sides) < 5.0;
        const e = em.get(k); if (e) { e.f.push(nrm); e.thin = e.thin || thin; } else em.set(k, { p, q, f: [nrm], thin });
      }
    }
    for (const e of em.values()) {
      const feat = e.f.length !== 2 || (dot(e.f[0], e.f[1]) < cosA && !e.thin);
      edges.push({ a: P[e.p], b: P[e.q], n1: e.f[0], n2: e.f[1] || null, feat });
    }
  }
  out[job.name] = {};
  for (const vname of job.views) {
    const { d, u, r } = viewBasis(vname);
    const hidden = job.hidden ? job.hidden.includes(vname) : !vname.startsWith('iso');
    // ---- depth buffer
    let u0 = Infinity, u1 = -Infinity, v0 = Infinity, v1 = -Infinity;
    for (const T of tris) for (let i = 0; i < T.length; i += 3) {
      const pu = T[i] * r[0] + T[i + 1] * r[1] + T[i + 2] * r[2], pv = T[i] * u[0] + T[i + 1] * u[1] + T[i + 2] * u[2];
      if (pu < u0) u0 = pu; if (pu > u1) u1 = pu; if (pv < v0) v0 = pv; if (pv > v1) v1 = pv;
    }
    const res = job.res || 6;
    const W = Math.ceil((u1 - u0) * res) + 4, H = Math.ceil((v1 - v0) * res) + 4;
    const Z = new Float32Array(W * H).fill(Infinity);
    const X = (pu) => (pu - u0) * res + 2, Y = (pv) => (pv - v0) * res + 2;
    for (const T of tris) for (let i = 0; i < T.length; i += 9) {
      const px = [], py = [], pz = [];
      for (let k = 0; k < 3; k++) {
        const x = T[i + k * 3], y = T[i + k * 3 + 1], z = T[i + k * 3 + 2];
        px.push(X(x * r[0] + y * r[1] + z * r[2])); py.push(Y(x * u[0] + y * u[1] + z * u[2])); pz.push(x * d[0] + y * d[1] + z * d[2]);
      }
      const area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0]);
      if (Math.abs(area) < 1e-9) continue;
      const xa = Math.max(0, Math.floor(Math.min(px[0], px[1], px[2]))), xb = Math.min(W - 1, Math.ceil(Math.max(px[0], px[1], px[2])));
      const ya = Math.max(0, Math.floor(Math.min(py[0], py[1], py[2]))), yb = Math.min(H - 1, Math.ceil(Math.max(py[0], py[1], py[2])));
      for (let yy = ya; yy <= yb; yy++) for (let xx = xa; xx <= xb; xx++) {
        const cx = xx + 0.5, cy = yy + 0.5;
        const w0 = ((px[1] - cx) * (py[2] - cy) - (px[2] - cx) * (py[1] - cy)) / area;
        const w1 = ((px[2] - cx) * (py[0] - cy) - (px[0] - cx) * (py[2] - cy)) / area;
        const w2 = 1 - w0 - w1;
        if (w0 < -1e-6 || w1 < -1e-6 || w2 < -1e-6) continue;
        const zz = w0 * pz[0] + w1 * pz[1] + w2 * pz[2];
        const o = yy * W + xx; if (zz < Z[o]) Z[o] = zz;
      }
    }
    const zmax = (x, y) => {
      let m = -Infinity; const ix = Math.floor(x), iy = Math.floor(y);
      for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
        const xx = ix + dx, yy = iy + dy; if (xx < 0 || yy < 0 || xx >= W || yy >= H) return Infinity;
        const z = Z[yy * W + xx]; if (z > m) m = z;
      }
      return m;
    };
    // ---- classify edges
    const eps = 0.06 + 1.5 / res;
    const segs = { vis: [], hid: [] };
    for (const e of edges) {
      let use = e.feat;
      if (!use && e.n2) { const s1 = dot(e.n1, d), s2 = dot(e.n2, d); use = s1 * s2 < 0; }
      if (!use) continue;
      const pa = [dot(e.a, r), dot(e.a, u), dot(e.a, d)], pb = [dot(e.b, r), dot(e.b, u), dot(e.b, d)];
      const len = Math.hypot(pa[0] - pb[0], pa[1] - pb[1]);
      if (len < 1e-4) continue;            // seen end-on
      const ns = Math.max(2, Math.ceil(len * res * 2));
      const flags = [];
      for (let s = 0; s <= ns; s++) {
        const t = s / ns, qu = pa[0] + (pb[0] - pa[0]) * t, qv = pa[1] + (pb[1] - pa[1]) * t, qd = pa[2] + (pb[2] - pa[2]) * t;
        flags.push(qd <= zmax(X(qu), Y(qv)) + eps);
      }
      for (let s = 1; s < ns; s++) if (flags[s - 1] === flags[s + 1] && flags[s] !== flags[s - 1]) flags[s] = flags[s - 1];
      let st = 0;
      for (let s = 1; s <= ns + 1; s++) {
        if (s === ns + 1 || flags[s] !== flags[st]) {
          const ta = st / ns, tb = (s - 1) / ns;
          if (s - 1 > st || ns <= 2) {
            const seg = [pa[0] + (pb[0] - pa[0]) * ta, pa[1] + (pb[1] - pa[1]) * ta, pa[0] + (pb[0] - pa[0]) * tb, pa[1] + (pb[1] - pa[1]) * tb];
            if (flags[st]) segs.vis.push(seg); else if (hidden) segs.hid.push(seg);
          }
          st = s;
        }
      }
    }
    // ---- dedupe (coincident projections) and chain into polylines
    const R = (x) => Math.round(x * 50);
    const keyOf = (s) => { const a = R(s[0]) + ',' + R(s[1]), b = R(s[2]) + ',' + R(s[3]); return a < b ? a + '|' + b : b + '|' + a; };
    const seen = new Set();
    const uniq = (list) => list.filter((s) => { const k = keyOf(s); if (seen.has(k) || (R(s[0]) === R(s[2]) && R(s[1]) === R(s[3]))) return false; seen.add(k); return true; });
    const vis = uniq(segs.vis), hid = uniq(segs.hid);
    const chain = (list) => {
      const ends = new Map(), used = new Uint8Array(list.length), pk = (x, y) => R(x) + ',' + R(y);
      list.forEach((s, i) => { for (const k of [pk(s[0], s[1]), pk(s[2], s[3])]) { if (!ends.has(k)) ends.set(k, []); ends.get(k).push(i); } });
      const lines = [];
      const grow = (pts, front) => {
        for (;;) {
          const x = front ? pts[0] : pts[pts.length - 2], y = front ? pts[1] : pts[pts.length - 1];
          const c = (ends.get(pk(x, y)) || []).find((j) => !used[j]); if (c === undefined) return;
          used[c] = 1; const s = list[c];
          const fw = pk(s[0], s[1]) === pk(x, y); const nx = fw ? s[2] : s[0], ny = fw ? s[3] : s[1];
          if (front) pts.unshift(nx, ny); else pts.push(nx, ny);
        }
      };
      list.forEach((s, i) => { if (used[i]) return; used[i] = 1; const pts = [s[0], s[1], s[2], s[3]]; grow(pts, false); grow(pts, true); lines.push(pts); });
      // drop collinear middle points
      return lines.map((p) => {
        const q = [p[0], p[1]];
        for (let i = 2; i < p.length - 2; i += 2) {
          const ax = q[q.length - 2], ay = q[q.length - 1], bx = p[i], by = p[i + 1], cx = p[i + 2], cy = p[i + 3];
          const cr = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax), l = Math.hypot(cx - ax, cy - ay);
          if (l > 0 && Math.abs(cr) / l < 0.004) continue;
          q.push(bx, by);
        }
        q.push(p[p.length - 2], p[p.length - 1]);
        return q.map((x) => Math.round(x * 1000) / 1000);
      });
    };
    out[job.name][vname] = { b: [u0, u1, v0, v1], vis: chain(vis), hid: chain(hid) };
    process.stderr.write(`${job.name} ${vname}: ${vis.length} visible, ${hid.length} hidden segments\n`);
  }
}
fs.writeFileSync(outFile, JSON.stringify(out));
