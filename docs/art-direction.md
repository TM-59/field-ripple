# M4: the field observatory

## Direction

Ink-blue scientific concept art, cyan light and restrained amber instruments, with a quiet, readable measurement interface.

This is a physics experiment without characters or enemies. A single illustrated laboratory gives the entry and observation header a coherent setting. Mathematical curves remain calculated from `src/field.c`, never generated images. The existing terminal renderer, CSV, SVG and animated GIF remain available.

## Original artwork

Generated: 2026-10-24, one image with Codex built-in image_gen; one accepted, none rejected. The existing generated image was retained on resumption rather than generated again. No third-party artwork, fonts, audio or franchise designs were imported. No generation API key was used.

Adopted asset: `assets/observatory.jpg`, 1440 × 960, 249,674 bytes. This is a compressed copy of the generated original. It is visibly used as the entry background and the observation header background, with text protection gradients. If it fails to load, a visible notice appears and the controls and physical plots still work.

### Recorded generation prompt

> Use case: stylized-concept. Asset type: original background illustration for Field Ripple, an interactive sine-Gordon scalar field physics observatory. Create one exquisite wide 1536x1024 painterly science-fiction observatory interior: a long experimental wave chamber stretching horizontally from foreground left to vanishing point right, fine copper instrument rails, pale cyan luminous ribbon forming one smooth S-shaped kink between two flat levels, dark ink-blue room, soft warm amber indicator lamps, a few distant star windows and no people. Analog scientific instrument craftsmanship with restrained modern editorial concept-art quality, atmospheric perspective, carefully drawn machinery, soft film grain. The bottom and left third have dark uncluttered space to overlay interface text. Cyan and amber accents on deep navy. This is an evocative fictional laboratory scene, not a scientific chart: absolutely no labels, equations, numerals, words, logos, watermarks, UI panels, grids or frames. Original design only, no existing franchise references. This background will be visibly used on an application's entry screen and short header banner; the real quantitative field plot is drawn separately by code.

### Visual inspection and adoption

The image and application screenshots were inspected directly. The original contains an empty observatory, copper rails, cyan light and star windows, without garbled lettering, logos or people. The luminous ribbon is an atmospheric depiction; it is not the computed sine-Gordon profile. The entry explicitly calls the illustration an imagined laboratory. The generated ribbon's shape is not used as a measurement.

The title remains readable on the dark left side. Inside the observatory the image is confined to the header; quantitative plots sit on an opaque panel. Solid cyan, dashed grey and a separate amber density plot distinguish the measurements by line style and position as well as colour. The field axis uses radians, the distance axis follows the kink, and the position strip locates it in the finite domain.

## Local draft review

A local gemma4:e4b draft proposed checks for a hero, enemies, title artwork, image failure and load size. Hero/enemy checks were rejected because this experiment has neither. Image decode/failure, actual title rendering and the total build budget were adopted. A production fallback for oversized images was rejected: the complete build fails its size gate instead. A test tied to an arbitrary screenshot count was replaced by actual keyboard/touch and physical-result checks.

## Before and after

- `docs/images/before.png`: retained actual terminal field rendering from before M4.
- `docs/images/title.jpg`: original art in the actual browser entry.
- `docs/images/coasting.jpg`: measured coasting at initial speed 0.6 c.
- `docs/images/contracted.jpg`: measured contraction at initial speed 0.9 c, with a rest-profile comparison.
- `docs/images/mobile.jpg`: actual 390-pixel touch-controlled observation.

The numeric art score is a material-completeness proxy, not a judgment of artistic quality. Human first-use impressions and physical-device readability remain unverified.
