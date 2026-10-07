import os
import glob
from datetime import datetime, timezone

src_files = glob.glob('src/**/*', recursive=True)
print(f'Total items in src/: {len(src_files)}')
for sf in sorted(src_files):
    if os.path.isfile(sf):
        mtime = os.path.getmtime(sf)
        dt_utc = datetime.fromtimestamp(mtime, tz=timezone.utc)
        print(f'{sf}: mtime UTC = {dt_utc.isoformat()} ({os.path.getsize(sf)} bytes)')
