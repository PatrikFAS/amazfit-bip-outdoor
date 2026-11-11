"""Render captured app drawing calls; simulated sensors, approximate 9x16 font."""
from html import escape
from pathlib import Path
import sys

screens = []
foreground = '#FFFFFF'
for filename in sys.argv[1:]:
    for line in Path(filename).read_text().splitlines():
        if line.startswith('SCREEN\t'):
            foreground = '#FFFFFF'
            screens.append((line.split('\t', 1)[1], [], []))
        elif line.startswith('F\t'):
            _, color = line.split('\t', 1)
            if len(color) != 6 or any(char not in '0123456789abcdefABCDEF' for char in color):
                raise SystemExit(f'Invalid foreground color: {color!r}')
            # The SDK value is BGR (BBGGRR); SVG expects RGB (#RRGGBB).
            foreground = f'#{color[4:6]}{color[2:4]}{color[0:2]}'.upper()
        elif line.startswith('R\t'):
            x1, y1, x2, y2 = map(int, line.split('\t')[1:])
            if not (0 <= x1 <= x2 < 176 and 0 <= y1 <= y2 < 176):
                raise SystemExit(f'Rectangle bounds exceeded: {screens[-1][0]}')
            screens[-1][2].append((x1, y1, x2 - x1 + 1, y2 - y1 + 1, foreground))
        elif line.startswith(('T\t', 'C\t')):
            kind, x, y, text = line.split('\t', 3)
            x, y = int(x), int(y)
            width = len(text) * 9
            left = x - width / 2 if kind == 'C' else x
            if left < 0 or left + width > 176 or y < 0 or y + 16 > 176:
                raise SystemExit(f'Approximate font bounds exceeded: {screens[-1][0]}: {text!r} at {left},{y}')
            screens[-1][1].append((left, y, width, text, foreground))

assert screens, 'No captured app screens'
for screen_name, texts, rectangles in screens:
    for x, y, width, label, _ in texts:
        for rx, ry, rw, rh, _ in rectangles:
            if max(x, rx) < min(x + width, rx + rw) and max(y, ry) < min(y + 16, ry + rh):
                raise SystemExit(f'Text overlaps a rectangle: {screen_name}: {label!r}')
    for index, (x, y, width, label, _) in enumerate(texts):
        for other_x, other_y, other_width, other_label, _ in texts[index + 1:]:
            if (max(x, other_x) < min(x + width, other_x + other_width) and
                    max(y, other_y) < min(y + 16, other_y + 16)):
                raise SystemExit(f'Approximate text overlap: {screen_name}: {label!r} / {other_label!r}')
columns, cell_w, cell_h, scale = 4, 390, 420, 2
rows = (len(screens) + columns - 1) // columns
width, height = columns * cell_w, 90 + rows * cell_h
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
       '<rect width="100%" height="100%" fill="#e8edf0"/>',
       '<g font-family="sans-serif" fill="#152730">',
       '<text x="20" y="30" font-size="23">Bip Outdoor — local app-source previews</text>',
       '<text x="20" y="57" font-size="16">Simulated GPS/storage · approximate font · native C execution, not ARM/firmware emulation</text></g>']
for i, (name, texts, rectangles) in enumerate(screens):
    x, y = (i % columns) * cell_w + 20, 90 + (i // columns) * cell_h
    svg.append(f'<text x="{x}" y="{y}" font-family="sans-serif" font-size="17" fill="#152730">{escape(name)}</text>')
    svg.append(f'<g transform="translate({x},{y+14}) scale({scale})"><rect width="176" height="176" fill="black"/>')
    for left, top, rect_width, rect_height, color in rectangles:
        svg.append(f'<rect x="{left}" y="{top}" width="{rect_width}" height="{rect_height}" fill="{color}"/>')
    for left, top, text_width, text, color in texts:
        svg.append(f'<text x="{left}" y="{top+13}" fill="{color}" font-family="DejaVu Sans Mono,monospace" font-size="14" textLength="{text_width}" lengthAdjust="spacingAndGlyphs">{escape(text)}</text>')
    svg.append('</g>')
svg.append('</svg>')
Path('build/preview/screens.svg').write_text('\n'.join(svg))
print(f'Rendered {len(screens)} app-source scenarios; approximate text bounds and overlap checks passed.')
