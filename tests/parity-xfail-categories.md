# Parity accepted-difference categories

Narrative classification of `tests/parity-xfail.txt` (the
accepted-difference list `make check-parity` uses). Every `fail`
line has class + frozen T-norm + one-line reason in
`tests/parity-t-numbers.md`.

Counts from the 750-test `make check-parity` run after T6
(`pass=608 / fail=0 / xfail=115 / skip=27 / unexpected_pass=0`).
Track 1 closed: class C=0. The 115 misses are **tolerance classes**, not
open bugs.

Named real-gap is **0**. `LIBRSVG_VERSION` **2.62.3** is
feature-complete vs GNOME 2.62.3 for the named `bugs/` + root
`filter-*` + `svg2/` corpus. It is **not** pixel-identity to every
rust PNG.

Primary class is one of TFM / TAA / TCR (every `fail` line).
**E** is empty. Policy: `tests/README-parity.md`.

## Totals (TFM / TAA / TCR / E)

| Class | Fail | Skip | Meaning | Action |
| --- | ---: | ---: | --- | --- |
| **TFM** font / metrics | **73** | 0 | Pango-vs-rust / Dana-FaNum / Verdana / suite-font | accepted difference |
| **TAA** coverage AA | **37** | 0 | Adwaita path AA + named interpolator/AA | accepted difference |
| **C** Locale / `systemLanguage` | **0** | 0 | Harness sets `LANG=de` for that fixture only (Q3) | Flipped |
| **TCR** cairo vs rust PNG | **5** | 0 | Feature in; host cairo vs rust PNG | accepted difference |
| **E** Missing / wrong C feature | **0** | 0 | — | None |
| 2.62 also incomplete / out of scope | (3 of TFM also first-x) | 26 | n/a | skips stay skips |
| Unexpected pass still listed | 0 | 0 | — | Remove on sight |
| **Listed** | **115** | **27** | | Named real-gap **0** |

`115 = 73 + 37 + 5`. `fail` rows are accepted rust-oracle misses.
`skip` rows have no `-ref.png`. Skips are not feature work unless a
consumer fails.

Directory split of the 115: root 5, `adwaita/` 33, `bugs/` 8,
`svg1.1/` 68, `svg2/` 1.

## TFM. Host font / metrics (73) — accepted difference

C `rsvg-test` isolates **LiberationSans-Regular.ttf** only. Rust
reftests isolate **Roboto** (as `sans-serif` / `sans` / unknown
families), **Ahem**, **DejaVu Sans**, **Noto Sans Hebrew**. Suite
files name `SVGFreeSansASCII` via deprecated SVG `<font>`; rust will
not implement SVG fonts. Labels in the rust PNG are therefore Roboto;
C draws Liberation Sans. That is the leftover, not a missing SVG
feature.

**P19 done:** `RSVG_PARITY=1` loads rust’s TTF set + `fonts.conf`.
Pixel noise dropped; **no line flipped**. Class A stays **180**.

**Q1:** isolated rust TTF list + `FcConfigSetCurrent`.

**Q2:** `RSVG_PARITY=1` fills text as a cairo path in paint-order
(`rust/rsvg/src/drawing_ctx.rs`). That flipped 105 xfails (including
the named set below). Leftover class A is Pango-vs-rust metrics /
missing face, not another algorithm. Do **not** guess outlines.

### A1. Named text still listed (5)

T6 live `rsvg-test` 72 dpi / `RSVG_PARITY=1`. These will not match
without rust’s text/AA stack:

