"""
Beetleboy — Blender starter scene (works in Blender 4.2+ and 5.x)

Builds a fully editable project at real scale (1 Blender unit = 1 mm):
  01 Reference parts  board, screen, USB-C, battery, antenna zone (don't print)
  02 Case             Shell, Cover, Divider  (live modifiers: bevel + booleans, nothing applied)
  03 Cutters          hidden helper shapes the booleans subtract; move/resize them to move holes
A text block "MEASURES" (Scripting / Text Editor) lists every number and what to verify.

Use: Scripting workspace > Open this file > Run Script.  Or just open beetleboy.blend.
Orientation: lying flat, screen facing +Z (up), top edge = +Y, USB-C edge = -Y.
To export for printing: select Shell / Cover / Divider > File > Export > STL, "Selection Only",
Scale 1.0 (modifiers are applied on export automatically).
"""
import math
import bpy
import bmesh

# ----------------------------------------------------------------- measures (mm)
M = dict(
    # board (Waveshare ESP32-S3-Touch-LCD-1.69, landscape) — datasheet / photo estimates
    BOARD_W=41.13, BOARD_H=33.13, BOARD_R=4.0,
    BOARD_T=15.0,           # measured: glass front -> back-most part (connectors)
    ACTIVE_W=32.63, ACTIVE_H=27.97, ACTIVE_R=4.0,   # Waveshare outline drawing
    ACTIVE_DX=-0.85, ACTIVE_DY=0.4,  # image centre vs glass centre, read off the drawing (3.3 left / 5.0 right, 2.0 top / 2.8 bottom)
    MODULE_T=3.82, GLASS_TO_PCB_BACK=6.60, PCB_W=38.48, PCB_H=31.07,   # Waveshare outline drawing
    USB_W=8.94, USB_T=3.26, USB_L=7.35,
    USB_Z_FROM_GLASS=8.2,   # PCB back at 6.60 (drawing) + half of the 3.2 mm USB-C receptacle sitting on it
    # case
    FIT=0.25, WALL=3.2, FRONT=1.0, BACK=1.8, CORNER_R=11.0, EDGE_R=4.0,
    WIN_MARGIN=0.7, WIN_R=5.0,
    GAP=0.3,                # board's back connectors almost touch the divider -> board can't rattle
    DIVIDER_T=1.2,
    POCKET_WALL=1.0,        # battery pocket walls (part of the cover)
    BATT_CLEAR=0.5,         # around the battery, per side (length gets +1 mm extra, see POCK_W)
    SWELL=0.8,              # above and below the battery (swelling room)
    # parts
    BATT_W=40.0, BATT_H=30.0, BATT_T=6.0,   # 603040 datasheet
    WIRE_ROOM=3.0,          # extra length at the end where the wires leave (left side)
)

# board pocket (front zone)
IN_W, IN_H = M["BOARD_W"] + 2 * M["FIT"], M["BOARD_H"] + 2 * M["FIT"]
IN_R = M["BOARD_R"] + M["FIT"]
# battery zone (back): battery + clearance + the cover's pocket walls. The battery is wider
# than the board, so the inside steps out behind the board; the divider sits on that step.
POCK_W = M["BATT_W"] + M["WIRE_ROOM"] + 2 * M["BATT_CLEAR"] + 1.0   # +1 mm: battery was tight on the last print -> 45 mm
POCK_H = M["BATT_H"] + 2 * M["BATT_CLEAR"] + 1.0                       # 32 mm, same as the last print
BZ_W = max(IN_W, POCK_W + 2 * M["POCKET_WALL"]) + 0.1
BZ_H = max(IN_H, POCK_H + 2 * M["POCKET_WALL"]) + 0.1
BZ_R = 3.0
OUT_W, OUT_H = BZ_W + 2 * M["WALL"], BZ_H + 2 * M["WALL"]
# stack, back (z=0) to front
Z_COVER = M["BACK"]
Z_DIV = Z_COVER + M["BATT_T"] + 2 * M["SWELL"]
Z_ZONE = Z_DIV + M["DIVIDER_T"]
Z_BOARD_BACK = Z_ZONE + M["GAP"]
Z_GLASS = Z_BOARD_BACK + M["BOARD_T"]
DEPTH = Z_GLASS + M["FRONT"]
USB_Z = Z_GLASS - M["USB_Z_FROM_GLASS"]


