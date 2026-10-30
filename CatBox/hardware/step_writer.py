# Cat Box 3: a small STEP (AP214) writer for parts made of extruded profiles ("slabs").
# Exact geometry: lines and circular arcs stay lines and circles (planes and cylinders in STEP),
# so in Onshape / Fusion / FreeCAD the holes are real holes you can select, measure and change.
# A part = several slabs (each a prism: an outer loop and holes, from z0 to z1); each slab becomes one
# solid body - in Onshape select them all and Boolean > Union to make one part.
import math

class Loop:
    """A closed profile: segments ('L', p, q) | ('A', p, q, centre, ccw) | ('C', centre, r)."""
    def __init__(self, segs): self.segs = segs
    def area(self):
        pts = self.sample()
        return 0.5 * sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1] for i in range(len(pts)))
    def sample(self):
        out = []
        for s in self.segs:
            if s[0] == 'L': out.append(s[1])
            elif s[0] == 'C':
                c, r, sg = s[1], s[2], (1 if s[3] else -1)
                out += [(c[0] + r * math.cos(sg * 2 * math.pi * k / 32), c[1] + r * math.sin(sg * 2 * math.pi * k / 32)) for k in range(32)]
            else:
                _, p, q, c, ccw = s
                a0, a1 = math.atan2(p[1] - c[1], p[0] - c[0]), math.atan2(q[1] - c[1], q[0] - c[0])
                if ccw and a1 <= a0: a1 += 2 * math.pi
                if not ccw and a1 >= a0: a1 -= 2 * math.pi
                r = math.hypot(p[0] - c[0], p[1] - c[1])
                out += [(c[0] + r * math.cos(a0 + (a1 - a0) * k / 8), c[1] + r * math.sin(a0 + (a1 - a0) * k / 8)) for k in range(8)]
        return out
    def reversed(self):
        segs = []
        for s in reversed(self.segs):
            if s[0] == 'L': segs.append(('L', s[2], s[1]))
            elif s[0] == 'A': segs.append(('A', s[2], s[1], s[3], not s[4]))
            else: segs.append(('C', s[1], s[2], not s[3]))
        return Loop(segs)
    def oriented(self, ccw):
        a = self.area()
        return self if (a > 0) == ccw else self.reversed()

def circle(cx, cy, r): return Loop([('C', (cx, cy), r, True)])
def rect(cx, cy, w, h):
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    return Loop([('L', (x0, y0), (x1, y0)), ('L', (x1, y0), (x1, y1)), ('L', (x1, y1), (x0, y1)), ('L', (x0, y1), (x0, y0))])
def rrect(cx, cy, w, h, r):
    if r <= 0: return rect(cx, cy, w, h)
    r = min(r, w / 2, h / 2)
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    segs = []
    def line(p, q):
        if math.hypot(q[0] - p[0], q[1] - p[1]) > 1e-6: segs.append(('L', p, q))
    line((x0 + r, y0), (x1 - r, y0)); segs.append(('A', (x1 - r, y0), (x1, y0 + r), (x1 - r, y0 + r), True))
    line((x1, y0 + r), (x1, y1 - r)); segs.append(('A', (x1, y1 - r), (x1 - r, y1), (x1 - r, y1 - r), True))
    line((x1 - r, y1), (x0 + r, y1)); segs.append(('A', (x0 + r, y1), (x0, y1 - r), (x0 + r, y1 - r), True))
    line((x0, y1 - r), (x0, y0 + r)); segs.append(('A', (x0, y0 + r), (x0 + r, y0), (x0 + r, y0 + r), True))
    return Loop(segs)
def dhole(cx, cy, d=6.15, flat=4.6):
    """A D-shaft hole: round, one flat side at x = cx + flat - d/2."""
    r = d / 2; xf = flat - r; yf = math.sqrt(r * r - xf * xf)
    p, q = (cx + xf, cy + yf), (cx + xf, cy - yf)
    return Loop([('A', p, q, (cx, cy), True), ('L', q, p)])
def poly(pts): return Loop([('L', tuple(pts[i]), tuple(pts[(i + 1) % len(pts)])) for i in range(len(pts))])

