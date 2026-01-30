import time
import board
import analogio
import keypad
import displayio
from game_engine import MinesweeperEngine, DIFFICULTIES
from ui_manager import UIManager, DIFFICULTY_KEYS, DIFFICULTY_NAMES
from high_scores import load_scores, save_scores, qualifies, insert_score

# ── Hardware Setup ────────────────────────────────────────────

display = board.DISPLAY
display.brightness = 0.5

# Joystick
joy_x = analogio.AnalogIn(board.JOYSTICK_X)
joy_y = analogio.AnalogIn(board.JOYSTICK_Y)

# Buttons using keypad module (more reliable in CP 10.x)
buttons = keypad.ShiftRegisterKeys(
    clock=board.BUTTON_CLOCK,
    data=board.BUTTON_OUT,
    latch=board.BUTTON_LATCH,
    key_count=4,
    value_when_pressed=True,
)

# Button indices for PyGamer shift register
BTN_B = 0
BTN_A = 1
BTN_START = 2
BTN_SELECT = 3

# ── Input Handling ────────────────────────────────────────────

JOY_THRESHOLD = 15000  # Dead zone threshold (center is ~32768)
JOY_CENTER = 32768
DEBOUNCE_MS = 150
MOVE_REPEAT_MS = 120

class InputManager:
    def __init__(self):
        self.last_joy_time = 0
        self.last_btn = {}
        self.btn_events = []

    def update(self):
        """Poll inputs and return (joy_dir, btn_pressed).
        joy_dir: (dx, dy) or None
        btn_pressed: set of button indices pressed this frame
        """
        now = time.monotonic_ns() // 1_000_000  # ms

        # Read joystick
        jx = joy_x.value - JOY_CENTER
        jy = joy_y.value - JOY_CENTER
        joy_dir = None

        if abs(jx) > JOY_THRESHOLD or abs(jy) > JOY_THRESHOLD:
            if now - self.last_joy_time >= MOVE_REPEAT_MS:
                dx = 0
                dy = 0
                if jx < -JOY_THRESHOLD:
                    dx = -1
                elif jx > JOY_THRESHOLD:
                    dx = 1
                if jy < -JOY_THRESHOLD:
                    dy = -1
                elif jy > JOY_THRESHOLD:
                    dy = 1
                joy_dir = (dx, dy)
                self.last_joy_time = now
        else:
            self.last_joy_time = 0  # Reset for immediate response

        # Read buttons
        pressed = set()
        event = keypad.Event()
        while buttons.events.get_into(event):
            if event.pressed:
                btn = event.key_number
                last = self.last_btn.get(btn, 0)
                if now - last >= DEBOUNCE_MS:
                    pressed.add(btn)
                    self.last_btn[btn] = now

        return joy_dir, pressed


# ── Game States ───────────────────────────────────────────────

STATE_START = 0
STATE_PLAYING = 1
STATE_PAUSED = 2
STATE_GAME_OVER = 3
STATE_YOU_WIN = 4
STATE_HIGH_SCORE_ENTRY = 5
STATE_HIGH_SCORE_DISPLAY = 6

# Character set for initials
CHARSET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

# ── Main Game ─────────────────────────────────────────────────