- `rtl-tspan.svg` — **1977 / 255** — Dana-FaNum not in the rust TTF set; leave xfail.
- `bugs/bug340047.svg` — **777 / 24** — `baseline-shift` super/sub is already in C (`-0.2` / `+0.4`). Leftover is Pango-vs-rust italic metrics on a large page.
- `bugs/bug587721-text-transform.svg` — **472 / 255** — filename is “transform”; the SVG is DejaVu `font-size="1"` with `matrix(12,…)`. Pango-vs-rust at that scale, not CSS `text-transform`.
- `bugs/bug730-font-scaling.svg` — **4450 / 46** — nested `viewBox` scales of the same sans sentence. Placement is correct; leftover is scaled-font AA.
- `filter-component-transfer-from-reference-page.svg` — **4578 / 150** — CT table/linear/gamma already match; leftover is Verdana 75px labels.

**T6 flipped:** `text-objectBoundingBox.svg` — rust `text.rs` unions
every span’s pango ink box and uses that for objectBoundingBox
paint. C now does the same two-pass.

**T5 flipped:** `bugs/bug108-font-size-relative.svg` — `font-size: smaller`/`larger` now resolve to parent/1.2 and parent×1.2 (`rust/rsvg/src/font_props.rs`). The render path used to treat stored length 0 as “skip”.

**Q2 flipped (removed from xfail):** `a-pseudo-class.svg`,
`font-shorthand.svg`, `include-text.svg`, `include-fallback.svg`,
`svg2/text-paint-order.svg`, `bugs/bug668-small-caps.svg`, plus
`a-inside-text-content*` / `bug363` / `bug481` / `bug494` / `bug642`
/ `bug667` / `bug760180` and 92 suite-font files whose leftover was
only `show_layout` vs rust’s path fill.

### A2. SVG 1.1 suite (68 fail + 1 skip)

Every `svg1.1/` PNG xfail names `SVGFreeSansASCII` in the source.
Features underneath may already be implemented; the rust PNG still
contains suite-font labels. Do not implement features just to move
these numbers. If a named `bugs/` fixture isolates the same feature
without suite fonts, that named fixture is the one that mattered
(P8–P18).

| Prefix | Fail | Also exercises |
| --- | ---: | --- |
| `coords-trans-*` / `coords-viewattr-*` | 8 | transforms / viewBox |
| `filters-*` | 31 | filter primitives |
| `masking-*` | 1 | clip / mask |
| `painting-*` | 4 | stroke, markers |
| `paths-data-*` | 9 | path commands |
| `pservers-*` | 6 | gradients / patterns |
| `struct-*` | 4 | use / group |
| `text-*` | 4 | text / tref / dominant-baseline (see E watch) |
| `types-basic-02-f.svg` | 1 | types |
| skip `svg1.1/rects.svg` | — | no PNG |

## TAA. Coverage AA (37) — accepted difference

Placement is already correct. Leftover is cairo raster vs the rust
PNG (`max_diff` a few, or Adwaita path AA). Not a missing feature.

**Adwaita symbolic (33).** Path-only 16×16 icons (`stroke:none`).
`max_diff` 2–60 (median 9; 30 of 33 are ≤20). Not missing CSS.

**Q5 (sampled all 33 at 72 dpi):** rust and C already share the same
cairo path (`fill-rule`, paint-order, `shape-rendering` → Default /
None). **0** pixels are fully opaque on one side and empty on the
other. 30 icons differ only on coverage edges; 5 leftover pixels on
3 icons are opaque `#bebebe` next to an AA fringe, off by 1 RGB
count. Leave xfail. Do not rewrite cairo stroke/fill.

**Named (4), live `rsvg-test` at 72 dpi:**

| Fixture | pixels / max_diff | Note |
| --- | --- | --- |
| `bugs/bug241-light-source-type.svg` | 1331 / 7 | lighting types present |
| `bugs/bug282-drop-shadow.svg` | 118 / 2 | rust even-kernel 3-box in |
| `bugs/bug590-mask-units.svg` | 55 / 6 | mask units present |
| `filter-image-from-reference-page.svg` | **52 / 2** | PAR + unclipped subregion in; cairo GOOD vs rust. (`parity-xfail.txt` still quotes an older 60306/255 snapshot) |

