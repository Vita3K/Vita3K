from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="vita3k-avplayer-") as folder:
    fixture = Path(folder) / "24fps-90khz.mp4"
    subprocess.run([sys.argv[2], "-v", "error", "-nostdin", "-f", "lavfi", "-i",
                    "testsrc2=size=64x48:rate=24", "-f", "lavfi", "-i",
                    "sine=frequency=440:sample_rate=48000", "-t", "1", "-c:v", "libx264",
                    "-bf", "2", "-g", "24", "-threads", "1", "-video_track_timescale",
                    "90000", "-c:a", "aac", "-ac", "2", str(fixture)], check=True, timeout=120)
    for mode in ("timestamps", "drain", "loop", "delayed"):
        subprocess.run([sys.argv[1], mode, str(fixture)], check=True, timeout=10)