class Game:
    def __init__(self):
        self.ui = UIManager(display)
        self.input = InputManager()
        self.scores = load_scores()
        self.difficulty_idx = 0
        self.state = STATE_START
        self.engine = None
        self.cursor_r = 0
        self.cursor_c = 0
        self.timer_start = 0
        self.timer_secs = 0
        self.timer_running = False
        self.pause_select = 0
        self.blink_on = True
        self.blink_time = 0
        self.idle_time = 0
        self.state_enter_time = 0
        # High score entry
        self.hs_initials = ""
        self.hs_char_pos = 0
        self.hs_char_idx = 0
        self.needs_redraw = True

    def run(self):
        self._refresh_needed = False
        while True:
            joy, btns = self.input.update()
            now = time.monotonic()

            if self.state == STATE_START:
                self._handle_start(joy, btns, now)
            elif self.state == STATE_PLAYING:
                self._handle_playing(joy, btns, now)
            elif self.state == STATE_PAUSED:
                self._handle_paused(joy, btns, now)
            elif self.state == STATE_GAME_OVER:
                self._handle_game_over(joy, btns, now)
            elif self.state == STATE_YOU_WIN:
                self._handle_you_win(joy, btns, now)
            elif self.state == STATE_HIGH_SCORE_ENTRY:
                self._handle_hs_entry(joy, btns, now)
            elif self.state == STATE_HIGH_SCORE_DISPLAY:
                self._handle_hs_display(joy, btns, now)

            if self._refresh_needed:
                self.ui.refresh()
                self._refresh_needed = False

            time.sleep(0.02)  # ~50fps cap

    def _set_state(self, new_state):
        self.state = new_state
        self.state_enter_time = time.monotonic()
        self.needs_redraw = True

    # ── Start Screen ──────────────────────────────────────────

    def _handle_start(self, joy, btns, now):
        # Blink "PRESS START" at 1Hz
        if now - self.blink_time >= 0.5:
            self.blink_on = not self.blink_on
            self.blink_time = now
            self.needs_redraw = True

        if BTN_SELECT in btns:
            self.difficulty_idx = (self.difficulty_idx + 1) % 3
            self.idle_time = now
            self.needs_redraw = True

        if BTN_START in btns:
            self._start_game()
            return

        # Auto-show high scores after 10s idle
        if joy or btns:
            self.idle_time = now

        if self.idle_time and now - self.idle_time >= 10:
            self.idle_time = now  # Reset
            diff_key = DIFFICULTY_KEYS[self.difficulty_idx]
            self.ui.show_high_scores(self.scores, diff_key)
            self._refresh_needed = True
            self._set_state(STATE_HIGH_SCORE_DISPLAY)
            return

        if not self.idle_time:
            self.idle_time = now

        if self.needs_redraw:
            self.ui.show_start_screen(self.difficulty_idx, self.blink_on)
            self.needs_redraw = False
            self._refresh_needed = True

    def _start_game(self):
        diff_key = DIFFICULTY_KEYS[self.difficulty_idx]
        self.engine = MinesweeperEngine(diff_key)
        self.cursor_r = self.engine.rows // 2
        self.cursor_c = self.engine.cols // 2
        self.timer_start = 0
        self.timer_secs = 0
        self.timer_running = False
        self._set_state(STATE_PLAYING)

    # ── Playing ───────────────────────────────────────────────

    def _handle_playing(self, joy, btns, now):
        # Update timer
        if self.timer_running:
            self.timer_secs = int(now - self.timer_start)
            if self.timer_secs >= 999:
                self.timer_secs = 999
                self.engine.game_over = True
                self.engine.won = False
                self._set_state(STATE_GAME_OVER)
                self.ui.show_game_screen(self.engine, self.cursor_r,
                                         self.cursor_c, self.timer_secs)
                self.ui.show_game_over()
                self._refresh_needed = True
                return

        # Cursor movement
        if joy:
            dx, dy = joy
            self.cursor_c = (self.cursor_c + dx) % self.engine.cols
            self.cursor_r = (self.cursor_r + dy) % self.engine.rows
            self.needs_redraw = True

        # Reveal cell
        if BTN_A in btns:
            if not self.timer_running:
                self.timer_start = now
                self.timer_running = True
            result = self.engine.reveal(self.cursor_r, self.cursor_c)
            if result:
                self.needs_redraw = True
                if self.engine.game_over:
                    if self.engine.won:
                        self.timer_secs = int(now - self.timer_start)
                        self.ui.show_game_screen(self.engine, self.cursor_r,
                                                 self.cursor_c, self.timer_secs)
                        self.ui.show_you_win()
                        self._set_state(STATE_YOU_WIN)
                    else:
                        # Show explosion then game over
                        self.ui.show_game_screen(self.engine, self.cursor_r,
                                                 self.cursor_c, self.timer_secs)
                        self._play_explosion()
                        self.ui.show_game_over()
                        self._set_state(STATE_GAME_OVER)
                    self.needs_redraw = False
                    self._refresh_needed = True
                    return

        # Toggle flag
        if BTN_B in btns:
            self.engine.toggle_flag(self.cursor_r, self.cursor_c)
            self.needs_redraw = True

        # Pause
        if BTN_SELECT in btns:
            self.pause_select = 0
            self._set_state(STATE_PAUSED)
            return

        if self.needs_redraw:
            self.ui.show_game_screen(self.engine, self.cursor_r,
                                     self.cursor_c, self.timer_secs)
            self.needs_redraw = False
            self._refresh_needed = True

    def _play_explosion(self):
        """Simple explosion animation."""
        cx, cy, sizes, colors = self.ui.show_explosion(
            self.engine, self.cursor_r, self.cursor_c)
        for i in range(len(sizes)):
            elem = self.ui.draw_explosion_frame(cx, cy, sizes[i], colors[i])
            self.ui.refresh()
            time.sleep(0.15)
            self.ui.remove_element(elem)
        self.ui.refresh()
        time.sleep(0.3)

    # ── Paused ────────────────────────────────────────────────

    def _handle_paused(self, joy, btns, now):
        if joy:
            _, dy = joy
            if dy != 0:
                self.pause_select = (self.pause_select + dy) % 2
                self.needs_redraw = True

        if BTN_A in btns or BTN_START in btns:
            if self.pause_select == 0:
                # Resume
                self._set_state(STATE_PLAYING)
            else:
                # Quit to menu
                self._set_state(STATE_START)
                self.idle_time = now
            return

        if BTN_SELECT in btns:
            # Quick resume
            self._set_state(STATE_PLAYING)
            return

        if self.needs_redraw:
            self.ui.show_pause_menu(self.pause_select)
            self.needs_redraw = False
            self._refresh_needed = True

    # ── Game Over ─────────────────────────────────────────────

    def _handle_game_over(self, joy, btns, now):
        if now - self.state_enter_time >= 2.0:
            self._set_state(STATE_START)
            self.idle_time = now

    # ── You Win ───────────────────────────────────────────────

    def _handle_you_win(self, joy, btns, now):
        if now - self.state_enter_time >= 2.0:
            diff_key = DIFFICULTY_KEYS[self.difficulty_idx]
            if qualifies(self.scores, diff_key, self.timer_secs):
                self.hs_initials = ""
                self.hs_char_pos = 0
                self.hs_char_idx = 0
                self._set_state(STATE_HIGH_SCORE_ENTRY)
            else:
                self.ui.show_high_scores(self.scores, diff_key)
                self._refresh_needed = True
                self._set_state(STATE_HIGH_SCORE_DISPLAY)

    # ── High Score Entry ──────────────────────────────────────

    def _handle_hs_entry(self, joy, btns, now):
        if joy:
            dx, _ = joy
            if dx != 0:
                self.hs_char_idx = (self.hs_char_idx + dx) % len(CHARSET)
                self.needs_redraw = True

        if BTN_A in btns:
            self.hs_initials += CHARSET[self.hs_char_idx]
            self.hs_char_pos += 1
            self.hs_char_idx = 0
            self.needs_redraw = True

            if self.hs_char_pos >= 3:
                # Save and show scores
                diff_key = DIFFICULTY_KEYS[self.difficulty_idx]
                insert_score(self.scores, diff_key,
                             self.hs_initials, self.timer_secs)
                self.ui.show_high_scores(self.scores, diff_key)
                self._refresh_needed = True
                self._set_state(STATE_HIGH_SCORE_DISPLAY)
                return

        if self.needs_redraw:
            current_char = CHARSET[self.hs_char_idx]
            self.ui.show_high_score_entry(
                self.timer_secs, self.hs_initials,
                self.hs_char_pos, current_char)
            self.needs_redraw = False
            self._refresh_needed = True

    # ── High Score Display ────────────────────────────────────

    def _handle_hs_display(self, joy, btns, now):
        if now - self.state_enter_time >= 3.0:
            self._set_state(STATE_START)
            self.idle_time = now

# ── Entry Point ───────────────────────────────────────────────

game = Game()
game.run()
