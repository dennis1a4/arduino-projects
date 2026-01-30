import random

# Difficulty configurations
DIFFICULTIES = {
    "easy": {"cols": 8, "rows": 8, "mines": 10},
    "medium": {"cols": 10, "rows": 10, "mines": 20},
    "hard": {"cols": 12, "rows": 12, "mines": 30},
}

# Cell states
UNREVEALED = 0
REVEALED = 1
FLAGGED = 2

class MinesweeperEngine:
    def __init__(self, difficulty="easy"):
        cfg = DIFFICULTIES[difficulty]
        self.cols = cfg["cols"]
        self.rows = cfg["rows"]
        self.total_mines = cfg["mines"]
        self.reset()

    def reset(self):
        # Grid data: -1 = mine, 0-8 = adjacent mine count
        self.grid = [[0] * self.cols for _ in range(self.rows)]
        # Cell state: UNREVEALED, REVEALED, FLAGGED
        self.state = [[UNREVEALED] * self.cols for _ in range(self.rows)]
        self.mines_placed = False
        self.game_over = False
        self.won = False
        self.flags_placed = 0
        self.cells_revealed = 0
        self.total_safe = self.rows * self.cols - self.total_mines

    @property
    def mine_counter(self):
        return self.total_mines - self.flags_placed

    def _neighbors(self, r, c):
        result = []
        for dr in (-1, 0, 1):
            for dc in (-1, 0, 1):
                if dr == 0 and dc == 0:
                    continue
                nr, nc = r + dr, c + dc
                if 0 <= nr < self.rows and 0 <= nc < self.cols:
                    result.append((nr, nc))
        return result

    def _place_mines(self, safe_r, safe_c):
        # Collect safe zone using a 2D grid (avoids set-of-tuples)
        safe = [[False] * self.cols for _ in range(self.rows)]
        safe[safe_r][safe_c] = True
        for nr, nc in self._neighbors(safe_r, safe_c):
            safe[nr][nc] = True

        # All possible positions minus safe zone
        candidates = []
        for r in range(self.rows):
            for c in range(self.cols):
                if not safe[r][c]:
                    candidates.append((r, c))

        # Place mines (no random.sample in CircuitPython, use shuffle)
        count = min(self.total_mines, len(candidates))
        for i in range(len(candidates) - 1, 0, -1):
            j = random.randint(0, i)
            candidates[i], candidates[j] = candidates[j], candidates[i]
        mines = candidates[:count]
        for r, c in mines:
            self.grid[r][c] = -1

        # Calculate numbers
        for r in range(self.rows):
            for c in range(self.cols):
                if self.grid[r][c] == -1:
                    continue
                count = 0
                for nr, nc in self._neighbors(r, c):
                    if self.grid[nr][nc] == -1:
                        count += 1
                self.grid[r][c] = count

        self.mines_placed = True

    def reveal(self, r, c):
        """Reveal a cell. Returns list of (row, col, value) for cells revealed.
        value: -1=mine, 0-8=number. Empty list if no action taken."""
        if self.game_over:
            return []
        if self.state[r][c] != UNREVEALED:
            return []

        # First move - place mines
        if not self.mines_placed:
            self._place_mines(r, c)

        # Hit a mine
        if self.grid[r][c] == -1:
            self.game_over = True
            self.won = False
            self.state[r][c] = REVEALED
            # Return all mine positions for reveal
            result = [(r, c, -1)]
            for mr in range(self.rows):
                for mc in range(self.cols):
                    if self.grid[mr][mc] == -1 and (mr, mc) != (r, c):
                        result.append((mr, mc, -1))
            return result

        # Flood fill for zeros, single reveal for numbers
        revealed = []
        self._flood_reveal(r, c, revealed)

        # Check win
        if self.cells_revealed >= self.total_safe:
            self.game_over = True
            self.won = True

        return revealed

    def _flood_reveal(self, r, c, revealed):
        # Iterative flood fill to avoid stack overflow on CircuitPython
        stack = [(r, c)]
        while stack:
            cr, cc = stack.pop()
            if self.state[cr][cc] != UNREVEALED:
                continue
            if self.grid[cr][cc] == -1:
                continue

            self.state[cr][cc] = REVEALED
            self.cells_revealed += 1
            revealed.append((cr, cc, self.grid[cr][cc]))

            if self.grid[cr][cc] == 0:
                for nr, nc in self._neighbors(cr, cc):
                    if self.state[nr][nc] == UNREVEALED:
                        stack.append((nr, nc))

    def toggle_flag(self, r, c):
        """Toggle flag on a cell. Returns new state or None if no action."""
        if self.game_over:
            return None
        if self.state[r][c] == REVEALED:
            return None
        if self.state[r][c] == FLAGGED:
            self.state[r][c] = UNREVEALED
            self.flags_placed -= 1
            return UNREVEALED
        else:
            self.state[r][c] = FLAGGED
            self.flags_placed += 1
            return FLAGGED

    def is_mine(self, r, c):
        return self.grid[r][c] == -1
