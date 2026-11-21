# Project 3D models

These four STEP files are used only to complete the PCB's 3D appearance. Pad geometry, nets, tracks, holes, placement, silkscreen, board outline and DNP flags are unchanged. The existing ESP32 model is untouched.

Source: **JLCEDA/EasyEDA Official Library**, [JLCEDA](https://lceda.cn/) and [EasyEDA](https://easyeda.com/). Raw downloaded STEP files and source metadata are preserved; they remain subject to the source's terms. Original metadata and copyright notices are included in `sources/`.

| Reference | Model | Source part |
|---|---|---|
| J1 | GT-USB-7010ASV.step | G-Switch GT-USB-7010ASV, LCSC C2988369 |
| J5 | JXTCONN_TF-01A.step | JXTCONN TF-01A, LCSC C41426929 |
| J6 | BWSMA-KWE-Z001.step | BAT Wireless BWSMA-KWE-Z001, LCSC C496551 |
| SW1 | EC11E15204A3.step | Compatible switched EC11 visual model supplied with LCSC C470710; a generic EC11 model, not a certified mechanical model of this MPN |

SW1 is suitable for visualization of the existing switched EC11 footprint. Confirm the actual purchased encoder's shaft height and other dimensions before using this assembly for enclosure design.

Applied model transforms (mm; KiCad model rotation convention): J1 offset(0,0.275,0), rotation(0,0,0); J5 offset(0.002223,0.29,0.127337), rotation(0,0,0); J6 offset(0,0,0), rotation(0,0,-90); SW1 offset(7.5,-2.5,0), rotation(0,0,90). All scales are 1:1.
