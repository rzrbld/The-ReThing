"""Publish one complete command for a consumer that unlinks after reading."""
import os
from pathlib import Path
import tempfile


def publish_command(path, command):
    path = Path(path)
    fd, pending = tempfile.mkstemp(prefix='.' + path.name + '-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(command + '\n')
        os.replace(pending, path)
    finally:
        try:
            os.unlink(pending)
        except FileNotFoundError:
            pass
