#!/usr/bin/env python3
"""Concurrent unlinking reader must observe complete payloads only."""
from atomic_control import publish_command
from pathlib import Path
import tempfile
import threading
import time

with tempfile.TemporaryDirectory(prefix='thing-command-') as directory:
    path=Path(directory)/'render.control'
    seen=[]
    done=threading.Event()
    def reader():
        while not done.is_set():
            try:
                data=path.read_text()
                path.unlink()
                seen.append(data)
            except FileNotFoundError:
                pass
            time.sleep(0.00001)
    thread=threading.Thread(target=reader)
    thread.start()
    expected=[]
    try:
        for index in range(1000):
            value=str(index)+':'+('x'*8192)
            expected.append(value+'\n')
            publish_command(path,value)
            deadline=time.monotonic()+2
            while len(seen)<=index:
                assert time.monotonic()<deadline, 'consumer missed command'
                time.sleep(0.00001)
        assert seen==expected
        assert not list(Path(directory).iterdir())
    finally:
        done.set();thread.join()
print('PASS: 1000 complete commands delivered to a concurrent unlinking reader')
