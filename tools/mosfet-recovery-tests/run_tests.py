from pathlib import Path
import subprocess,tempfile,sys,argparse
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SKETCH=ROOT/'firmware/mosfet-node-esp32/ShowduinoMosfetNode'
P4=ROOT/'firmware/stage-engine-p4/ShowduinoStageEngineP4'
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');args=p.parse_args()
with tempfile.TemporaryDirectory(prefix='showduino-mosfet-recovery-') as tmp:
 for test in ('recovery','p4_emergency_grant'):
  modes=('reference','candidate') if test=='recovery' else ('candidate',)
  for mode in modes:
   extra=[]
   if mode=='reference':
    for filename,macro in (('MosfetOutputEngine.cpp','MOSFET_OUTPUT_SOURCE'),('MosfetNodeState.cpp','MOSFET_STATE_SOURCE')):
     ref=Path(tmp)/filename
     ref.write_bytes(subprocess.check_output(['git','show','39bfbd823a4f61976a65db3da09c7bd14ad164b9:firmware/mosfet-node-esp32/ShowduinoMosfetNode/src/'+filename],cwd=ROOT))
     extra+=['-D'+macro+'="'+ref.as_posix()+'"']
   binary=Path(tmp)/(test+'-'+mode+('.exe' if sys.platform=='win32' else ''))
   sources=[str(HERE/('test_'+test+'.cpp'))]
   if test=='recovery':sources+=[str(SKETCH/'src/MosfetRecoveryStore.cpp')]
   subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',
    *(['-pthread'] if sys.platform!='win32' else []),
    *(['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g'] if args.sanitize else []),
    '-I'+str(HERE/'stubs'),'-I'+str(SKETCH),'-I'+str(SKETCH/'src'),'-I'+str(P4),'-I'+str(P4/'src/nodes'),*extra,*sources,'-o',str(binary)],check=True)
   if mode=='reference':
    result=subprocess.run([str(binary),'--hold-only'],capture_output=True)
    assert result.returncode!=0 and b'level(0)==37' in result.stderr,'Original no longer reproduces the expected held-level assertion'
    print('Confirmed original MOSFET drops held level on authority loss')
   else:subprocess.run([str(binary)],check=True)
