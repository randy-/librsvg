# Accepted result differences (tolerance classes)

Frozen **T-norm** for every `fail` line in `tests/parity-xfail.txt`.
These are accepted differences vs the rust 2.62.3 oracle PNGs, not
open defects. Structure (path / fill / text / feature) is present.

Counts after T6 (`make check-parity`, 72 dpi, `RSVG_PARITY=1`):
**pass=608 / fail=0 / xfail=115 / skip=27 / unexpected_pass=0**.

`LIBRSVG_VERSION` stays **2.62.3**. Pixel-identity is not required.

Policy: `tests/README-parity.md`.
Machine-readable sibling the harness loads:
`tests/parity-t-numbers.txt` (`class<TAB>path<TAB>px<TAB>max_diff`).

`make check-parity` applies the tolerance band (see `tests/README-parity.md`).
In-band residuals are silent passes. Out-of-spec prints the TFM/TAA/TCR
and numbers. `tests/parity-xfail.txt` is documentation, not TAP.

Narrative: `tests/parity-xfail-categories.md`.

## How to read a row

| Column | Meaning |
| --- | --- |
| Class | **TFM** font/metrics, **TAA** coverage AA, **TCR** cairo-vs-rust |
| Norm | live `pixels / max_diff` at 72 dpi / `RSVG_PARITY=1` |
| Reason | why this is accepted, not a missing feature |

**E** (missing / wrong C feature) is **0**. A new hole, missing
feature, or opaque-vs-empty on an TAA icon is an **error**, not a
new tolerance class.

Numbers marked **live** were remasured after P19–T6. Unmarked
svg1.1 / Adwaita norms are the recorded `parity-xfail.txt` values
(already 72 dpi / `RSVG_PARITY=1` snapshots). Do not re-measure
to chase suite-font or Adwaita AA.

## Totals

| Class | Count | What it is |
| --- | ---: | --- |
| TFM | 74 | Pango-vs-rust / missing face / SVG 1.1 suite-font labels |
| TAA | 37 | Coverage AA (33 Adwaita + 4 named interpolator/AA) |
| TCR | 5 | Feature in; host cairo vs rust-generated PNG |
| E | 0 | — |
| **Listed** | **116** | accepted differences |

`116 = 74 + 37 + 5`. Directory split: root 5, `adwaita/` 33,
`bugs/` 8, `svg1.1/` 69, `svg2/` 1.

---

## TFM — font / metrics (74)

Pango layout vs rust’s path-filled Roboto/Ahem/DejaVu PNG. Do not
invent SVGFreeSans or Dana-FaNum outlines.

### Named (6) — live

| Fixture | Class | Norm | Reason |
| --- | --- | --- | --- |
| `rtl-tspan.svg` | TFM | **1977 / 255** live | Dana-FaNum not in the rust TTF set |
| `bugs/bug340047.svg` | TFM | **777 / 24** live | `baseline-shift` super/sub is in C; Pango-vs-rust italic metrics |
| `bugs/bug587721-text-transform.svg` | TFM | **472 / 255** live | DejaVu `font-size="1"` + `matrix(12,…)`; not CSS `text-transform` |
| `bugs/bug730-font-scaling.svg` | TFM | **4450 / 46** live | Nested `viewBox` scales of the same sentence; scaled-font AA |
| `filter-component-transfer-from-reference-page.svg` | TFM | **4578 / 150** live | CT table/linear/gamma match; leftover is Verdana 75px labels |
| `svg1.1/text-text-03-b.svg` | TFM | **12 / 255** | CRUX isolated-font Pango line-through vs rust oracle (not TAA: opaque-vs-empty would still TAP-fail) |

### SVG 1.1 suite (69)

Every source names `SVGFreeSansASCII` (deprecated SVG `<font>`; rust
will not implement). The rust PNG is Roboto. Features underneath may
already be in C. Do not implement anything just to move these numbers.