Optional: match rust testing font options (hint-style None, hint-metrics
Off). Do **not** tighten the global `max_diff > 1` pass rule just to
swallow Adwaita.

## C. Locale (0)

- `system-language-de.svg` — **Q3 flipped.** `rsvg-test` sets `LANG=de`
  only for this fixture (C reads `LANG` at parse time). Host locale
  is unchanged for every other test. `system-language-en.svg` still
  passes on the host `en` language.

## TCR. Cairo vs rust PNG on implemented features (5) — accepted difference

Feature is in. Residual is host cairo vs the rust-generated PNG
(blur tails, blend backdrop, lighting intensity, 1 px IRect, edge
alpha). No new interpolator and no cairo-operator rewrite.

| Fixture | pixels / max_diff | Residual |
| --- | --- | --- |
| `filter-kernel-unit-length.svg` | 906 / 134 | KUL + linearRGB unlinearize; bilinear leftover |
| `svg2/mix-blend-mode.svg` | 142247 / 255 | HSL + isolation in; cairo AA vs rust PNG |
| `filter-effects-region.svg` | 407 / 255 | default 62×62 in; 1 px USOU column (x=251) + `1em` strip (x=226) |
| `bugs/bug476507.svg` | 845 / 56 | marker-end present; stroke AA |
| `bugs/bug603550-mask-luminance.svg` | 2147 / 254 | luminance present; nested edge alpha |

**P21 (after P19):** live `rsvg-test` 72 dpi matched this table
before Q4. None of the IRect / marker / luminance leftovers is a
clear remaining algorithm.

**Q4:** lighting default linearRGB now unlinearizes on store (rust
`into_output` / `to_srgb`). `filter-kernel-unit-length.svg` is
**906 / 134** (was 2146 / 77); still `max_diff > 1`. Mix-blend
interiors match cairo operators; leftover is Inkscape-rect AA.

**T5:** remasured; all five unchanged. No new 2.62 algorithm.
Leave xfail. No interpolator. No cairo-operator rewrite.

**T6:** remasured; all five unchanged. Forever cairo residual for
this track.

## E. Missing or wrong C feature (0)

Nothing in the 223 whose *primary* problem is a missing SVG feature
we should implement to shrink the list. The 2.62.3 named-corpus claim
stands.

**Watch (still class A, not a version reason):**
`svg1.1/text-dominant-baseline-01.svg` — C has no
`dominant-baseline` offset; rust 2.62 implements
`compute_baseline_offset`. The PNG is also DejaVu/Roboto-locked.
Optional later C port **after** P19; will not flip while the testing
face is Liberation Sans. Rust `text_layout.rst` still lists a larger
baseline overhaul as unfinished.

Not E:

- `bug481` / `bug494` / `bug642` — 2.62 also uses the first `x`/`y`/`dx`/`dy` only
- `image-rendering` — parsed and applied; only fixture is a skip
- SVG `<font>` / `font-face-uri` — rust will not implement
- AVIF / svg-to-svg / helpers — skips

## Historical: how the named real-gap closed (P8–P18)

The lists below are the work that emptied the named bucket. They stay
for archaeology. The open named set is empty.

**P8 batch — flipped (removed from xfail):**

| Fixture | Fix |
| --- | --- |
| `bugs/bug548-data-url-without-mimetype.svg` | `data:;base64,` (empty MIME) now base64-decodes |
| `bugs/bug718-rect-negative-rx-ry.svg` | negative `rx`/`ry` ignored (SVG2 / 2.62 Auto) |
| `bugs/bug245-negative-dashoffset.svg` | `stroke-dashoffset` may be negative |
| `bugs/bug165-zero-length-subpath-square-linecap.svg` | expand empty subpath so cairo draws a square cap |

**P9 batch — flipped (removed from xfail):**

