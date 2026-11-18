"""Actual ambient renderer parity and a reproduced Emergency/locator regression."""
from pathlib import Path
import subprocess, tempfile, sys, hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/director-esp32-8048s050/ShowduinoDirector8048S050'
with tempfile.TemporaryDirectory(prefix='showduino-director-ambient-') as tmp:
    reference=Path(tmp)/'DirectorAmbientPixels.cpp'
    reference.write_bytes(subprocess.check_output(['git','show',
        'b103d6f7cdcb2e4ce166fd43d7c6266cbb08e97c:firmware/director-esp32-8048s050/ShowduinoDirector8048S050/DirectorAmbientPixels.cpp'],cwd=ROOT))
    traces=[]
    for mode,backend in (('reference',0),('legacy',0),('fastled',1)):
        binary=Path(tmp)/('ambient-'+mode+('.exe' if sys.platform=='win32' else ''))
        extra=['-DAMBIENT_ENGINE_SOURCE="'+reference.as_posix()+'"'] if mode=='reference' else []
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
            '-DSHOWDUINO_DIRECTOR_AMBIENT_USE_FASTLED='+str(backend),
            '-I'+str(HERE/'stubs'),'-I'+str(ROOT/'tools/led-backend-tests/stubs'),
            '-I'+str(SKETCH),*extra,str(HERE/'test_ambient_backends.cpp'),'-o',str(binary)],check=True)
        traces.append(subprocess.check_output([str(binary)]))
        result=subprocess.run([str(binary),'--emergency-locator'],capture_output=True)
        if mode=='reference':
            assert result.returncode!=0,'Reference no longer reproduces the locator/Emergency bug'
            print('Confirmed reference fails Emergency-white/locator assertion')
        else:
            assert result.returncode==0,result.stderr.decode(errors='replace')
            print('PASS',mode,'Emergency preempts active/new locator at zero requested brightness and disabled cosmetics')
    assert traces[0]==traces[1]==traces[2],'Ordinary renderer/frame/state traces differ'
    print('PASS reference/legacy/FastLED ordinary traces:',len(traces[0].splitlines()),
          'samples, sha256='+hashlib.sha256(traces[0]).hexdigest())
