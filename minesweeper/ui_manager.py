import gc
import displayio
import terminalio
from adafruit_display_text import label
from game_engine import UNREVEALED, REVEALED, FLAGGED

# Display dimensions
SCREEN_W = 160
SCREEN_H = 128
STATUS_BAR_H = 16
BORDER = 2

# Colors (RGB565 via palette indices)
BLACK = 0x000000
WHITE = 0xFFFFFF
GRAY = 0x808080
LIGHT_GRAY = 0xC0C0C0
YELLOW = 0xFFFF00
RED = 0xFF0000
GREEN = 0x00FF00
DARK_GREEN = 0x008000
BLUE = 0x0000FF
DARK_BLUE = 0x000080
MAROON = 0x800000
CYAN = 0x008080
ORANGE = 0xFF8800
DARK_GRAY = 0x404040

# Number colors by adjacent mine count (index 0 unused)
NUM_COLORS = [
    BLACK,      # 0 - blank
    BLUE,       # 1
    GREEN,      # 2
    RED,        # 3
    DARK_BLUE,  # 4
    MAROON,     # 5
    CYAN,       # 6
    BLACK,      # 7
    GRAY,       # 8
]

DIFFICULTY_NAMES = ["EASY", "MEDIUM", "HARD"]
DIFFICULTY_KEYS = ["easy", "medium", "hard"]


