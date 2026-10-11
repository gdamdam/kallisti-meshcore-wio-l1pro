#!/usr/bin/env python3
"""Run the real GPS provider/parser with hardware and calendar adapters only."""
from pathlib import Path
import subprocess
import tempfile
import sys
root = Path(__file__).resolve().parents[2]
parser = Path(sys.argv[1]) if len(sys.argv) > 1 else root / '.pio/libdeps/WioTrackerL1_companion_radio_ble/MicroNMEA/src'
with tempfile.TemporaryDirectory(prefix='wio-gps-') as directory:
    temp = Path(directory)
    for source, target in [('LocationProvider.h', 'location-under-test.h'), ('MicroNMEALocationProvider.h', 'provider-under-test.h')]:
        text = (root / 'src/helpers/sensors' / source).read_text()
        (temp / target).write_text('\n'.join(line for line in text.splitlines() if not line.startswith('#include')))
    binary = temp / 'gps'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I'+str(temp), '-I'+str(Path(__file__).parent/'gps_stubs'), '-I'+str(parser),
                    str(Path(__file__).parent/'gps_provider.cpp'), str(parser/'MicroNMEA.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
