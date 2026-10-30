# Cat Box 3: STEP files of the printed parts, for Onshape / Fusion / FreeCAD (exact holes, editable).
#   python3 make_step.py <stl folder with parts.json, screws.json, case_layers.json> <out folder>
# Each part = a few solid bodies stacked (the slabs): in Onshape select all of them, Boolean > Union.
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from step_writer import Step, Loop, circle, rect, rrect, dhole, poly, check

S, O = sys.argv[1], sys.argv[2]
os.makedirs(O, exist_ok=True)
written = []

def save(st, name):
    path = os.path.join(O, name + '.step')
    st.write(path, name)
    edges, bad = check(path)
    written.append((name, edges, bad))

# ---------------------------------------------------------------- the panel (flat, as printed; face up)
d = json.load(open(S + '/parts.json'))
sc = json.load(open(S + '/screws.json'))
W, H = d['W'], d['H']
LEN = sc['len']; sy = LEN / H
T, POCKET = 3.0, 1.5
Y = lambda y: LEN - y          # top view: the back edge at the top
through, pockets = [], []
for p in d['parts']:
    x, y = p['pos']
    if p['kind'] == 'circle':
        cx, cy = x + p['d'] / 2, Y((y + p['d'] / 2) * sy)
        if p['id'] == 'speaker':
            for k in range(-12, 13, 6): through.append(rrect(cx, cy - k, 28, 2, 1.0))
            pockets.append(('c', cx, cy, circle(cx, cy, 40.6 / 2)))
        elif p['id'] == 'joy':
            through.append(circle(cx, cy, 15.0))
        else:
            through.append(circle(cx, cy, 3.7))
    elif p['kind'] == 'rect':
        cx, cy = x + p['w'] / 2, Y((y + p['h'] / 2) * sy)
        if p['id'] == 'display':
            through.append(rrect(cx, cy, 110.0, 66.0, 1.0)); pockets.append(('r', cx, cy, rect(cx, cy, 121.6, 76.6)))
        elif p['id'] == 'slider':
            through.append(rrect(cx, cy, 64.0, 3.0, 1.5)); pockets.append(('r', cx, cy, rrect(cx, cy, 76.0, 10.0, 1.0)))
        else:
            through.append(rect(cx, cy, 14.0, 14.0)); pockets.append(('r', cx, cy, rect(cx, cy, 15.4, 15.4)))
for (sx, sv) in sc['screws']:
    through.append(circle(sx, Y(sv), 1.7))

def inside(loop, pk):
    pts = loop.sample()
    xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
    kind, cx, cy, pl = pk
    if kind == 'c':
        r = pl.segs[0][2]
        return all(math.hypot(px - cx, py - cy) < r for px, py in pts)
    ps = pl.sample(); X0, X1 = min(q[0] for q in ps), max(q[0] for q in ps); Y0, Y1 = min(q[1] for q in ps), max(q[1] for q in ps)
    return min(xs) > X0 and max(xs) < X1 and min(ys) > Y0 and max(ys) < Y1
under = [pk[3] for pk in pockets] + [h for h in through if not any(inside(h, pk) for pk in pockets)]
st = Step()
outer = rect(W / 2, LEN / 2, W, LEN)
st.prism('panel underside 0-1.5 (pockets)', outer, under, 0.0, POCKET)
st.prism('panel top 1.5-3', outer, through, POCKET, T)
save(st, 'CatBox3_panel_full')

# ---------------------------------------------------------------- knobs and caps (as mounted: bottom at z = 0)
def knob(name, D, Hk, depth, skirt):
    R = D / 2; st = Step()
    z = 0.0
    if skirt:
        st.prism('skirt', circle(0, 0, R + 1.6), [dhole(0, 0)], 0.0, 1.6); z = 1.6
    st.prism('shaft part', circle(0, 0, R), [dhole(0, 0)], z, depth)
    st.prism('body', circle(0, 0, R), [], depth, Hk - 0.6)
    st.prism('top with the pointer', circle(0, 0, R), [rect((R - 1.2) / 2 + 0.6, 0, R - 1.2, 1.0)], Hk - 0.6, Hk)
    save(st, name)
knob('CatBox3_knob_pot_11mm', 11.0, 14.0, 9.0, True)
knob('CatBox3_knob_cutoff_15mm', 15.0, 15.0, 9.0, True)
knob('CatBox3_cap_encoder_14mm', 14.0, 16.0, 11.0, False)

def cross(w=4.15, t=1.35):
    a, b = w / 2, t / 2
    return poly([(-a, -b), (-b, -b), (-b, -a), (b, -a), (b, -b), (a, -b), (a, b), (b, b), (b, a), (-b, a), (-b, b), (-a, b)])
st = Step(); Sq, WALL, TOP = 17.6, 1.2, 1.4
st.prism('walls', rect(0, 0, Sq, Sq), [rect(0, 0, Sq - 2 * WALL, Sq - 2 * WALL)], 0.0, 7.2 - TOP)
st.prism('stem (MX cross)', circle(0, 0, 2.75), [cross()], 7.2 - TOP - 4.0, 7.2 - TOP)
st.prism('top', rect(0, 0, Sq, Sq), [], 7.2 - TOP, 7.2)
save(st, 'CatBox3_keycap_mx')
st = Step()
st.prism('grip with the lever slot', rect(0, 0, 12.0, 18.0), [rect(0, 0, 4.1, 1.3)], 0.0, 6.0)
st.prism('top', rect(0, 0, 12.0, 18.0), [], 6.0, 8.0)
save(st, 'CatBox3_cap_slider')

# ---------------------------------------------------------------- the case (its print layers: polygons)
cl = json.load(open(S + '/case_layers.json'))
maps = {'xy': (lambda p: p, False), 'xz': (lambda p: (p[0], p[2], p[1]), True), 'yz': (lambda p: (p[2], p[0], p[1]), False)}
for name, bodies in cl.items():
    st = Step()
    for bi, b in enumerate(bodies):
        mf, mir = maps[b.get('plane', 'xy')]
        for pi, pg in enumerate(b['polys']):
            st.prism('%s body %d' % (name, bi + 1), poly(pg['outer']), [poly(h) for h in pg['holes']], b['z0'], b['z1'], mf, mir)
    save(st, name)

for n, e, bad in written: print('%-28s %6d edges  %s' % (n, e, 'OK' if bad == 0 else '%d BAD' % bad))