class Step:
    def __init__(self): self.lines = []; self.n = 0; self.solids = []
    def e(self, s):
        self.n += 1; self.lines.append('#%d=%s;' % (self.n, s)); return '#%d' % self.n
    def pt(self, p): return self.e('CARTESIAN_POINT(\'\',(%.6f,%.6f,%.6f))' % tuple(p))
    def dr(self, d): return self.e('DIRECTION(\'\',(%.9f,%.9f,%.9f))' % tuple(d))
    def ax(self, o, z, x): return self.e('AXIS2_PLACEMENT_3D(\'\',%s,%s,%s)' % (self.pt(o), self.dr(z), self.dr(x)))

    def prism(self, name, outer, holes, z0, z1, mapf=None, mirror=False):
        """One solid: outer loop and holes extruded from z0 to z1. mapf maps (u, v, w) to the world;
        mirror = mapf is a reflection (the faces then turn round)."""
        mapf = mapf or (lambda p: p)
        loops = [outer.oriented(not mirror)] + [h.oriented(mirror) for h in holes]
        M = lambda u, v, w: mapf((u, v, w))
        def vec(a, b): return (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        def norm(v):
            l = math.sqrt(sum(c * c for c in v)) or 1.0
            return tuple(c / l for c in v)
        up = norm(vec(M(0, 0, z0), M(0, 0, z1)))
        X = norm(vec(M(0, 0, 0), M(1, 0, 0)))
        faces = []
        top_bounds, bot_bounds = [], []
        for li, lp in enumerate(loops):
            segs = lp.segs
            # vertices at the start of each segment
            vb, vt, eb, et, ev = [], [], [], [], []
            for s in segs:
                p = s[1] if s[0] != 'C' else (s[1][0] + s[2], s[1][1])
                vb.append(self.e('VERTEX_POINT(\'\',%s)' % self.pt(M(p[0], p[1], z0))))
                vt.append(self.e('VERTEX_POINT(\'\',%s)' % self.pt(M(p[0], p[1], z1))))
                pb, pt_ = M(p[0], p[1], z0), M(p[0], p[1], z1)
                ln = self.e('LINE(\'\',%s,%s)' % (self.pt(pb), self.e('VECTOR(\'\',%s,%.6f)' % (self.dr(up), z1 - z0))))
                ev.append(None)  # filled below
                ev[-1] = (pb, pt_, ln)
            ev = [self.e('EDGE_CURVE(\'\',%s,%s,%s,.T.)' % (vb[i], vt[i], ev[i][2])) for i in range(len(segs))]
            nseg = len(segs)
            for i, s in enumerate(segs):
                j = (i + 1) % nseg
                for z, vv, store in ((z0, vb, eb), (z1, vt, et)):
                    if s[0] == 'L':
                        p, q = s[1], s[2]
                        P, Q = M(p[0], p[1], z), M(q[0], q[1], z)
                        d = vec(P, Q); L = math.sqrt(sum(c * c for c in d))
                        cv = self.e('LINE(\'\',%s,%s)' % (self.pt(P), self.e('VECTOR(\'\',%s,%.6f)' % (self.dr(norm(d)), L))))
                        store.append(self.e('EDGE_CURVE(\'\',%s,%s,%s,.T.)' % (vv[i], vv[j], cv)))
                    elif s[0] == 'A':
                        c, ccw = s[3], s[4]
                        r = math.hypot(s[1][0] - c[0], s[1][1] - c[1])
                        cv = self.e('CIRCLE(\'\',%s,%.6f)' % (self.ax(M(c[0], c[1], z), up, X), r))
                        same = ccw != mirror
                        store.append(self.e('EDGE_CURVE(\'\',%s,%s,%s,%s)' % (vv[i], vv[j], cv, '.T.' if same else '.F.')))
                    else:
                        c, r, ccw = s[1], s[2], s[3]
                        cv = self.e('CIRCLE(\'\',%s,%.6f)' % (self.ax(M(c[0], c[1], z), up, X), r))
                        same = ccw != mirror
                        store.append(self.e('EDGE_CURVE(\'\',%s,%s,%s,%s)' % (vv[i], vv[i], cv, '.T.' if same else '.F.')))
            oe = lambda e, f: self.e('ORIENTED_EDGE(\'\',*,*,%s,%s)' % (e, '.T.' if f else '.F.'))
            # side faces
            for i, s in enumerate(segs):
                j = (i + 1) % nseg if s[0] != 'C' else i
                loop = self.e('EDGE_LOOP(\'\',(%s))' % ','.join([oe(eb[i], True), oe(ev[j], True), oe(et[i], False), oe(ev[i], False)]))
                bound = self.e('FACE_OUTER_BOUND(\'\',%s,.T.)' % loop)
                if s[0] == 'L':
                    p, q = s[1], s[2]
                    P, Q = M(p[0], p[1], z0), M(q[0], q[1], z0)
                    d = norm(vec(P, Q))
                    n = norm((d[1] * up[2] - d[2] * up[1], d[2] * up[0] - d[0] * up[2], d[0] * up[1] - d[1] * up[0]))   # d x up = to the right
                    surf = self.e('PLANE(\'\',%s)' % self.ax(P, n, d))
                    faces.append(self.e('ADVANCED_FACE(\'\',(%s),%s,.T.)' % (bound, surf)))
                else:
                    c = s[3] if s[0] == 'A' else s[1]
                    r = math.hypot(s[1][0] - c[0], s[1][1] - c[1]) if s[0] == 'A' else s[2]
                    ccw = s[4] if s[0] == 'A' else s[3]
                    surf = self.e('CYLINDRICAL_SURFACE(\'\',%s,%.6f)' % (self.ax(M(c[0], c[1], z0), up, X), r))
                    faces.append(self.e('ADVANCED_FACE(\'\',(%s),%s,%s)' % (bound, surf, '.T.' if ccw != mirror else '.F.')))
            top_bounds.append((li, [oe(e, True) for e in et]))
            bot_bounds.append((li, [oe(e, False) for e in reversed(eb)]))
        for z, bounds, nrm in ((z1, top_bounds, up), (z0, bot_bounds, tuple(-c for c in up))):
            bs = []
            for li, oes in bounds:
                loop = self.e('EDGE_LOOP(\'\',(%s))' % ','.join(oes))
                bs.append(self.e('%s(\'\',%s,.T.)' % ('FACE_OUTER_BOUND' if li == 0 else 'FACE_BOUND', loop)))
            surf = self.e('PLANE(\'\',%s)' % self.ax(M(0, 0, z), nrm, X))
            faces.append(self.e('ADVANCED_FACE(\'\',(%s),%s,.T.)' % (','.join(bs), surf)))
        shell = self.e('CLOSED_SHELL(\'\',(%s))' % ','.join(faces))
        self.solids.append(self.e('MANIFOLD_SOLID_BREP(\'%s\',%s)' % (name, shell)))

    def write(self, path, product):
        ctx_len = self.e('(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.))')
        ctx_ang = self.e('(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.))')
        ctx_sol = self.e('(NAMED_UNIT(*)SI_UNIT($,.STERADIAN.)SOLID_ANGLE_UNIT())')
        unc = self.e('UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-04),%s,\'distance_accuracy_value\',\'\')' % ctx_len)
        ctx = self.e('(GEOMETRIC_REPRESENTATION_CONTEXT(3)GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((%s))GLOBAL_UNIT_ASSIGNED_CONTEXT((%s,%s,%s))REPRESENTATION_CONTEXT(\'\',\'3D\'))' % (unc, ctx_len, ctx_ang, ctx_sol))
        origin = self.ax((0, 0, 0), (0, 0, 1), (1, 0, 0))
        rep = self.e('ADVANCED_BREP_SHAPE_REPRESENTATION(\'%s\',(%s,%s),%s)' % (product, ','.join(self.solids), origin, ctx))
        appc = self.e('APPLICATION_CONTEXT(\'automotive design\')')
        self.e('APPLICATION_PROTOCOL_DEFINITION(\'international standard\',\'automotive_design\',2000,%s)' % appc)
        pctx = self.e('PRODUCT_CONTEXT(\'\',%s,\'mechanical\')' % appc)
        prod = self.e('PRODUCT(\'%s\',\'%s\',\'\',(%s))' % (product, product, pctx))
        self.e('PRODUCT_RELATED_PRODUCT_CATEGORY(\'part\',$,(%s))' % prod)
        pdf = self.e('PRODUCT_DEFINITION_FORMATION(\'\',\'\',%s)' % prod)
        pdc = self.e('PRODUCT_DEFINITION_CONTEXT(\'part definition\',%s,\'design\')' % appc)
        pd = self.e('PRODUCT_DEFINITION(\'design\',\'\',%s,%s)' % (pdf, pdc))
        pds = self.e('PRODUCT_DEFINITION_SHAPE(\'\',\'\',%s)' % pd)
        self.e('SHAPE_DEFINITION_REPRESENTATION(%s,%s)' % (pds, rep))
        head = ("ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('Cat Box 3'),'2;1');\n"
                "FILE_NAME('%s','2026-10-30T00:00:00',('Cat Box 3'),(''),'catbox step_writer','','');\n"
                "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\n" % product)
        open(path, 'w').write(head + '\n'.join(self.lines) + '\nENDSEC;\nEND-ISO-10303-21;\n')

def check(path):
    """Every edge used exactly twice, once each way (a closed, consistent shell)."""
    import re, collections
    txt = open(path).read()
    ent = dict(re.findall(r'^(#\d+)=(.*);$', txt, re.M))
    uses = collections.Counter()
    for k, v in ent.items():
        if v.startswith('ORIENTED_EDGE'):
            m = re.search(r",\*,\*,(#\d+),\.(T|F)\.\)", v)
            uses[(m.group(1), m.group(2))] += 1
    edges = {k for k, v in ent.items() if v.startswith('EDGE_CURVE')}
    bad = [e for e in edges if uses[(e, 'T')] != 1 or uses[(e, 'F')] != 1]
    return len(edges), len(bad)
