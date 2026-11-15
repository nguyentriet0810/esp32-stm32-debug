AI_layout - REV 1.0 - Fabrication package - 2026-11-13

BOARD
  Size: 70 x 60 mm, four R3 mm corners (Edge.Cuts centreline).
  Material: FR-4. Thickness: 1.6 mm. Copper: 2 layers, 35 um / 1 oz per side.
  Surface finish and solder-mask colour: select/confirm with the customer.
  The Gerber job records finish as None; it does not select a finish for the order.

FILES
  AI_layout-F_Cu.gbr / AI_layout-B_Cu.gbr: top / bottom copper.
  AI_layout-F_Mask.gbr / AI_layout-B_Mask.gbr: top / bottom solder mask.
  AI_layout-F_Silkscreen.gbr / AI_layout-B_Silkscreen.gbr: top / bottom legend.
  AI_layout-Edge_Cuts.gbr: closed board outline.
  AI_layout-F_Paste.gbr / AI_layout-B_Paste.gbr: stencil layers.
  B.Paste is intentionally empty; current SMD placement is front only.
  AI_layout-job.gbrjob: layer/stack metadata.
  AI_layout-PTH.drl: plated Excellon drilling and plated slots.
  AI_layout-NPTH.drl: non-plated Excellon drilling.

DRILLING / ROUTING
  Units: mm. Absolute coordinates (G90). Decimal Excellon format.
  Gerbers and BOTH drill files share the same lower-left board origin.
  No mirroring, scaling or automatic origin realignment is required.
  PTH: 160 positions = 154 round holes + 6 plated slots.
  NPTH: 8 round holes = 2 x dia 0.65, 2 x dia 1.0, 4 x dia 3.2 mm.
  USB shell: 4 plated slots, width 0.60 mm, length 1.40 / 1.70 mm.
  Encoder: 2 plated slots, width 1.50 mm, length 2.80 mm.
  IMPORTANT: confirm capability for 0.60 mm plated slots. Do not convert these
  slots to NPTH or replace them with round holes.
  Mounting holes dia 3.2 at (5,5), (5,55), (65,5), (65,55) mm.

ARTWORK
  Routed track width minimum 0.20 mm; power tracks minimum 0.40 mm.
  Via drill minimum 0.40 mm; via annular ring minimum 0.20 mm.
  Text stroke minimum 0.15 mm. Preserved footprint-outline graphics include
  0.12 mm strokes; please confirm legend capability if those fine outlines
  must be preserved. Clip legend outside the board to the physical outline.
  Ground pours are already filled. Preserve all copper and net assignments.

ASSEMBLY / MECHANICAL NOTES
  This package is for bare PCB fabrication; it contains no assembly BOM.
  Confirm actual connector/encoder compatibility separately. The USB-C
  GT-USB-7010ASV drawing recommends 0.8 mm PCB; this design intentionally
  retains 1.6 mm, requiring customer/vendor confirmation of stake engagement.

SOURCE
  AI_layout.kicad_pcb
  SHA-256: 82da382a408244f0554dbf6b1e7cf92267938712c95e73f0253b3162a39e05fb
  Exported using KiCad 10.0.6. All fabrication geometries checked against
  the reviewed board; PTH/NPTH common-origin alignment verified.
