# 2.62.3 parity reftests (P0)

`tests/fixtures/parity/` is a read-only copy of
`rust/rsvg/tests/fixtures/reftests` from the librsvg **2.62.3** snapshot.

The 2.52.0 baseline suite is unchanged: `make check` still runs
`tests/api.c` and `tests/fixtures/reftests/` only.

## How to run the parity corpus

From the out-of-tree build directory (never install onto the host):

```sh
# rebuild the harness if rsvg-test.c changed
make -C tests rsvg-test

# run the 2.62.3 PNG reftests (in-band tolerance classes are silent passes)
make -C tests check-parity
```

The default matcher is the `css/` engine. `make check-parity` and
`make check-parity-engine` load `tests/parity-t-numbers.txt` and
apply the tolerance band. They do **not** TAP-skip the 115 residuals.

`make check-parity-croco` errors: libcroco is discontinued.

Or, with the same environment `make check` uses:

```sh
cd tests   # in the build tree
G_TEST_SRCDIR=/path/to/src/tests \
G_TEST_BUILDDIR=$PWD \
RSVG_PARITY=1 \
RSVG_PARITY_XFAIL=/path/to/src/tests/parity-xfail.txt \
./rsvg-test
```

Pass a path to run one fixture (works with or without `RSVG_PARITY`):

```sh
./rsvg-test /path/to/tests/fixtures/parity/specificity.svg
```

## What is run

The C harness compares each `foo.svg` to `foo-ref.png` (same rule as
2.62.3 `reference.rs` and the existing 2.40 `rsvg-test`).

Not run by this harness (listed as `skip` in the xfail file):

- `*-ref.svg` pairs (SVG-to-SVG tests in 2.62.3)
- fixtures with no companion `-ref.png` (resources, helpers)
- `ignore*.svg` (already skipped by `rsvg-test`)
- the `resources/` and `images/` subdirectories (companion assets)

Adwaita icons, svg1.1, svg2, and bugs are all included when a PNG
reference exists.

## Accepted result differences (TFM / TAA / TCR)

TFM / TAA / TCR are **tolerance classes**, not TAP output.
`make check-parity` is pass/fail only. Report a class only when a
fixture is out of spec.

Pass = rust-oracle match (`max_diff ≤ 1`). That rule is not loosened.
The remaining rust-oracle misses are **accepted differences**, not
defects — unless structure is wrong (missing path / fill / text /
feature).

| Class | Meaning |
| --- | --- |
| **TFM** | font / metrics (Pango-vs-rust, suite-font, missing face) |
| **TAA** | coverage AA (Adwaita path icons, named interpolator/AA) |
| **TCR** | cairo vs rust PNG on an implemented feature |
| **E** | missing / wrong C feature (**none** now) |

Frozen per-fixture class + T-norm (`pixels / max_diff` at 72 dpi /
`RSVG_PARITY=1`): `tests/parity-t-numbers.md`. Machine-readable
sibling loaded by the harness: `tests/parity-t-numbers.txt`.
Narrative: `tests/parity-xfail-categories.md`.

### Two bands

| Suite | Refs | Norms | TAP |
| --- | --- | --- | --- |
| `make check` | 2.40 C PNGs in `fixtures/reftests/` | `tests/make-check-t-numbers.txt` (six residuals) | silent in-band PASS |
| `make check-parity` | rust 2.62.3 oracle | `tests/parity-t-numbers.txt` | silent in-band PASS |
| `make check-parity-local` | same-renderer | none | **strict** (`max_diff ≤ 1`) |

`max_diff ≤ 1` is still a perfect silent pass on every path.

### Band (same formula on both rust-oracle and make-check refs)

For each PNG comparison:

