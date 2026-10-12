"""Independent EXR QA. Install OpenEXR/numpy into build/qa/python-precision first."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'build/qa/python-precision'))
import OpenEXR
import numpy as np
source = ROOT/'build/qa/production/hdr.exr'
file = OpenEXR.File(str(source))
pixels = file.channels()['RGBA'].pixels
assert pixels.shape == (64,64,4) and pixels.dtype == np.float16
assert float(pixels[0,0,0]) >= .5 and float(pixels[0,0,1]) < 0
assert abs(float(pixels[0,0,3])-.25) < .001
result = {'reader': 'OpenEXR '+OpenEXR.__version__, 'file': str(source),
          'shape': list(pixels.shape), 'type': str(pixels.dtype),
          'hdr_negative_rgb_premultiplied_alpha': True}
(ROOT/'build/qa/precision-0.9-verification.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, indent=2))
