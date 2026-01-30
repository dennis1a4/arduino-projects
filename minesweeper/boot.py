import storage

# Make the filesystem writable by CircuitPython code
# so high scores can be saved to /high_scores.json
storage.remount("/", readonly=False)
