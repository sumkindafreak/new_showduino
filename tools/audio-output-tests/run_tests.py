"""Compare original, legacy and real FastLED RGB-frame paths with native output."""
from pathlib import Path
import argparse, subprocess, tempfile, sys, hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/audio-node-esp32-a1s/ShowduinoAudioNode'
p=argparse.ArgumentParser()
p.add_argument('--neopixel-dir',type=Path,required=True)
p.add_argument('--fastled-dir',type=Path,required=True)
p.add_argument('--sanitize',action='store_true',help='Address/undefined behavior checks on a supported host compiler')
p.add_argument('--evidence-dir',type=Path,help='Save full original/candidate traces for review')
args=p.parse_args()
properties=dict(line.split('=',1) for line in (args.neopixel_dir/'library.properties').read_text().splitlines() if '=' in line)
assert properties.get('version')=='1.15.5','Use pinned Adafruit NeoPixel 1.15.5'
properties=dict(line.split('=',1) for line in (args.fastled_dir/'library.properties').read_text().splitlines() if '=' in line)
assert properties.get('version')=='3.10.8','Use pinned FastLED 3.10.8'
with tempfile.TemporaryDirectory(prefix='showduino-audio-output-') as tmp:
    common=['g++','-std=c++17','-Wall','-Wextra','-Werror',
        *(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []),
        '-DARDUINO=10800','-DSHOWDUINO_LOG_H',
        '-I'+str(HERE/'stubs'),'-I'+str(args.neopixel_dir),'-I'+str(args.fastled_dir/'src'),
        '-I'+str(SKETCH),'-I'+str(SKETCH/'src'),'-include',str(HERE/'support.h')]
    # Keep the actual vendor ESP32 packing/release path while letting FastLED
    # select the real host integer widths in the renderer test translation unit.
    vendor=Path(tmp)/'neopixel.o'
    subprocess.run([*common,'-DESP32','-DARDUINO_ARCH_ESP32','-c',
        str(args.neopixel_dir/'Adafruit_NeoPixel.cpp'),'-o',str(vendor)],check=True)
    for test in ('output','engine','status','frame'):
        traces=[]
        modes=('reference','legacy','fastled') if test=='engine' else ('reference','candidate') if test=='status' else ('candidate','fastled') if test=='frame' else ('candidate',)
        for mode in modes:
            extra=['-DSHOWDUINO_AUDIO_FASTLED_RGB_FRAME='+('1' if mode=='fastled' else '0')]
            if mode=='reference':
                filename='AudioPixelEngine.cpp' if test=='engine' else 'AudioStatusPixel.cpp'
                reference=Path(tmp)/filename
                reference.write_bytes(subprocess.check_output(['git','show',
                    '2d896781274fa21a34d421e9e6fd56bc897fe70e:firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/'+filename],cwd=ROOT))
                macro='AUDIO_ENGINE_SOURCE' if test=='engine' else 'AUDIO_STATUS_SOURCE'
                extra+=['-D'+macro+'="'+reference.as_posix()+'"']
            binary=Path(tmp)/(test+'-'+mode+('.exe' if sys.platform=='win32' else ''))
            subprocess.run([*common,*extra,str(HERE/('test_'+test+'.cpp')),
                str(HERE/'instrumented_output.cpp'),str(HERE/'render_allocation.cpp'),str(vendor),
                '-o',str(binary)],check=True)
            trace=subprocess.check_output([str(binary)])
            traces.append(trace)
            if args.evidence_dir:
                args.evidence_dir.mkdir(parents=True,exist_ok=True)
                (args.evidence_dir/(test+'-'+mode+'.trace')).write_bytes(trace)
            if mode!='reference':
                for line in trace.decode().splitlines():
                    if line.startswith('PASS '):print(line)
            if test=='engine' and mode=='fastled':
                print(subprocess.check_output([str(binary),'--render-oom']).decode().strip())
        if test in ('engine','status'):
            assert all(trace==traces[0] for trace in traces),test+' original/legacy/FastLED frame, symbol, state, reply, cache or log trace differs'
            print('PASS',test,'exact '+('/'.join(modes))+' trace parity,',len(traces[0]),
                  'bytes, sha256='+hashlib.sha256(traces[0]).hexdigest())