class UIManager:
    def __init__(self, display):
        self.display = display
        self.display.auto_refresh = False
        self.font = terminalio.FONT
        self._main_group = displayio.Group()
        display.root_group = self._main_group

    def _clear(self):
        while len(self._main_group) > 0:
            self._main_group.pop()

    def refresh(self):
        self.display.refresh()

    def _make_rect(self, x, y, w, h, color):
        bmp = displayio.Bitmap(w, h, 1)
        pal = displayio.Palette(1)
        pal[0] = color
        tg = displayio.TileGrid(bmp, pixel_shader=pal, x=x, y=y)
        return tg

    # ── Start Screen ──────────────────────────────────────────

    def show_start_screen(self, difficulty_idx, blink_on):
        self._clear()
        # Black background
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, SCREEN_H, BLACK))

        # Draw bomb graphic (simple circle)
        self._draw_bomb_graphic(SCREEN_W // 2, 30)

        # Title
        title = label.Label(self.font, text="MINESWEEPER", color=WHITE, scale=2)
        title.anchor_point = (0.5, 0)
        title.anchored_position = (SCREEN_W // 2, 55)
        self._main_group.append(title)

        # Difficulty selector
        diff_text = "< " + DIFFICULTY_NAMES[difficulty_idx] + " >"
        diff_label = label.Label(self.font, text=diff_text, color=YELLOW, scale=1)
        diff_label.anchor_point = (0.5, 0)
        diff_label.anchored_position = (SCREEN_W // 2, 85)
        self._main_group.append(diff_label)

        # Press START (blinking)
        if blink_on:
            start_label = label.Label(self.font, text="PRESS START", color=WHITE, scale=1)
            start_label.anchor_point = (0.5, 0)
            start_label.anchored_position = (SCREEN_W // 2, 108)
            self._main_group.append(start_label)

    def _draw_bomb_graphic(self, cx, cy):
        # Simple bomb: black circle with orange highlight and fuse
        size = 24
        bmp = displayio.Bitmap(size, size, 4)
        pal = displayio.Palette(4)
        pal[0] = BLACK  # transparent/bg
        pal.make_transparent(0)
        pal[1] = GRAY  # bomb body visible against black bg
        pal[2] = ORANGE  # highlight
        pal[3] = RED  # fuse spark

        r = size // 2 - 2
        cxl = size // 2
        cyl = size // 2
        for y in range(size):
            for x in range(size):
                dx = x - cxl
                dy = y - cyl
                dist_sq = dx * dx + dy * dy
                if dist_sq <= r * r:
                    if dx < 0 and dy < 0 and dist_sq <= (r - 3) * (r - 3):
                        bmp[x, y] = 2  # highlight
                    else:
                        bmp[x, y] = 1  # body
        # Fuse line
        for i in range(4):
            fx = cxl + r - 2 + i
            fy = cyl - r + 2 - i
            if 0 <= fx < size and 0 <= fy < size:
                bmp[fx, fy] = 3

        tg = displayio.TileGrid(bmp, pixel_shader=pal,
                                x=cx - size // 2, y=cy - size // 2)
        self._main_group.append(tg)

    # ── Game Screen ───────────────────────────────────────────

    def calc_cell_size(self, rows, cols):
        avail_w = SCREEN_W - 2 * BORDER
        avail_h = SCREEN_H - STATUS_BAR_H - 2 * BORDER
        cw = avail_w // cols
        ch = avail_h // rows
        return min(cw, ch, 12)  # Cap at 12 pixels

    def show_game_screen(self, engine, cursor_r, cursor_c, timer_secs):
        self._clear()
        gc.collect()
        cell_size = self.calc_cell_size(engine.rows, engine.cols)
        grid_w = cell_size * engine.cols
        grid_h = cell_size * engine.rows
        ox = (SCREEN_W - grid_w) // 2
        oy = STATUS_BAR_H + BORDER + (SCREEN_H - STATUS_BAR_H - 2 * BORDER - grid_h) // 2

        # Background
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, SCREEN_H, BLACK))

        # Status bar background
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, STATUS_BAR_H, DARK_GRAY))

        # Timer
        timer_str = "T:{:03d}".format(min(timer_secs, 999))
        timer_lbl = label.Label(self.font, text=timer_str, color=WHITE)
        timer_lbl.anchor_point = (0, 0.5)
        timer_lbl.anchored_position = (4, STATUS_BAR_H // 2)
        self._main_group.append(timer_lbl)

        # Mine counter
        mc_str = "M:{:02d}".format(engine.mine_counter)
        mc_lbl = label.Label(self.font, text=mc_str, color=WHITE)
        mc_lbl.anchor_point = (1.0, 0.5)
        mc_lbl.anchored_position = (SCREEN_W - 4, STATUS_BAR_H // 2)
        self._main_group.append(mc_lbl)

        # Draw grid cells as a single bitmap for efficiency
        bmp_w = grid_w
        bmp_h = grid_h
        # Palette: 0=black, 1=light_gray(unrevealed), 2=gray(revealed),
        #          3=yellow(cursor), 4=red(flag), 5=dark_gray(border),
        #          6-13=number colors, 14=mine, 15=orange
        num_colors = 16
        bmp = displayio.Bitmap(bmp_w, bmp_h, num_colors)
        pal = displayio.Palette(num_colors)
        pal[0] = BLACK
        pal[1] = LIGHT_GRAY
        pal[2] = GRAY
        pal[3] = YELLOW
        pal[4] = RED
        pal[5] = DARK_GRAY
        pal[6] = BLUE       # 1
        pal[7] = GREEN       # 2
        pal[8] = RED         # 3
        pal[9] = DARK_BLUE   # 4
        pal[10] = MAROON     # 5
        pal[11] = CYAN       # 6
        pal[12] = BLACK      # 7
        pal[13] = GRAY       # 8
        pal[14] = BLACK      # mine body
        pal[15] = ORANGE     # mine highlight

        for r in range(engine.rows):
            for c in range(engine.cols):
                cx = c * cell_size
                cy = r * cell_size
                is_cursor = (r == cursor_r and c == cursor_c)
                st = engine.state[r][c]
                val = engine.grid[r][c]

                # Draw cell background
                if is_cursor:
                    # Yellow border for cursor
                    self._fill_rect(bmp, cx, cy, cell_size, cell_size, 3)
                    inner = 2
                    if st == REVEALED:
                        bg = 2
                    elif st == FLAGGED:
                        bg = 1
                    else:
                        bg = 1
                    self._fill_rect(bmp, cx + inner, cy + inner,
                                    cell_size - 2 * inner, cell_size - 2 * inner, bg)
                else:
                    if st == REVEALED:
                        bg = 2
                    else:
                        bg = 1
                    self._fill_rect(bmp, cx, cy, cell_size, cell_size, bg)
                    # Cell border
                    self._draw_rect_border(bmp, cx, cy, cell_size, cell_size, 5)

                # Draw cell content
                if st == FLAGGED:
                    self._draw_flag(bmp, cx, cy, cell_size)
                elif st == REVEALED:
                    if val == -1:
                        self._draw_mine(bmp, cx, cy, cell_size)
                    elif val > 0:
                        self._draw_number(bmp, cx, cy, cell_size, val)

        tg = displayio.TileGrid(bmp, pixel_shader=pal, x=ox, y=oy)
        self._main_group.append(tg)

    def _fill_rect(self, bmp, x, y, w, h, color_idx):
        bw = bmp.width
        bh = bmp.height
        x0 = max(0, x)
        y0 = max(0, y)
        x1 = min(bw, x + w)
        y1 = min(bh, y + h)
        for py in range(y0, y1):
            for px in range(x0, x1):
                bmp[px, py] = color_idx

    def _draw_rect_border(self, bmp, x, y, w, h, color_idx):
        bw = bmp.width
        bh = bmp.height
        for px in range(max(0, x), min(bw, x + w)):
            if 0 <= y < bh:
                bmp[px, y] = color_idx
            if 0 <= y + h - 1 < bh:
                bmp[px, y + h - 1] = color_idx
        for py in range(max(0, y), min(bh, y + h)):
            if 0 <= x < bw:
                bmp[x, py] = color_idx
            if 0 <= x + w - 1 < bw:
                bmp[x + w - 1, py] = color_idx

    def _draw_flag(self, bmp, cx, cy, cs):
        # Simple flag: red triangle + white pole
        mid_x = cx + cs // 2
        mid_y = cy + cs // 2
        # Pole (white = palette 2 light gray)
        pole_x = mid_x
        for dy in range(-cs // 4, cs // 3):
            py = mid_y + dy
            if 0 <= py < bmp.height and 0 <= pole_x < bmp.width:
                bmp[pole_x, py] = 2  # light gray pole
        # Flag triangle (red = palette 4)
        flag_h = max(cs // 3, 3)
        flag_w = max(cs // 3, 3)
        for fy in range(flag_h):
            w = flag_w - (fy * flag_w) // flag_h
            for fx in range(w):
                px = pole_x - fx - 1
                py = mid_y - cs // 4 + fy
                if 0 <= px < bmp.width and 0 <= py < bmp.height:
                    bmp[px, py] = 4

    def _draw_mine(self, bmp, cx, cy, cs):
        # Simple mine: black circle with orange dot
        mid_x = cx + cs // 2
        mid_y = cy + cs // 2
        r = max(cs // 3, 2)
        for dy in range(-r, r + 1):
            for dx in range(-r, r + 1):
                if dx * dx + dy * dy <= r * r:
                    px = mid_x + dx
                    py = mid_y + dy
                    if 0 <= px < bmp.width and 0 <= py < bmp.height:
                        bmp[px, py] = 14  # mine body
        # Orange highlight
        if 0 <= mid_x - 1 < bmp.width and 0 <= mid_y - 1 < bmp.height:
            bmp[mid_x - 1, mid_y - 1] = 15

    def _draw_number(self, bmp, cx, cy, cs, num):
        # Draw number as simple pixel pattern centered in cell
        color_idx = 5 + num  # palette indices 6-13 for numbers 1-8
        mid_x = cx + cs // 2
        mid_y = cy + cs // 2
        # Simplified 3x5 pixel font for digits 1-8
        digits = {
            1: [(0, -2), (0, -1), (0, 0), (0, 1), (0, 2), (-1, -1)],
            2: [(-1, -2), (0, -2), (1, -2), (1, -1), (0, 0), (-1, 0),
                (-1, 1), (0, 2), (1, 2), (-1, 2)],
            3: [(-1, -2), (0, -2), (1, -2), (1, -1), (0, 0), (1, 0),
                (1, 1), (-1, 2), (0, 2), (1, 2)],
            4: [(-1, -2), (-1, -1), (-1, 0), (0, 0), (1, 0), (1, -2),
                (1, -1), (1, 1), (1, 2)],
            5: [(-1, -2), (0, -2), (1, -2), (-1, -1), (-1, 0), (0, 0),
                (1, 0), (1, 1), (-1, 2), (0, 2), (1, 2)],
            6: [(-1, -2), (0, -2), (1, -2), (-1, -1), (-1, 0), (0, 0),
                (1, 0), (-1, 1), (1, 1), (-1, 2), (0, 2), (1, 2)],
            7: [(-1, -2), (0, -2), (1, -2), (1, -1), (0, 0), (0, 1), (0, 2)],
            8: [(-1, -2), (0, -2), (1, -2), (-1, -1), (1, -1), (-1, 0),
                (0, 0), (1, 0), (-1, 1), (1, 1), (-1, 2), (0, 2), (1, 2)],
        }
        if num in digits:
            for dx, dy in digits[num]:
                px = mid_x + dx
                py = mid_y + dy
                if 0 <= px < bmp.width and 0 <= py < bmp.height:
                    bmp[px, py] = color_idx

    # ── Pause Menu ────────────────────────────────────────────

    def show_pause_menu(self, selected_idx):
        self._clear()
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, SCREEN_H, BLACK))

        # Semi-transparent overlay effect (just dark background)
        title = label.Label(self.font, text="PAUSED", color=WHITE, scale=2)
        title.anchor_point = (0.5, 0)
        title.anchored_position = (SCREEN_W // 2, 25)
        self._main_group.append(title)

        options = ["RESUME", "QUIT TO MENU"]
        for i, opt_text in enumerate(options):
            color = YELLOW if i == selected_idx else GRAY
            prefix = "> " if i == selected_idx else "  "
            opt_lbl = label.Label(self.font, text=prefix + opt_text, color=color)
            opt_lbl.anchor_point = (0.5, 0)
            opt_lbl.anchored_position = (SCREEN_W // 2, 65 + i * 20)
            self._main_group.append(opt_lbl)

    # ── Game Over ─────────────────────────────────────────────

    def show_game_over(self):
        # Overlay "GAME OVER" on current screen
        overlay = self._make_rect(20, 40, 120, 48, BLACK)
        self._main_group.append(overlay)
        border = self._make_rect(22, 42, 116, 44, RED)
        self._main_group.append(border)
        inner = self._make_rect(24, 44, 112, 40, BLACK)
        self._main_group.append(inner)

        go_lbl = label.Label(self.font, text="GAME OVER", color=RED, scale=2)
        go_lbl.anchor_point = (0.5, 0.5)
        go_lbl.anchored_position = (SCREEN_W // 2, 64)
        self._main_group.append(go_lbl)

    def show_you_win(self):
        overlay = self._make_rect(20, 40, 120, 48, BLACK)
        self._main_group.append(overlay)
        border = self._make_rect(22, 42, 116, 44, GREEN)
        self._main_group.append(border)
        inner = self._make_rect(24, 44, 112, 40, BLACK)
        self._main_group.append(inner)

        win_lbl = label.Label(self.font, text="YOU WIN!", color=GREEN, scale=2)
        win_lbl.anchor_point = (0.5, 0.5)
        win_lbl.anchored_position = (SCREEN_W // 2, 64)
        self._main_group.append(win_lbl)

    # ── Explosion Animation ───────────────────────────────────

    def show_explosion(self, engine, cursor_r, cursor_c):
        """Simple explosion effect at the mine location."""
        cell_size = self.calc_cell_size(engine.rows, engine.cols)
        grid_w = cell_size * engine.cols
        grid_h = cell_size * engine.rows
        ox = (SCREEN_W - grid_w) // 2
        oy = STATUS_BAR_H + BORDER + (SCREEN_H - STATUS_BAR_H - 2 * BORDER - grid_h) // 2

        cx = ox + cursor_c * cell_size + cell_size // 2
        cy = oy + cursor_r * cell_size + cell_size // 2

        # Draw expanding circles
        sizes = [4, 8, 12, 16]
        colors_list = [ORANGE, RED, YELLOW, ORANGE]
        return cx, cy, sizes, colors_list

    def draw_explosion_frame(self, cx, cy, radius, color):
        bmp = displayio.Bitmap(radius * 2, radius * 2, 2)
        pal = displayio.Palette(2)
        pal[0] = BLACK
        pal.make_transparent(0)
        pal[1] = color
        for dy in range(-radius, radius):
            for dx in range(-radius, radius):
                if dx * dx + dy * dy <= radius * radius:
                    px = radius + dx
                    py = radius + dy
                    if 0 <= px < bmp.width and 0 <= py < bmp.height:
                        bmp[px, py] = 1
        tg = displayio.TileGrid(bmp, pixel_shader=pal,
                                x=cx - radius, y=cy - radius)
        self._main_group.append(tg)
        return tg

    def remove_element(self, elem):
        try:
            self._main_group.remove(elem)
        except ValueError:
            pass

    # ── High Score Entry ──────────────────────────────────────

    def show_high_score_entry(self, time_val, initials, char_pos, current_char):
        self._clear()
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, SCREEN_H, BLACK))

        title = label.Label(self.font, text="NEW HIGH SCORE!", color=YELLOW, scale=1)
        title.anchor_point = (0.5, 0)
        title.anchored_position = (SCREEN_W // 2, 10)
        self._main_group.append(title)

        time_lbl = label.Label(self.font, text="TIME: {:03d}".format(time_val), color=WHITE)
        time_lbl.anchor_point = (0.5, 0)
        time_lbl.anchored_position = (SCREEN_W // 2, 35)
        self._main_group.append(time_lbl)

        name_lbl = label.Label(self.font, text="ENTER NAME:", color=WHITE)
        name_lbl.anchor_point = (0.5, 0)
        name_lbl.anchored_position = (SCREEN_W // 2, 55)
        self._main_group.append(name_lbl)

        # Build display string with cursor
        display_chars = list(initials)
        while len(display_chars) < 3:
            display_chars.append("_")
        if char_pos < 3:
            display_chars[char_pos] = current_char

        char_str = "  ".join(display_chars)
        char_lbl = label.Label(self.font, text=char_str, color=YELLOW, scale=2)
        char_lbl.anchor_point = (0.5, 0)
        char_lbl.anchored_position = (SCREEN_W // 2, 75)
        self._main_group.append(char_lbl)

        hint = label.Label(self.font, text="<> SELECT  A NEXT", color=GRAY)
        hint.anchor_point = (0.5, 0)
        hint.anchored_position = (SCREEN_W // 2, 110)
        self._main_group.append(hint)

    # ── High Score Display ────────────────────────────────────

    def show_high_scores(self, scores, difficulty):
        self._clear()
        self._main_group.append(self._make_rect(0, 0, SCREEN_W, SCREEN_H, BLACK))

        title = label.Label(self.font, text="HIGH SCORES", color=WHITE, scale=1)
        title.anchor_point = (0.5, 0)
        title.anchored_position = (SCREEN_W // 2, 4)
        self._main_group.append(title)

        diff_lbl = label.Label(self.font, text="[" + difficulty.upper() + "]", color=YELLOW)
        diff_lbl.anchor_point = (0.5, 0)
        diff_lbl.anchored_position = (SCREEN_W // 2, 20)
        self._main_group.append(diff_lbl)

        entries = scores[difficulty]
        for i, entry in enumerate(entries[:5]):
            text = "{}. {} .... {:03d}".format(i + 1, entry["initials"], entry["time"])
            color = WHITE if entry["time"] < 999 else GRAY
            row_lbl = label.Label(self.font, text=text, color=color)
            row_lbl.anchor_point = (0.5, 0)
            row_lbl.anchored_position = (SCREEN_W // 2, 38 + i * 16)
            self._main_group.append(row_lbl)
