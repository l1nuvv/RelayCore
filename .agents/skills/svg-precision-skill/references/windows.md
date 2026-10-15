# SVG tools on Windows

Build and validation use standard-library Python. Choose an existing interpreter or an isolated environment under `build/skill-tools/`. Invoke it directly in PowerShell; activation is optional. PNG preview requires CairoSVG with native Cairo available, and `diff` also uses Pillow. The Python package alone may not make Cairo available on Windows: import CairoSVG with the chosen interpreter before promising a preview. Use an existing trusted Cairo installation if available; keep any DLL search configuration local to the task process.

If sandboxed Python cannot use the default temporary directory, point `TEMP` and `TMP` for the current process to an existing ignored directory under `build/`. The comparison helper creates temporary PNGs there and removes them after use.

From the repository root, after assigning `$svgPython` to the chosen interpreter and creating `build/svg-task/`:

```powershell
& $svgPython .agents/skills/svg-precision-skill/scripts/svg_cli.py build .agents/skills/svg-precision-skill/scripts/examples/icon_check.json build/svg-task/icon.svg
& $svgPython .agents/skills/svg-precision-skill/scripts/svg_cli.py validate build/svg-task/icon.svg
& $svgPython .agents/skills/svg-precision-skill/scripts/svg_cli.py render build/svg-task/icon.svg build/svg-task/icon.png --scale 4
& $svgPython .agents/skills/svg-precision-skill/scripts/svg_cli.py diff build/svg-task/icon.svg build/svg-task/revised.svg build/svg-task/diff.png
```

`diff` exits 0 for identical renders, 1 for changed renders and 2 for an execution failure. It is a pixel comparison helper, not a runtime graphics regression test. Compare rendered images directly when visible changes matter.

Run the rendered comparison regression cases with the same interpreter and Cairo setup:

```powershell
& $svgPython -m unittest discover -s .agents/skills/svg-precision-skill/scripts/tests -v
```

The upstream `self_test.py` writes to `scripts/_out/` and allows preview rendering to be skipped. For repository verification, exercise the example specs through the CLI into `build/` and check preview success separately. Validate an invalid SVG too when changing validation behavior. Do not report a skipped render as a passed preview.

Preserve source JSON/SVG separately from exported game PNGs. Review the PNG at its intended display size, especially Cyrillic text and alpha edges. Successful XML/reference validation does not check font availability, clipping or visual layout.
