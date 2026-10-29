# Cat Box 3: printable knobs, encoder caps, MX keycaps and the slider cap (prism stacks for mesh.js)
# printing orientation: the TOP face lies on the bed (z = 0), so the top comes out smooth; no supports needed
import json, math, sys
O = sys.argv[1]
def circ(r, n=96, cx=0.0, cy=0.0, ccw=True):
    pts = [[cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)] for i in range(n)]
    return pts if ccw else pts[::-1]
def knurl(r, ridges=30, depth=0.35, n=240):   # a grippy edge
    return [[(r - depth * (0.5 - 0.5 * math.cos(ridges * 2 * math.pi * i / n))) * math.cos(2 * math.pi * i / n),
             (r - depth * (0.5 - 0.5 * math.cos(ridges * 2 * math.pi * i / n))) * math.sin(2 * math.pi * i / n)] for i in range(n)]
def dhole(d=6.1, flat=4.6, n=72):   # a D-shaft hole (round, one flat side), clockwise
    r = d / 2; xf = flat - r
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n
        x, y = r * math.cos(a), r * math.sin(a)
        pts.append([min(x, xf), y])
    return pts[::-1]
def rect(w, h, cx=0.0, cy=0.0, ccw=True):
    p = [[cx - w / 2, cy - h / 2], [cx + w / 2, cy - h / 2], [cx + w / 2, cy + h / 2], [cx - w / 2, cy + h / 2]]
    return p if ccw else p[::-1]
def cross(w=4.15, t=1.35):   # the MX stem's cross, as one clockwise polygon
    a, b = w / 2, t / 2
    p = [[-a, -b], [-b, -b], [-b, -a], [b, -a], [b, -b], [a, -b], [a, b], [b, b], [b, a], [-b, a], [-b, b], [-a, b]]
    return p[::-1]
def body(z0, z1, outer, holes=()): return {'z0': z0, 'z1': z1, 'polys': [{'outer': outer, 'holes': list(holes)}]}
files = {}

def knob(name, D, H, hole_depth, shaft_d, flat, skirt=True):
    R = D / 2
    # the pointer: a groove across the top from the middle to the edge (0.6 deep): two half discs leave a slot
    g = 0.5   # half the groove width
    half = lambda side: [[0, side * g]] + [[R * math.cos(a), R * math.sin(a)] for a in
            [ (math.asin(g / R) + i * (math.pi - 2 * math.asin(g / R)) / 60) * (1 if side > 0 else -1) for i in range(61)]] + [[-R, side * 0.0]]
    top = [p for p in circ(R, 120)]
    # simpler, robust: the top layer = the disc minus a slot from the centre to the edge
    slot = rect(R + 0.5, 2 * g, R / 2 + 0.25, 0.0, ccw=False)
    L = [body(0.0, 0.6, knurl(R), [])]
    # a slot as a hole needs to stay inside the outer outline: make it end 0.3 mm before the edge, and add a dot
    L[0]['polys'][0]['holes'] = [rect(R - 1.2, 2 * g, (R - 1.2) / 2 + 0.6, 0.0, ccw=False)]
    L.append(body(0.6, H - hole_depth, knurl(R)))
    L.append(body(H - hole_depth, H - (1.6 if skirt else 0.0), knurl(R), [dhole(shaft_d, flat)]))
    if skirt: L.append(body(H - 1.6, H, circ(R + 1.6, 120), [dhole(shaft_d, flat)]))
    files[name] = L

knob('CatBox3_knob_pot_11mm', 11.0, 14.0, 9.0, 6.15, 4.6)        # 27 x (Alpha 9 mm, 6 mm D shaft)
knob('CatBox3_knob_cutoff_15mm', 15.0, 15.0, 9.0, 6.15, 4.6)     # 1 x, the big one
knob('CatBox3_cap_encoder_14mm', 14.0, 16.0, 11.0, 6.15, 4.6, skirt=False)   # 12 x EC11 (6 mm D shaft, 20 mm)

# MX keycap, flat, straight walls (print in clear / natural PETG so the LED shows through)
S, WALL, TOP = 17.6, 1.2, 1.4
files['CatBox3_keycap_mx'] = [
    body(0.0, TOP, rect(S, S)),
    body(TOP, TOP + 4.0, rect(S, S), [rect(S - 2 * WALL, S - 2 * WALL, ccw=False)]),
    {'z0': TOP, 'z1': TOP + 4.0, 'polys': [{'outer': circ(2.75, 64), 'holes': [cross()]}]},   # the stem, 4 mm deep
    body(TOP + 4.0, 7.2, rect(S, S), [rect(S - 2 * WALL, S - 2 * WALL, ccw=False)]),
]
# the slider's cap: a 12 x 18 grip, slot for the lever (1.3 x 4.1 mm, 6 mm deep)
files['CatBox3_cap_slider'] = [
    body(0.0, 2.0, rect(12.0, 18.0)),
    body(2.0, 8.0, rect(12.0, 18.0), [rect(4.1, 1.3, ccw=False)]),
]
json.dump(files, open(O + '/caps_layers.json', 'w'))
print('\n'.join(files))