1. `max_diff ≤ 1` → **PASS** (silent).
2. Else if the fixture has a T-norm:
   - **PASS** (silent) if both
     `px ≤ max(2 × norm_px, norm_px + 50)` and
     `max_diff ≤ max(2 × norm_max_diff, norm_max_diff + 2)`.
   - **FAIL** out of spec:
     `TFM rtl-tspan out of spec: 2500/255 (norm 1977/255)`.
   - **TAA:** if any pixel is fully opaque on one side and empty
     on the other → **FAIL** (real error), even if px is in band.
3. No T-norm and not a perfect match → **FAIL**. New misses are
   not silently accepted.

In-band residuals are TAP `ok`, not `xfail` / `SKIP`.
`check-parity` reports `fail=0` when every residual is in band.

`tests/parity-xfail.txt` is the catalog of accepted differences
(documentation / archaeology). It is **not** an open-bug list and
is **not** what `check-parity` TAP-skips.

### Regression rule

- In-band residual → silent pass.
- Out of spec → FAIL with the TFM/TAA/TCR and numbers.
- Perfect match of a former residual → silent pass.
- New unlisted miss → FAIL (not a new tolerance class).
- **TAA opaque-vs-empty** hole → FAIL, even in band.

## Known residual: `svg1.1/text-text-03-b.svg`

Accepted **C × Pango line-through** vs the rust 2.62.3 oracle
(`pixels=12`, `max_diff=255` on the CRUX isolated-font host).  Not a
missing feature, not a missing font file.  Class **TFM** (not TAA:
opaque-vs-empty on this strike would still TAP-fail).  T-norm
**12 / 255** in `tests/parity-t-numbers.txt`; the usual band
(`2×` / `+50` and `2×` / `+2`) applies.  On CRUX, `make check-parity`
should then be `fail=0` for this name when in-band.  This host may
match the oracle (`max_diff ≤ 1`).  Do not raise the global
`max_diff` band or edit fixtures/oracles.

Detail (maintainer tree, not in the public tarball):
`crux-test-fail/KNOWN-FAIL-text-text-03-b.md`.

To dump a fresh miss list after a renderer change (inspect; do not
copy blindly):

```sh
RSVG_PARITY=1 RSVG_PARITY_XFAIL_OUT=/tmp/parity-xfail.txt \
  G_TEST_SRCDIR=... G_TEST_BUILDDIR=... ./rsvg-test
```

## Same-renderer local refs (P20)

`make check-parity` always compares against the **rust 2.62.3 oracle**
PNGs in `fixtures/parity/*-ref.png`. That is the version-claim corpus.
Do not replace those files.

`make check-parity-local` is a separate path: this renderer vs itself.
A local miss is a **hard error**. It does **not** apply the tolerance band
(`max_diff ≤ 1` only).

```sh
# regenerate host-local PNGs (rsvg-convert -d 72 -p 72, RSVG_PARITY=1)
make -C tests generate-parity-local

# compare the class A named subset to those PNGs
make -C tests check-parity-local
```

Local PNGs live in `fixtures/parity-local/` (same relative names,
`-ref.png`). The list is `tests/parity-local.txt` (21 named class A
text fixtures). They are **not** the 2.62.3 oracle and do not change
`LIBRSVG_VERSION`.

Do not remove lines from `parity-xfail.txt` because a local ref
matches. Only rust-oracle `unexpected_pass` may drop an accepted-
difference line.

## Rules

- Do not implement features just to shrink this list in the P0 step.
- `LIBRSVG_VERSION` is **2.62.3** after P18 (named real-gap empty).
  Do not bump further for TFM / TAA / TCR accepted differences.
- Do not build or link `libcroco/` (discontinued archive; css/ only).
- `RSVG_PARITY=1` loads only the rust 2.62.3 TTF list (Roboto / Ahem /
  DejaVu / Noto Hebrew) + `fonts.conf` (P19 / Q1). LiberationSans is
  **not** in that map. `make check` still uses LiberationSans.
- `RSVG_PARITY=1` fills text as a cairo path in paint-order (Q2,
  matching rust `drawing_ctx.rs`). `make check` still uses
  `show_layout` so the 2.40 baseline PNGs stay green.