| Fixture | Class | Norm | Reason |
| --- | --- | --- | --- |
| `svg1.1/coords-trans-02-t.svg` | TFM | 6589 / 255 | suite-font labels on transform |
| `svg1.1/coords-trans-03-t.svg` | TFM | 5604 / 255 | suite-font labels on transform |
| `svg1.1/coords-trans-04-t.svg` | TFM | 3911 / 255 | suite-font labels on transform |
| `svg1.1/coords-trans-05-t.svg` | TFM | 7225 / 255 | suite-font labels on transform |
| `svg1.1/coords-trans-06-t.svg` | TFM | 7958 / 255 | suite-font labels on transform |
| `svg1.1/coords-trans-09-t.svg` | TFM | 16194 / 255 | suite-font labels on transform |
| `svg1.1/coords-viewattr-02-b.svg` | TFM | 9728 / 255 | suite-font labels on viewBox |
| `svg1.1/coords-viewattr-04-f.svg` | TFM | 18413 / 255 | suite-font labels on viewBox |
| `svg1.1/filters-background-01-f.svg` | TFM | 17584 / 255 | suite-font labels on filter page |
| `svg1.1/filters-blend-01-b.svg` | TFM | 125472 / 255 | suite-font labels on filter page |
| `svg1.1/filters-color-01-b.svg` | TFM | 57608 / 255 | suite-font labels on filter page |
| `svg1.1/filters-color-02-b.svg` | TFM | 22046 / 255 | suite-font labels on filter page |
| `svg1.1/filters-composite-02-b.svg` | TFM | 35625 / 255 | suite-font labels on filter page |
| `svg1.1/filters-composite-03-f.svg` | TFM | 42053 / 255 | suite-font labels on filter page |
| `svg1.1/filters-composite-04-f.svg` | TFM | 41822 / 255 | suite-font labels on filter page |
| `svg1.1/filters-composite-05-f.svg` | TFM | 21091 / 255 | suite-font labels on filter page |
| `svg1.1/filters-comptran-01-b.svg` | TFM | 50548 / 255 | suite-font labels on filter page |
| `svg1.1/filters-conv-01-f.svg` | TFM | 22431 / 255 | suite-font labels on filter page |
| `svg1.1/filters-conv-02-f.svg` | TFM | 25103 / 255 | suite-font labels on filter page |
| `svg1.1/filters-conv-03-f.svg` | TFM | 15306 / 255 | suite-font labels on filter page |
| `svg1.1/filters-conv-04-f.svg` | TFM | 68367 / 255 | suite-font labels on filter page |
| `svg1.1/filters-diffuse-01-f.svg` | TFM | 22172 / 255 | suite-font labels on filter page |
| `svg1.1/filters-displace-02-f.svg` | TFM | 40352 / 255 | suite-font labels on filter page |
| `svg1.1/filters-felem-02-f.svg` | TFM | 5290 / 255 | suite-font labels on filter page |
| `svg1.1/filters-gauss-01-b.svg` | TFM | 23600 / 255 | suite-font labels on filter page |
| `svg1.1/filters-gauss-02-f.svg` | TFM | 14371 / 255 | suite-font labels on filter page |
| `svg1.1/filters-image-03-f.svg` | TFM | 18651 / 255 | suite-font labels on filter page |
| `svg1.1/filters-light-01-f.svg` | TFM | 35353 / 255 | suite-font labels on filter page |
| `svg1.1/filters-light-02-f.svg` | TFM | 7965 / 255 | suite-font labels on filter page |
| `svg1.1/filters-light-03-f.svg` | TFM | 18996 / 255 | suite-font labels on filter page |
| `svg1.1/filters-light-04-f.svg` | TFM | 11092 / 255 | suite-font labels on filter page |
| `svg1.1/filters-light-05-f.svg` | TFM | 44482 / 255 | suite-font labels on filter page |
| `svg1.1/filters-morph-01-f.svg` | TFM | 10331 / 255 | suite-font labels on filter page |
| `svg1.1/filters-offset-01-b.svg` | TFM | 12032 / 255 | suite-font labels on filter page |
| `svg1.1/filters-overview-01-b.svg` | TFM | 49903 / 255 | suite-font labels on filter page |
| `svg1.1/filters-overview-02-b.svg` | TFM | 53009 / 255 | suite-font labels on filter page |
| `svg1.1/filters-overview-03-b.svg` | TFM | 53090 / 255 | suite-font labels on filter page |
| `svg1.1/filters-turb-01-f.svg` | TFM | 56034 / 255 | suite-font labels on filter page |
| `svg1.1/filters-turb-02-f.svg` | TFM | 33043 / 255 | suite-font labels on filter page |
| `svg1.1/masking-filter-01-f.svg` | TFM | 24679 / 255 | suite-font labels on clip/mask page |
| `svg1.1/painting-control-02-f.svg` | TFM | 4101 / 255 | suite-font labels on painting page |
| `svg1.1/painting-marker-04-f.svg` | TFM | 9329 / 255 | suite-font labels on marker page |
| `svg1.1/painting-marker-properties-01-f.svg` | TFM | 13588 / 255 | suite-font labels on marker page |
| `svg1.1/painting-stroke-09-t.svg` | TFM | 5126 / 255 | suite-font labels on stroke page |
| `svg1.1/paths-data-04-t.svg` | TFM | 6240 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-05-t.svg` | TFM | 4792 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-06-t.svg` | TFM | 4576 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-07-t.svg` | TFM | 4182 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-08-t.svg` | TFM | 7655 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-09-t.svg` | TFM | 6291 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-14-t.svg` | TFM | 2971 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-18-f.svg` | TFM | 4125 / 255 | suite-font labels on path commands |
| `svg1.1/paths-data-20-f.svg` | TFM | 59395 / 255 | suite-font labels on path commands |
| `svg1.1/pservers-grad-07-b.svg` | TFM | 14536 / 255 | suite-font labels on gradient page |
| `svg1.1/pservers-grad-08-b.svg` | TFM | 41931 / 255 | suite-font labels on gradient page |
| `svg1.1/pservers-grad-16-b.svg` | TFM | 29267 / 255 | suite-font labels on gradient page |
| `svg1.1/pservers-grad-18-b.svg` | TFM | 12867 / 255 | suite-font labels on gradient page |
| `svg1.1/pservers-pattern-03-f.svg` | TFM | 83507 / 255 | suite-font labels on pattern page |
| `svg1.1/pservers-pattern-09-f.svg` | TFM | 75507 / 255 | suite-font labels on pattern page |
| `svg1.1/struct-group-03-t.svg` | TFM | 15986 / 255 | suite-font labels on group |
| `svg1.1/struct-use-01-t.svg` | TFM | 9650 / 255 | suite-font labels on `use` |
| `svg1.1/struct-use-04-b.svg` | TFM | 5248 / 255 | suite-font labels on `use` |
| `svg1.1/struct-use-10-f.svg` | TFM | 9046 / 255 | suite-font labels on `use` |
| `svg1.1/text-align-02-b.svg` | TFM | 21239 / 255 | suite-font / Pango-vs-rust text align |
| `svg1.1/text-dominant-baseline-01.svg` | TFM | 112492 / 255 | DejaVu/Roboto-locked PNG; `dominant-baseline` watch is not E |
| `svg1.1/text-text-03-b.svg` | TFM | 12 / 255 | CRUX isolated-font Pango line-through vs rust oracle |
| `svg1.1/text-text-10-t.svg` | TFM | 32576 / 255 | suite-font / Pango-vs-rust text |
| `svg1.1/text-tref-02-b.svg` | TFM | 7430 / 255 | suite-font / Pango-vs-rust `tref` |
| `svg1.1/types-basic-02-f.svg` | TFM | 16911 / 255 | suite-font labels on types page |

---

## TAA — coverage AA (37)

Placement is correct. Leftover is cairo coverage vs the rust PNG.
Do not rewrite cairo stroke/fill. Do not loosen `max_diff ≤ 1`.

A new **opaque-vs-empty** hole on an Adwaita icon is an error, not
an tolerance class.

### Named (4) — live

| Fixture | Class | Norm | Reason |
| --- | --- | --- | --- |
| `bugs/bug241-light-source-type.svg` | TAA | **1331 / 7** live | lighting types present; coverage AA |
| `bugs/bug282-drop-shadow.svg` | TAA | **118 / 2** live | rust even-kernel 3-box in; box-blur rounding |
| `bugs/bug590-mask-units.svg` | TAA | **55 / 6** live | mask units present; coverage AA |
| `filter-image-from-reference-page.svg` | TAA | **52 / 2** live | PAR + unclipped subregion in; cairo GOOD vs rust GOOD |

### Adwaita symbolic (33)

Path-only 16×16 icons (`stroke:none`, `#bebebe`). Q5: **0** pixels
are fully opaque on one side and empty on the other. `max_diff`
2–60 (median 9; 30 of 33 ≤ 20).

