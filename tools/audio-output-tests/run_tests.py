"""Compare Stage B to original Stage A code with the real pinned vendor buffer."""
from pathlib import Path
import argparse, subprocess, tempfile, sys, hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/audio-node-esp32-a1s/ShowduinoAudioNode'
p=argparse.ArgumentParser()
p.add_argument('--neopixel-dir',type=Path,required=True)
p.add_argument('--sanitize',action='store_true',help='Address/undefined behavior checks on a supported host compiler')
p.add_argument('--evidence-dir',type=Path,help='Save full original/candidate traces for review')
args=p.parse_args()
properties=dict(line.split('=',1) for line in (args.neopixel_dir/'library.properties').read_text().splitlines() if '=' in line)
assert properties.get('version')=='1.15.5','Use pinned Adafruit NeoPixel 1.15.5'
with tempfile.TemporaryDirectory(prefix='showduino-audio-output-') as tmp:
    for test in ('output','engine','status','frame'):
        traces=[]
        modes=('reference','candidate') if test in ('engine','status') else ('candidate',)
        for mode in modes:
            extra=[]
            if mode=='reference':
                filename='AudioPixelEngine.cpp' if test=='engine' else 'AudioStatusPixel.cpp'
                reference=Path(tmp)/filename
                reference.write_bytes(subprocess.check_output(['git','show',
                    '2d896781274fa21a34d421e9e6fd56bc897fe70e:firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/'+filename],cwd=ROOT))
                macro='AUDIO_ENGINE_SOURCE' if test=='engine' else 'AUDIO_STATUS_SOURCE'
                extra=['-D'+macro+'="'+reference.as_posix()+'"']
            binary=Path(tmp)/(test+'-'+mode+('.exe' if sys.platform=='win32' else ''))
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                *(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []),
                '-DARDUINO=10800','-DESP32','-DARDUINO_ARCH_ESP32','-DSHOWDUINO_LOG_H',*extra,
                '-I'+str(HERE/'stubs'),'-I'+str(args.neopixel_dir),'-I'+str(SKETCH),'-I'+str(SKETCH/'src'),
                '-include',str(HERE/'support.h'),str(HERE/('test_'+test+'.cpp')),
                str(HERE/'instrumented_output.cpp'),str(args.neopixel_dir/'Adafruit_NeoPixel.cpp'),
                '-o',str(binary)],check=True)
            trace=subprocess.check_output([str(binary)])
            traces.append(trace)
            if args.evidence_dir:
                args.evidence_dir.mkdir(parents=True,exist_ok=True)
                (args.evidence_dir/(test+'-'+mode+'.trace')).write_bytes(trace)
            if mode=='candidate':
                for line in trace.decode().splitlines():
                    if line.startswith('PASS '):print(line)
        if len(traces)==2:
            assert traces[0]==traces[1],test+' original/candidate frame, symbol, state, reply, cache or log trace differs'
            print('PASS',test,'exact original/candidate trace parity,',len(traces[0]),
                  'bytes, sha256='+hashlib.sha256(traces[0]).hexdigest())
