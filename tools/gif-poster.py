# Prepends frame N of a GIF (held for MS milliseconds) so the first frame, which is
# what static previews show, already has the panel on it. Usage: gif-poster.py file.gif N MS
import sys
from PIL import Image, ImageSequence
path, n, ms = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
im = Image.open(path)
frames = [f.convert('RGB') for f in ImageSequence.Iterator(im)]
durs = [f.info.get('duration', 50) for f in ImageSequence.Iterator(Image.open(path))]
frames.insert(0, frames[n]); durs.insert(0, ms)
pal = [f.quantize(colors=128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for f in frames]
pal[0].save(path, save_all=True, append_images=pal[1:], duration=durs, loop=0, optimize=True, disposal=1)
print(path, len(frames))
