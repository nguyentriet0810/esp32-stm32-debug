# CAM review — current AI_layout, 13 November 2026

Saved PCB SHA-256: `82da382a408244f0554dbf6b1e7cf92267938712c95e73f0253b3162a39e05fb`.

This is a fresh **verification export**, not a production release or an order submission. The PCB was read only; its content was unchanged.

## Fabrication geometry

- Nine Gerbers, separate PTH/NPTH Excellon files and a Gerber job file were exported from the same saved PCB using the same auxiliary origin.
- Native board: 70 × 60 mm, four R3 corners, two copper layers, 1.6 mm thick, 35 µm copper on each outer face.
- Auxiliary/grid origin: (100,110) mm on the lower-left of the KiCad board; CAM outline centreline spans X=0…70 mm and Y=0…60 mm.
- Four Ø3.2 mm mounting holes are at (5,5), (5,55), (65,5), (65,55) mm in CAM coordinates.
- All 160 PTH positions, including six plated slots, match both copper Gerber flash centres. Maximum discrepancy is 0.0005 mm from decimal drill rounding. All eight NPTH positions match the native PCB.
- No copper or solder-mask openings extend outside the physical board in a 0.025 mm/pixel raster check. No silkscreen intersects solder-mask openings on either face at that resolution.
- Minimum routed copper width in the export is 0.20 mm. The electrical/clearance DRC and netlist audit are separate checks and must be considered with this CAM review.
- B.Paste is empty, consistent with the current front-only SMD placement.

## Items to communicate at prototype ordering

- USB-C has four **plated slots 0.6 × 1.4/1.7 mm**. The encoder has two **plated slots 2.8 × 1.5 mm**. Confirm support for 0.6 mm plated routing; the supplied capability picture specifies round-hole limits, not slot limits.
- The job file's size 70.05 × 60.05 mm includes the 0.05 mm profile stroke; the actual centreline board dimensions are 70 × 60 mm.
- The job file records surface finish as `None`; choose the actual finish in the fabricator's order options.
- Three ESP32-S3 silkscreen outline segments are outside the board, following the antenna overhang up to 6.4 mm. They will be clipped at the board outline. No functional connector labels are outside the board.
- Existing footprint outline graphics use 0.12 mm strokes. The new lettering uses its own saved text stroke settings; factory capability for fine legend graphics should be checked if preserving those outlines matters.
- Physical connector compatibility still depends on buying the exact matching parts. Local models name GT-USB-7010ASV, JXTCONN TF-01A and BWSMA-KWE-Z001. The SW1 model is explicitly a generic EC11 visualization, not a certified mechanical model; check the actual encoder pin/slot/shaft dimensions.

`cam_audit.json`, `origin_check/origin_validation.json`, `port_geometry.json`, and the two CAM PNGs provide the supporting evidence. Passing these geometry checks supports a bare-board prototype order once plated-slot capability is confirmed. It does not establish USB impedance compliance or tested electrical behavior on assembled hardware.
