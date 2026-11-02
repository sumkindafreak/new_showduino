"""Stage A: unchanged production code and real pinned Adafruit buffer implementation."""
from pathlib import Path
import argparse, subprocess, tempfile, sys
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/audio-node-esp32-a1s/ShowduinoAudioNode'
p=argparse.ArgumentParser()
p.add_argument('--neopixel-dir',type=Path,required=True)
p.add_argument('--sanitize',action='store_true',help='Address/undefined behavior checks on a supported host compiler')
args=p.parse_args()
properties=dict(line.split('=',1) for line in (args.neopixel_dir/'library.properties').read_text().splitlines() if '=' in line)
assert properties.get('version')=='1.15.5','Use pinned Adafruit NeoPixel 1.15.5'
with tempfile.TemporaryDirectory(prefix='showduino-audio-output-') as tmp:
    for test in ('output','engine','status'):
        binary=Path(tmp)/(test+('.exe' if sys.platform=='win32' else ''))
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
            *(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []),
            '-DARDUINO=10800','-DESP32','-DARDUINO_ARCH_ESP32','-DSHOWDUINO_LOG_H',
            '-I'+str(HERE/'stubs'),'-I'+str(args.neopixel_dir),'-I'+str(SKETCH),'-I'+str(SKETCH/'src'),
            '-include',str(HERE/'support.h'),str(HERE/('test_'+test+'.cpp')),
            str(HERE/'instrumented_output.cpp'),str(args.neopixel_dir/'Adafruit_NeoPixel.cpp'),
            '-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
