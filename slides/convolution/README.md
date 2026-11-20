# Faltung_LTI – Foliensatz zur Faltung (Ergänzung zu Blatt 3)

| Datei | Inhalt |
|---|---|
| `Faltung_LTI.pptx` | 17 Folien; Folien 5, 6, 7, 8, 12, 15 sind interaktiv |
| `Faltung.bas` | VBA-Modul für die interaktiven Folien |
| `model.py` | alle Signale/Systeme (dieselben Formeln wie im VBA) |
| `figs.py` | erzeugt die Grafiken in `build/` |
| `build.py` | erzeugt die .pptx (python-pptx) |

Makros einrichten (einmalig): PowerPoint → Alt+F11 → Datei → Datei importieren →
`Faltung.bas`; Cursor in `Sub Setup` → F5; als `.pptm` speichern. Ohne Makros
zeigen die interaktiven Folien ein festes Standardbild.

Neu bauen: `python figs.py && python build.py` (numpy, matplotlib, python-pptx).
