"""Host adapter contract checks. Stubs do not validate LEDs, timing or radio."""
from pathlib import Path
import subprocess,tempfile,sys
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='showduino-led-backend-') as tmp:
    for backend in (0,1):
        binary=Path(tmp)/('backend-'+str(backend)+('.exe' if sys.platform=='win32' else ''))
        subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
                        '-DSHOWDUINO_LED_USE_FASTLED='+str(backend),'-I'+str(HERE/'stubs'),
                        str(HERE/'test_led_backend.cpp'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
