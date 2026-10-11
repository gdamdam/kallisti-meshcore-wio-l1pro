#!/usr/bin/env python3
"""Exercise actual Wio adapter against the installed firmware's LittleFS.
Usage: run_qspi_tests.py /path/to/framework-arduinoadafruitnrf52
"""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2]
lfs=Path(sys.argv[1])/'libraries/Adafruit_LittleFS/src/littlefs'
with tempfile.TemporaryDirectory(prefix='wio-qspi-') as d:
 temp=Path(d)
 source=(root/'examples/companion_radio/ui-new/WioInboxStorage.h').read_text()
 (temp/'wio-storage-under-test.h').write_text('class WioInboxStorage'+source.split('class WioInboxStorage',1)[1])
 flags=['-fsanitize=address,undefined','-g','-I'+str(lfs)]
 objects=[]
 for name in ('lfs','lfs_util'):
  obj=temp/(name+'.o');subprocess.run(['cc',*flags,'-c',str(lfs/(name+'.c')),'-o',str(obj)],check=True);objects.append(str(obj))
 binary=temp/'qspi'
 subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror',*flags,'-I'+str(temp),str(Path(__file__).parent/'qspi_persistence.cpp'),*objects,'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
