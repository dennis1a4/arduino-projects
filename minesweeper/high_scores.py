import json
import os

SCORES_FILE = "/high_scores.json"
MAX_SCORES = 5
DEFAULT_TIME = 999

def _default_scores():
    return {
        "easy": [{"initials": "---", "time": DEFAULT_TIME} for _ in range(MAX_SCORES)],
        "medium": [{"initials": "---", "time": DEFAULT_TIME} for _ in range(MAX_SCORES)],
        "hard": [{"initials": "---", "time": DEFAULT_TIME} for _ in range(MAX_SCORES)],
    }

def load_scores():
    try:
        with open(SCORES_FILE, "r") as f:
            data = json.load(f)
        # Validate structure
        for key in ("easy", "medium", "hard"):
            if key not in data or not isinstance(data[key], list):
                return _default_scores()
            for entry in data[key]:
                if "initials" not in entry or "time" not in entry:
                    return _default_scores()
        return data
    except (OSError, ValueError, KeyError):
        return _default_scores()

def save_scores(scores):
    try:
        with open(SCORES_FILE, "w") as f:
            json.dump(scores, f)
        os.sync()  # Flush to flash so it survives power-off
    except OSError:
        pass  # Flash full or write-protected

def qualifies(scores, difficulty, time_val):
    """Check if time qualifies for high score list."""
    entries = scores[difficulty]
    if len(entries) < MAX_SCORES:
        return True
    return time_val < entries[-1]["time"]

def insert_score(scores, difficulty, initials, time_val):
    """Insert a new score into the list, maintaining sorted order."""
    entries = scores[difficulty]
    new_entry = {"initials": initials, "time": time_val}
    # Find insertion point (stable - existing scores win ties)
    pos = len(entries)
    for i, entry in enumerate(entries):
        if time_val < entry["time"]:
            pos = i
            break
    entries.insert(pos, new_entry)
    # Keep only top MAX_SCORES
    scores[difficulty] = entries[:MAX_SCORES]
    save_scores(scores)