# ----------------------------------------------------------------- helpers
def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    s = bpy.context.scene
    s.unit_settings.system = "METRIC"
    s.unit_settings.scale_length = 0.001
    s.unit_settings.length_unit = "MILLIMETERS"


def coll(name, parent=None, hide=False):
    c = bpy.data.collections.new(name)
    (parent or bpy.context.scene.collection).children.link(c)
    c.hide_render = hide
    return c


def mat(name, rgba):
    m = bpy.data.materials.new(name)
    m.diffuse_color = rgba
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Roughness"].default_value = 0.45
    return m


def rrect_pts(w, h, r, seg=16):
    r = min(r, w / 2 - 1e-3, h / 2 - 1e-3)
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90),
                       (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(seg + 1):
            a = math.radians(a0 + 90 * i / seg)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def prism(name, w, h, r, z0, z1, collection, material=None, x=0.0, y=0.0, seg=16):
    """Rounded-rectangle prism (r=0 -> plain box)."""
    bm = bmesh.new()
    pts = rrect_pts(w, h, r, seg) if r > 0 else [(w / 2, h / 2), (-w / 2, h / 2), (-w / 2, -h / 2), (w / 2, -h / 2)]
    bot = [bm.verts.new((x + px, y + py, z0)) for px, py in pts]
    face = bm.faces.new(bot)
    ext = bmesh.ops.extrude_face_region(bm, geom=[face])
    for v in [g for g in ext["geom"] if isinstance(g, bmesh.types.BMVert)]:
        v.co.z = z1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    collection.objects.link(ob)
    if material:
        ob.data.materials.append(material)
    return ob


def ring(name, w, h, r, t, z0, z1, collection, material=None):
    outer = prism(name, w, h, r, z0, z1, collection, material)
    inner = prism(name + "_hole", w - 2 * t, h - 2 * t, max(r - t, 0.5), z0 - 1, z1 + 1, collection)
    boolean(outer, inner, "DIFFERENCE", apply=True)
    bpy.data.objects.remove(inner)
    return outer


def boolean(ob, operand, op, apply=False, name="Boolean"):
    md = ob.modifiers.new(name, "BOOLEAN")
    md.operation = op
    md.solver = "EXACT"
    if isinstance(operand, bpy.types.Collection):
        md.operand_type = "COLLECTION"
        md.collection = operand
    else:
        md.object = operand
    if apply:
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.modifier_apply(modifier=md.name)
    return md


def edge_bevel(ob, width, segments=10, angle=40):
    md = ob.modifiers.new("Soft edges", "BEVEL")
    md.width = width
    md.segments = segments
    md.limit_method = "ANGLE"
    md.angle_limit = math.radians(angle)
    md.harden_normals = False
    return md


def as_cutter(ob):
    ob.display_type = "WIRE"
    ob.hide_render = True


# ----------------------------------------------------------------- build
def build():
    reset()
    c_ref, c_case = coll("01 Reference parts (don't print)"), coll("02 Case")
    c_cut = coll("03 Cutters", hide=True)
    c_shell_cut = coll("Shell cutters", c_cut, hide=True)
    c_cover_keep = coll("Cover zone", c_cut, hide=True)
    c_div_cut = coll("Divider cutters", c_cut, hide=True)

    m_case = mat("Case white", (0.92, 0.91, 0.88, 1))
    m_cover = mat("Cover", (0.85, 0.45, 0.30, 1))
    m_div = mat("Divider", (0.55, 0.42, 0.84, 1))
    m_glass = mat("Glass", (0.03, 0.03, 0.04, 1))
    m_screen = mat("Screen", (0.25, 0.20, 0.75, 1))
    m_pcb = mat("PCB", (0.08, 0.20, 0.45, 1))
    m_metal = mat("Metal", (0.7, 0.7, 0.72, 1))
    m_batt = mat("Battery", (0.75, 0.75, 0.70, 1))
    m_keep = mat("Keep-out", (1.0, 0.2, 0.2, 1))

    # --- reference parts
    prism("Board glass+LCD module", M["BOARD_W"], M["BOARD_H"], M["BOARD_R"], Z_GLASS - M["MODULE_T"], Z_GLASS, c_ref, m_glass)
    prism("Board PCB", M["PCB_W"], M["PCB_H"], 3.0, Z_GLASS - M["GLASS_TO_PCB_BACK"], Z_GLASS - M["MODULE_T"], c_ref, m_pcb)
    prism("Board back parts (envelope)", M["PCB_W"] - 2, M["PCB_H"] - 2, 3.0, Z_BOARD_BACK, Z_GLASS - M["GLASS_TO_PCB_BACK"], c_ref, m_pcb)
    prism("Screen active area", M["ACTIVE_W"], M["ACTIVE_H"], M["ACTIVE_R"], Z_GLASS - 0.05, Z_GLASS + 0.02, c_ref, m_screen, x=M["ACTIVE_DX"], y=M["ACTIVE_DY"])
    usb = prism("USB-C receptacle", M["USB_W"], M["USB_L"], 1.2, USB_Z - M["USB_T"] / 2, USB_Z + M["USB_T"] / 2, c_ref, m_metal,
                y=-M["BOARD_H"] / 2 + M["USB_L"] / 2)
    prism("Battery", M["BATT_W"], M["BATT_H"], 1.0, Z_COVER + M["SWELL"], Z_COVER + M["SWELL"] + M["BATT_T"], c_ref, m_batt,
          x=(M["WIRE_ROOM"] + 1.0) / 2)
    keep = prism("Antenna keep-out (no metal, no battery)", 8.0, 9.0, 0, Z_DIV, Z_GLASS, c_ref, m_keep,
                 x=-M["BOARD_W"] / 2 + 4.0, y=M["BOARD_H"] / 2 - 4.5)
    keep.display_type = "WIRE"

    # --- cutters (move these to move holes)
    a = prism("Board pocket", IN_W, IN_H, IN_R, Z_ZONE - 0.01, Z_GLASS, c_shell_cut)
    bz = prism("Battery zone (wider)", BZ_W, BZ_H, BZ_R, -1, Z_ZONE, c_shell_cut)
    w = prism("Window", M["ACTIVE_W"] + 2 * M["WIN_MARGIN"], M["ACTIVE_H"] + 2 * M["WIN_MARGIN"], M["WIN_R"], Z_GLASS - 1, DEPTH + 1, c_shell_cut, x=M["ACTIVE_DX"], y=M["ACTIVE_DY"])
    s1 = prism("USB-C slot", 9.6, 10, 2.2, USB_Z - 2.3, USB_Z + 2.3, c_shell_cut, y=-OUT_H / 2)
    s2 = prism("USB-C plug recess", 12.6, 2 * (M["WALL"] - 0.9), 3.8, USB_Z - 3.8, USB_Z + 3.8, c_shell_cut, y=-OUT_H / 2)
    cz = prism("Below cover line", OUT_W + 10, OUT_H + 10, 0, -5, Z_COVER, c_shell_cut)
    for o in (a, bz, w, s1, s2, cz):
        as_cutter(o)
    keepz = prism("Cover zone box", OUT_W + 10, OUT_H + 10, 0, -5, Z_COVER, c_cover_keep)
    as_cutter(keepz)
    ws = prism("Divider wire slot", 9.0, 5.0, 1.5, Z_DIV - 1, Z_ZONE + 1, c_div_cut, x=-M["BOARD_W"] / 2 + 10.5, y=-M["BOARD_H"] / 2 + 3.2)
    as_cutter(ws)

    # --- shell: body -> soft edges -> subtract cutters
    shell = prism("Shell", OUT_W, OUT_H, M["CORNER_R"], 0, DEPTH, c_case, m_case, seg=24)
    edge_bevel(shell, M["EDGE_R"])
    boolean(shell, c_shell_cut, "DIFFERENCE", name="Cutouts")

    # --- cover: same body, keep only the back slice, add battery pocket walls
    cover = prism("Cover", OUT_W, OUT_H, M["CORNER_R"], 0, DEPTH, c_case, m_cover, seg=24)
    edge_bevel(cover, M["EDGE_R"])
    boolean(cover, c_cover_keep, "INTERSECT", name="Keep back slice")
    walls = ring("Cover pocket walls", POCK_W + 2 * M["POCKET_WALL"], POCK_H + 2 * M["POCKET_WALL"], BZ_R - 0.05, M["POCKET_WALL"],
                 Z_COVER - 0.01, Z_DIV, c_case, m_cover)
    walls.parent = cover

    # --- divider plate (flat; the board's back connectors rest on it)
    div = prism("Divider", BZ_W - 0.3, BZ_H - 0.3, BZ_R - 0.15, Z_DIV, Z_ZONE, c_case, m_div)
    boolean(div, c_div_cut, "DIFFERENCE", name="Wire slot")

    # --- notes inside the file
    t = bpy.data.texts.new("MEASURES")
    t.write(notes())

    # viewport niceties
    for area in getattr(bpy.context, "screen", None) and bpy.context.screen.areas or []:
        if area.type == "VIEW_3D":
            area.spaces[0].clip_start, area.spaces[0].clip_end = 0.1, 10000
    return shell, cover, div


def notes():
    L = [
        "BEETLEBOY - MEASURES (mm). 1 Blender unit = 1 mm. Screen faces +Z, top edge +Y, USB-C -Y.",
        "",
        f"Outer case: {OUT_W:.1f} W x {OUT_H:.1f} H x {DEPTH:.1f} D, corner radius {M['CORNER_R']}, soft edge radius {M['EDGE_R']}",
        f"Board pocket: {IN_W:.2f} x {IN_H:.2f} (board + {M['FIT']} per side). Battery zone behind it: {BZ_W:.2f} x {BZ_H:.2f}",
        f"Battery pocket (inside the cover walls): {POCK_W:.1f} x {POCK_H:.1f} x {Z_DIV - Z_COVER:.1f} deep",
        f"Window: {M['ACTIVE_W'] + 2 * M['WIN_MARGIN']:.2f} x {M['ACTIVE_H'] + 2 * M['WIN_MARGIN']:.2f}, radius {M['WIN_R']}",
        "",
        "Stack, back (z=0) to front:",
        f"  0.0 - {Z_COVER:.1f}   back cover",
        f"  {Z_COVER:.1f} - {Z_DIV:.1f}   battery pocket ({M['BATT_T']} mm cell + {M['SWELL']} above and below)",
        f"  {Z_DIV:.1f} - {Z_ZONE:.1f}   divider plate (rests on the cover walls, held by the step in the shell; your cut plastic sheet works too)",
        f"  {Z_ZONE:.1f} - {Z_BOARD_BACK:.1f}   air gap",
        f"  {Z_BOARD_BACK:.1f} - {Z_GLASS:.1f}   board, {M['BOARD_T']} mm (its back connectors rest on the divider)",
        f"  {Z_GLASS:.1f} - {DEPTH:.1f}   front bezel",
        "",
        f"USB-C: centred on the bottom edge, centre height z={USB_Z:.1f} (= {M['USB_Z_FROM_GLASS']} below the glass). Slot 9.6 x 4.6, recess 12.6 x 7.6",
        "",
        "Still open (send these and the model gets updated):",
        "  - glass front -> back-most part of the board (15 used, from your estimate)",
        "",
        "Printing: Shell front-down, Cover outside-down, Divider flat. PLA/PETG, 0.2 mm layers, 3 walls.",
    ]
    return "\n".join(L)


if __name__ == "__main__":
    build()