| Fixture | Class | Norm | Reason |
| --- | --- | --- | --- |
| `adwaita/alarm-symbolic.svg` | TAA | 111 / 21 | coverage AA; 0 opaque-vs-empty |
| `adwaita/application-x-appliance-symbolic.svg` | TAA | 142 / 13 | coverage AA; 0 opaque-vs-empty |
| `adwaita/audio-card-symbolic.svg` | TAA | 95 / 17 | coverage AA; 0 opaque-vs-empty |
| `adwaita/audio-volume-overamplified-symbolic.svg` | TAA | 96 / 3 | coverage AA; 0 opaque-vs-empty |
| `adwaita/bluetooth-active-symbolic.svg` | TAA | 63 / 8 | coverage AA; 0 opaque-vs-empty |
| `adwaita/bluetooth-symbolic.svg` | TAA | 63 / 8 | coverage AA; 0 opaque-vs-empty |
| `adwaita/document-properties-symbolic.svg` | TAA | 80 / 17 | coverage AA; 0 opaque-vs-empty |
| `adwaita/edit-cut-symbolic.svg` | TAA | 68 / 5 | coverage AA; 0 opaque-vs-empty |
| `adwaita/edit-select-all-symbolic.svg` | TAA | 82 / 3 | coverage AA; 0 opaque-vs-empty |
| `adwaita/emoji-activities-symbolic.svg` | TAA | 9 / 2 | coverage AA; 0 opaque-vs-empty |
| `adwaita/find-location-symbolic.svg` | TAA | 90 / 4 | coverage AA; 0 opaque-vs-empty |
| `adwaita/focus-top-bar-symbolic.svg` | TAA | 23 / 16 | coverage AA; 0 opaque-vs-empty |
| `adwaita/folder-open-symbolic.svg` | TAA | 35 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/folder-remote-symbolic.svg` | TAA | 39 / 10 | coverage AA; 0 opaque-vs-empty |
| `adwaita/folder-symbolic.svg` | TAA | 36 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/go-home-symbolic.svg` | TAA | 62 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/input-mouse-symbolic.svg` | TAA | 31 / 6 | coverage AA; 0 opaque-vs-empty |
| `adwaita/input-tablet-symbolic.svg` | TAA | 96 / 60 | coverage AA; 0 opaque-vs-empty |
| `adwaita/insert-link-symbolic.svg` | TAA | 56 / 17 | coverage AA; 0 opaque-vs-empty |
| `adwaita/network-no-route-symbolic.svg` | TAA | 11 / 2 | coverage AA; 0 opaque-vs-empty |
| `adwaita/network-wired-acquiring-symbolic.svg` | TAA | 69 / 10 | coverage AA; 0 opaque-vs-empty |
| `adwaita/network-wired-no-route-symbolic.svg` | TAA | 73 / 8 | coverage AA; 0 opaque-vs-empty |
| `adwaita/object-select-symbolic.svg` | TAA | 8 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/orientation-portrait-inverse-symbolic.svg` | TAA | 20 / 17 | coverage AA; 0 opaque-vs-empty |
| `adwaita/orientation-portrait-symbolic.svg` | TAA | 19 / 17 | coverage AA; 0 opaque-vs-empty |
| `adwaita/rotation-allowed-symbolic.svg` | TAA | 70 / 13 | coverage AA; 0 opaque-vs-empty |
| `adwaita/rotation-locked-symbolic.svg` | TAA | 94 / 19 | coverage AA; 0 opaque-vs-empty |
| `adwaita/system-shutdown-symbolic.svg` | TAA | 59 / 15 | coverage AA; 0 opaque-vs-empty |
| `adwaita/tab-new-symbolic.svg` | TAA | 51 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/user-home-symbolic.svg` | TAA | 62 / 9 | coverage AA; 0 opaque-vs-empty |
| `adwaita/view-mirror-symbolic.svg` | TAA | 110 / 5 | coverage AA; 0 opaque-vs-empty |
| `adwaita/view-restore-symbolic.svg` | TAA | 64 / 26 | coverage AA; 0 opaque-vs-empty |
| `adwaita/weather-windy-symbolic.svg` | TAA | 76 / 6 | coverage AA; 0 opaque-vs-empty |

---

## TCR — cairo vs rust PNG (5) — live

Feature is in. Residual is host cairo vs the rust-generated PNG.
No new interpolator. No cairo-operator rewrite.

| Fixture | Class | Norm | Reason |
| --- | --- | --- | --- |
| `filter-kernel-unit-length.svg` | TCR | **906 / 134** live | KUL + linearRGB unlinearize in; bilinear scale-back |
| `svg2/mix-blend-mode.svg` | TCR | **142247 / 255** live | HSL + isolation in; cairo AA vs rust PNG |
| `filter-effects-region.svg` | TCR | **407 / 255** live | default 62×62 IRect in; 1 px USOU / `em` |
| `bugs/bug476507.svg` | TCR | **845 / 56** live | marker-end in; stroke AA |
| `bugs/bug603550-mask-luminance.svg` | TCR | **2147 / 254** live | luminance in; nested edge alpha |

---

## E — missing feature (0)

Empty. `svg1.1/text-dominant-baseline-01.svg` stays **TFM** (font-locked
PNG). Skips (27) have no `-ref.png` and are not tolerance classes.

## Regression rule

- **Unexpected pass** → remove the line from `parity-xfail.txt` and
  this catalog.
- **Same class, similar magnitude** → accept (still an tolerance class).
- **New hole / missing feature / opaque-vs-empty on an TAA icon** →
  error, not an tolerance class.
- `make check-parity-local` is a **hard error** (same-renderer). It
  does not use this list.