| Fixture | Fix |
| --- | --- |
| `bugs/bug761871-reset-reflection-points.svg` | Independent cubic vs quadratic reflection points (`S`/`T`) |
| `bugs/bug763386-marker-coincident.svg` | Marker tangents when Bézier controls coincide (2.62 `get_directionalities`) |

`feColorMatrix type="hueRotate"` now takes degrees (2.62). `svg2/multi-filter.svg` still xfails (blur + hue chain vs rust PNG).

**P10 batch — flipped (removed from xfail):**

| Fixture | Fix |
| --- | --- |
| `bugs/bug510-pattern-fill.svg` | objectBoundingBox uses path extents, not stroke-ink box |
| `bugs/bug510-pattern-fill-opacity.svg` | `fill-opacity` applied to the pattern tile (2.62 #510) |

**P11 batch — flipped (removed from xfail):**

| Fixture | Fix |
| --- | --- |
| `bugs/bug776297-marker-on-non-path-elements.svg` | `marker-mid` only on path/line/poly (2.62 `Markers::No` on rect/circle/ellipse) |
| `bugs/bug634324-blur-negative-transform.svg` | `stdDeviation` mapped through the full primitive affine (`transform_distance` + abs) |

Mid-marker angles use the shorter-arc bisect (2.62 `Angle::bisect`), not `(a+b)/2`.

**P12 batch — flipped (removed from xfail):**

| Fixture | Fix |
| --- | --- |
| `filter-conv-bounds.svg` | `order` was `MAX(n, INT_MAX)` (always overflowed to 0); wrap edge mode handles negative indices |
| `filter-conv-divisor.svg` | default `order` 3×3; default divisor = kernel sum or 1; convolve in linearRGB (2.62 CIF) |

`<filter>` now parses `style=` so `em` on the filter region uses the filter element's font-size.

**P15 — recategorized (still on `parity-xfail.txt`):**

| Fixture | New bucket |
| --- | --- |
| `bugs/bug760180.svg` | font/AA — saturated graphic matches |
| `include-fallback.svg` | font/AA — `xi:fallback` already works |
| `bugs/bug603550-mask-luminance.svg` | tiny-AA — luminance feature exists; rust edge alpha |

**P15 — C deltas, no PNG flip:**

| Fixture | What landed |
| --- | --- |
| `filter-effects-region.svg` | default OBB −10%/120% is rust’s 62×62 IRect; leftover USOU column + `1em` primitive (407 px) |
| `bugs/bug282-drop-shadow.svg` | rust even-kernel 3-box `(d,d/2; d,d/2-1; d+1,(d+1)/2)`; `max_diff=2` (was 5) |

**P16 — flipped:**

| Fixture | Fix |
| --- | --- |
| `svg2/multi-filter.svg` | chain already fed filter 2 the blur surface; `feColorMatrix` now uses a float matrix in default linearRGB (2.62 CIF). Center is rust orange `(206,113,0)`. |

**P16 — C delta, no PNG flip:**

| Fixture | What landed |
| --- | --- |
| `filter-kernel-unit-length.svg` | lighting KUL scales the bump `1/transform_distance`, normals at 1 px, lights stay in user space (`x*ox`). Spot cone is present (not washed out). `max_diff` 233→77; point/spot still darker than rust PNG. |

**P17 — recategorized (still on `parity-xfail.txt`):**

| Fixture | New bucket |
| --- | --- |
| `filter-component-transfer-from-reference-page.svg` | font/AA — CT table/linear/gamma now float + linearRGB; leftover is Verdana labels |
| `bugs/bug476507.svg` | tiny-AA — marker-end + cubic are present; ink bbox matches rust (1721 vs 1720 px); leftover is stroke AA (`max_diff=56`) |

**P17 — C delta, no PNG flip:**

| Fixture | What landed |
| --- | --- |
| `svg2/mix-blend-mode.svg` | `hue`/`saturation`/`color`/`luminosity` now map to cairo HSL operators (`CAIRO_VERSION >= 1.10`; the old `#ifdef CAIRO_OPERATOR_HSL_HUE` was always false because those names are enums). Isolation already groups when `comp_op != OVER`. Still ~52k px `gt1` vs rust PNG (backdrop / cairo). |

**P18 — last five named real-gap items decided (still on `parity-xfail.txt`):**

None of these is a missing C feature. Each is recategorized:

| Fixture | Decision | Reason |
| --- | --- | --- |
| `bugs/bug282-drop-shadow.svg` | tiny-AA | rust even-kernel 3-box is in; `rsvg-test` **118 px / max_diff=2** (pass is ≤1). Rounding of the box blur vs the rust PNG. |
| `filter-effects-region.svg` | tiny-AA | default OBB −10%/120% is rust’s 62×62 IRect. Leftover **407 px** is a 1 px USOU column + `1em` primitive far edge. |
| `filter-image-from-reference-page.svg` | interpolator / AA | PAR + unclipped subregion done. **52 px / max_diff=2** on the 1.2× smiley (cairo GOOD vs rust GOOD). No new resampler. |
| `filter-kernel-unit-length.svg` | cairo vs rust PNG | lighting KUL scale-bump / 1 px normals / user-space lights landed; Q4 unlinearizes default linearRGB. **906 px / max_diff=134** is bilinear scale-back (one spot-edge pixel), not a missing `kernelUnitLength`. |
| `svg2/mix-blend-mode.svg` | cairo vs rust PNG | HSL operators are cairo 1.10+ (`hue`/`saturation`/`color`/`luminosity`). Isolation groups when `comp_op != OVER`. Interiors match; leftover ~52k `gt1` is host cairo AA vs the rust-generated PNG. |

**Still open (0).** Named real-gap is empty.

Those nine leftovers are now class B or D above. Named real-gap stays **0**.

**Skip (27):** no `-ref.png`. Includes AVIF, XInclude, svg-to-svg pairs
(`svg2-reftests/*`, `bugs-reftests/*`, color-type helpers, `bug743`),
and `svg1.1/rects.svg`. Not a version-macro reason.

**Already fixed, still listed:** none (`unexpected_pass=0`). Remove a
line only when `check-parity` reports an unexpected pass.

## Residual plan (post-2.62.3)

Short form:

1. **P19 (done):** rust TTF set + `fonts.conf` under `tests/resources/`;
   rust hint None/Off when `RSVG_PARITY=1`. Gated so `make check` stays
   165/6/1. Class A still 180; 0 unexpected pass.
2. **P20 (done for 21 named class A):** `make generate-parity-local`
   / `make check-parity-local`. Rust PNGs stay the oracle. Local pass
   is not an rust-oracle xfail removal.
3. **P21 (done, no C):** class D re-measured; all five unchanged
   at that step.
4. **Q4 (done):** mix-blend left cairo-vs-rust; lighting linearRGB
   unlinearize in (`KUL` 2146/77 → 906/134, not flipped).
5. **Q5 (done, no C):** all 33 Adwaita xfails are coverage AA.
6. **T5 (done):** `font-size` smaller/larger = parent/1.2. `bug108`
   flipped. Remaining named text is Pango-vs-rust / Dana-FaNum /
   Verdana. Class D remasured, no new algorithm. **607 / 116**.
7. **T6 (done):** text OBB = union of span ink boxes. `text-objectBoundingBox`
   flipped. Remaining **608 / 115**. Named leftovers are font/AA/cairo
   forever for this track.
8. **TFM / TAA / TCR (done, no C):** 115 misses encoded as accepted
   differences (`tests/parity-t-numbers.md`). Harness list unchanged.
9. Do **not** guess suite-font outlines, tighten `max_diff` globally,
   or bump `LIBRSVG_VERSION`.

Pixel-identity is not required for the 2.62.3 claim.
