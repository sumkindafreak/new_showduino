"""Compare actual S3 Lamp renderer traces across both drivers; no hardware claims."""
from pathlib import Path
import subprocess, tempfile, sys, hashlib
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/s3-lamp-node/ShowduinoS3LampNode'
with tempfile.TemporaryDirectory(prefix='showduino-lamp-backends-') as tmp:
    traces=[]
    # Compare both candidates to the unmodified engine preserved in PR #52.
    reference=Path(tmp)/'LampEngine.cpp'
    reference.write_bytes(subprocess.check_output(['git','show',
        '9e9600c9269758ed531e2f98878cf8cc4adb0326:firmware/s3-lamp-node/ShowduinoS3LampNode/src/LampEngine.cpp'],cwd=ROOT))
    for mode,backend in (('reference',0),('legacy',0),('fastled',1)):
        binary=Path(tmp)/('lamp-'+mode+('.exe' if sys.platform=='win32' else ''))
        extra=['-DLAMP_ENGINE_SOURCE="'+reference.as_posix()+'"'] if mode=='reference' else []
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-DSHOWDUINO_LAMP_USE_FASTLED='+str(backend),
                        '-I'+str(HERE/'stubs'),'-I'+str(ROOT/'tools/led-backend-tests/stubs'),
                        '-I'+str(SKETCH),'-I'+str(SKETCH/'src'),*extra,
                        str(HERE/'test_engine_backends.cpp'),'-o',str(binary)],check=True)
        traces.append(subprocess.check_output([str(binary)]))
    assert traces[0]==traces[1]==traces[2], 'Reference/legacy/FastLED frame or state/audio traces differ'
    print('PASS actual LampEngine: identical reference/legacy/FastLED traces,',len(traces[0].splitlines()),
          'samples, sha256='+hashlib.sha256(traces[0]).hexdigest())
