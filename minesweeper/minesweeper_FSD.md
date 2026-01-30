# Minesweeper for Adafruit PyGamer
## Functional Specification Document

**Version:** 1.0  
**Date:** January 29, 2026  
**Platform:** Adafruit PyGamer  
**Language:** CircuitPython 8.x or higher

---

## 1. Executive Summary

This document specifies the requirements for developing a classic Minesweeper game for the Adafruit PyGamer handheld gaming device. The game will feature three difficulty levels, timer-based scoring, persistent high scores, arcade-style animations, and an intuitive control scheme optimized for the PyGamer's hardware.

---

## 2. Technical Platform

### 2.1 Hardware Specifications
- **Device:** Adafruit PyGamer (ATSAMD51 processor)
- **Display:** 160x128 pixel TFT LCD
- **Controls:** 
  - 8-way joystick
  - A button (right side)
  - B button (right side)
  - Start button (center)
  - Select button (center)
- **Storage:** 8MB Flash (for high score persistence)

### 2.2 Software Platform
**Selected Language:** CircuitPython

**Rationale:**
- Native support for Adafruit hardware
- Built-in display and button libraries
- Simpler sprite and graphics handling
- Better battery efficiency than MicroPython
- Excellent community support for PyGamer

**Required Libraries:**
- `adafruit_display_text`
- `adafruit_imageload`
- `displayio`
- `storage` (for high score persistence)
- `time` (for timer functionality)

---

## 3. Game Modes and Difficulty Levels

### 3.1 Difficulty Configurations

| Difficulty | Grid Size | Mines | Description |
|------------|-----------|-------|-------------|
| **Easy** | 8x8 | 10 | Beginner-friendly gameplay |
| **Medium** | 10x10 | 20 | Moderate challenge |
| **Hard** | 12x12 | 30 | Expert-level difficulty |

### 3.2 Grid Display Specifications
- **Cell Size:** Auto-calculated based on grid size to fit 160x128 display
- **Border:** 2-pixel border around game grid
- **Status Bar:** Top 16 pixels reserved for timer and mine counter
- **Color Scheme:** 
  - Unrevealed cells: Gray (#808080)
  - Revealed cells: Light gray (#C0C0C0)
  - Flagged cells: Red flag graphic
  - Numbers: Color-coded (1=blue, 2=green, 3=red, 4=purple, etc.)
  - Mines: Black bomb graphic with orange highlight

---

## 4. User Interface Specifications

### 4.1 Start Screen

**Layout:**
```
┌────────────────────────┐
│                        │
│    [BOMB GRAPHIC]      │ ← Large centered bomb sprite
│                        │
│    MINESWEEPER         │ ← Title text
│                        │
│   > EASY   <           │ ← Selectable difficulty
│                        │
│  PRESS START           │ ← Instruction text
│                        │
└────────────────────────┘
```

**Elements:**
- **Bomb Graphic:** 32x32 pixel animated bomb sprite (optional pulse animation)
- **Title:** "MINESWEEPER" in retro arcade font
- **Difficulty Selector:** Horizontal menu with arrows indicating selection
- **Instruction:** "PRESS START" blinking at 1Hz

**Controls:**
- **Select Button:** Cycle through difficulty modes (Easy → Medium → Hard → Easy)
- **Start Button:** Begin game with selected difficulty
- **Auto-timeout:** If no input for 10 seconds, display high score screen for 3 seconds, then return to start screen

### 4.2 Game Screen

**Layout:**
```
┌────────────────────────┐
│ ⏱️ 000  💣 10         │ ← Status bar
├────────────────────────┤
│ ░░░░░░░░░░░░          │
│ ░░░░░░░░░░░░          │ ← Game grid
│ ░░▓░░░░░░░░░          │ ← (▓ = cursor)
│ ░░░░░░░░░░░░          │
│ ░░░░░░░░░░░░          │
│ ░░░░░░░░░░░░          │
└────────────────────────┘
```

**Status Bar Elements:**
- **Timer:** Format "⏱️ XXX" (seconds elapsed, 000-999)
- **Mine Counter:** Format "💣 XX" (remaining unflagged mines)

**Game Grid:**
- Visual cursor highlight (inverted colors or border)
- Revealed cells show numbers (0-8) or blank if zero
- Flagged cells show red flag sprite
- Unrevealed cells show textured pattern

**Controls:**
- **Joystick:** Move cursor (8-directional, wraps at edges)
- **A Button:** Reveal cell at cursor position
- **B Button:** Toggle flag at cursor position
- **Select Button:** Pause game and show pause menu

### 4.3 Pause Menu

**Layout:**
```
┌────────────────────────┐
│                        │
│       PAUSED           │
│                        │
│   > RESUME             │
│     QUIT TO MENU       │
│                        │
└────────────────────────┘
```

**Controls:**
- **Joystick Up/Down:** Navigate menu options
- **A Button / Start Button:** Confirm selection
- **Select Button:** Quick resume

**Timer Behavior:** Timer pauses while menu is displayed

### 4.4 High Score Entry Screen

**Triggered When:** Player completes game with a time better than the worst stored high score (or if fewer than 5 scores exist for that difficulty)

**Layout:**
```
┌────────────────────────┐
│    NEW HIGH SCORE!     │
│                        │
│    TIME: 045           │
│                        │
│    ENTER NAME:         │
│       A▓_              │ ← (▓ = current letter)
│                        │
│   ◄ ► SELECT  A NEXT   │
└────────────────────────┘
```

**Controls:**
- **Joystick Left/Right:** Change current letter (A-Z, 0-9)
- **A Button:** Confirm letter and move to next position
- **After 3rd letter:** Auto-save and show high score screen

**Character Set:** A-Z, 0-9 (36 characters total)

### 4.5 High Score Display Screen

**Layout:**
```
┌────────────────────────┐
│    HIGH SCORES         │
│      [EASY]            │ ← Current difficulty
│                        │
│  1. ACE ..... 032      │
│  2. BOB ..... 045      │
│  3. ZOE ..... 058      │
│  4. MAX ..... 071      │
│  5. SAM ..... 089      │
│                        │
└────────────────────────┘
```

**Display Duration:** 3 seconds, then return to start screen

**Data Shown:**
- Top 5 scores for current difficulty
- Rank, initials, and time in seconds
- Difficulty level indicated at top

---

## 5. Game Logic Specifications

### 5.1 Game Initialization

**Mine Placement Algorithm:**
1. Wait for first cell reveal
2. Ensure first revealed cell and all adjacent cells are mine-free
3. Randomly place mines in remaining cells
4. Calculate numbers for all non-mine cells (count of adjacent mines)

**Reason:** Prevents instant loss on first move (arcade fairness)

### 5.2 Game States

```
START_SCREEN → PLAYING → (WIN or LOSE) → HIGH_SCORE_ENTRY? → HIGH_SCORE_DISPLAY → START_SCREEN
                  ↓
              PAUSED (reversible)
```

**State Transitions:**
- **START_SCREEN → PLAYING:** Press Start button
- **PLAYING → PAUSED:** Press Select button
- **PAUSED → PLAYING:** Select "Resume" or press Select
- **PAUSED → START_SCREEN:** Select "Quit to Menu"
- **PLAYING → LOSE:** Reveal a mine
- **PLAYING → WIN:** Reveal all non-mine cells
- **WIN/LOSE → HIGH_SCORE_ENTRY:** Win with qualifying time
- **WIN/LOSE → HIGH_SCORE_DISPLAY:** Win/lose without qualifying time
- **HIGH_SCORE_DISPLAY → START_SCREEN:** After 3 seconds

### 5.3 Win Condition

**Player wins when:**
- All non-mine cells are revealed
- All mines may be flagged (optional, not required)

**Actions on Win:**
1. Stop timer
2. Play victory sound/animation (optional)
3. Check if time qualifies for high score
4. Proceed to high score entry or display

### 5.4 Lose Condition

**Player loses when:**
- Any mine is revealed (A button pressed on mine cell)

**Actions on Lose:**
1. Stop timer
2. Trigger explosion animation at revealed mine
3. Reveal all mines on board
4. Display "GAME OVER" overlay for 2 seconds
5. Return to start screen (no high score check)

### 5.5 Cell Reveal Logic

**When A button pressed on unrevealed cell:**
1. **If flagged:** Do nothing (must unflag first)
2. **If mine:** Trigger lose condition
3. **If number (1-8):** Reveal only that cell
4. **If zero (no adjacent mines):** 
   - Reveal cell
   - Recursively reveal all adjacent unrevealed cells
   - Stop recursion when numbered cells are reached
   - This creates the "flood fill" effect

**First Move Special Case:**
- If this is the first reveal, generate mine layout ensuring this cell and adjacent cells are safe

### 5.6 Flag Toggle Logic

**When B button pressed:**
1. **If revealed:** Do nothing
2. **If unrevealed and unflagged:**
   - Place flag
   - Decrement mine counter
   - Prevent reveal with A button
3. **If unrevealed and flagged:**
   - Remove flag
   - Increment mine counter
   - Allow reveal with A button

**Mine Counter:**
- Starts at total mine count
- Decreases when flag placed
- Increases when flag removed
- Can go negative (allows over-flagging)

---

## 6. Animation Specifications

### 6.1 Explosion Animation (On Mine Reveal)

**Duration:** 1.5 seconds total

**Frame Sequence:**
1. **Frame 1 (0.0s):** Small orange circle (8x8 pixels)
2. **Frame 2 (0.1s):** Medium orange/yellow burst (16x16 pixels)
3. **Frame 3 (0.2s):** Large orange/red/yellow explosion (24x24 pixels)
4. **Frame 4 (0.4s):** Full explosion with debris particles (32x32 pixels)
5. **Frame 5-10 (0.6s-1.5s):** Fade out with screen shake effect

**Visual Effects:**
- Screen shake: ±2 pixels horizontal/vertical offset
- Color palette: Orange (#FF8800), Red (#FF0000), Yellow (#FFFF00), Black smoke
- Particle sprites: Small 2x2 pixel squares radiating outward

**Audio (Optional):**
- 8-bit explosion sound effect if speaker enabled

### 6.2 Victory Animation (Optional)

**Duration:** 1.0 second

**Effect:**
- All revealed cells flash green
- Simple confetti particles fall from top of screen
- "YOU WIN!" text appears center screen

### 6.3 Start Screen Idle Animation

**Bomb Pulse Animation:**
- Bomb sprite scales: 100% → 110% → 100% (1 second cycle)
- Subtle glow effect around bomb

**"PRESS START" Blink:**
- Text visible for 0.5s, hidden for 0.5s, repeat

---

## 7. Scoring and High Score System

### 7.1 Scoring Mechanism

**Score = Time to Complete (in seconds)**
- Lower score is better
- Timer starts on first move
- Timer stops on win condition
- Maximum time: 999 seconds

### 7.2 High Score Storage

**Data Structure:**
```python
high_scores = {
    "easy": [
        {"initials": "ACE", "time": 32},
        {"initials": "BOB", "time": 45},
        {"initials": "ZOE", "time": 58},
        {"initials": "MAX", "time": 71},
        {"initials": "SAM", "time": 89}
    ],
    "medium": [...],
    "hard": [...]
}
```

**Storage Method:**
- Saved to `/high_scores.json` in PyGamer flash storage
- Read on game start
- Written when new high score achieved
- Default scores if file doesn't exist: [999, 999, 999, 999, 999]

**High Score Qualification:**
- Top 5 times for each difficulty
- Ties broken by whoever achieved it first (existing record wins)

### 7.3 High Score Entry

**Process:**
1. Player completes game
2. Check if time is better than 5th place (or empty slots exist)
3. If yes: Enter initials screen
4. If no: Show high score display, return to start

**Entry Interface:**
- 3 character slots (A-Z, 0-9)
- Default starts at 'A'
- Joystick left/right cycles through characters
- A button confirms current character and advances
- After 3rd character: Auto-save and show high scores

---

## 8. Audio Specifications (Optional)

**Note:** PyGamer has built-in speaker. Audio is optional but enhances arcade feel.

**Sound Effects:**
- **Cell Reveal:** Soft beep (100ms)
- **Flag Place/Remove:** Click sound (50ms)
- **Mine Explosion:** 8-bit explosion (500ms)
- **Victory:** Ascending chime (1s)
- **Menu Navigation:** Soft tick (30ms)

**Implementation:**
- Use `audiocore` and `audioio` CircuitPython libraries
- Pre-generated WAV files stored in flash
- Keep file sizes minimal (8-bit, 22kHz)

---

## 9. Controls Summary

### 9.1 Start Screen
| Input | Action |
|-------|--------|
| Select | Cycle difficulty (Easy/Medium/Hard) |
| Start | Begin game |
| (10s idle) | Show high scores for 3s |

### 9.2 Gameplay
| Input | Action |
|-------|--------|
| Joystick | Move cursor (8-directional, wraps edges) |
| A Button | Reveal cell at cursor |
| B Button | Toggle flag at cursor |
| Select | Pause menu |

### 9.3 Pause Menu
| Input | Action |
|-------|--------|
| Joystick Up/Down | Navigate options |
| A / Start | Confirm selection |
| Select | Quick resume |

### 9.4 High Score Entry
| Input | Action |
|-------|--------|
| Joystick Left/Right | Change current letter |
| A Button | Confirm letter, move to next |

---

## 10. Visual Design Guidelines

### 10.1 Color Palette (Arcade Style)

**Primary Colors:**
- Background: #000000 (Black)
- UI Frame: #FFFFFF (White)
- Unrevealed Cell: #808080 (Gray)
- Revealed Cell: #C0C0C0 (Light Gray)
- Cursor Highlight: #FFFF00 (Yellow)

**Number Colors:**
- 1 mine: #0000FF (Blue)
- 2 mines: #00FF00 (Green)
- 3 mines: #FF0000 (Red)
- 4 mines: #000080 (Dark Blue)
- 5 mines: #800000 (Maroon)
- 6 mines: #008080 (Cyan)
- 7 mines: #000000 (Black)
- 8 mines: #808080 (Gray)

**Special Colors:**
- Flag: #FF0000 (Red) with white pole
- Mine: #000000 (Black) with #FF8800 (Orange) highlight
- Explosion: #FF8800, #FF0000, #FFFF00 gradient

### 10.2 Font Specifications

**Title Font:** 
- Style: Bold, blocky arcade font
- Size: 12pt
- Use: Title screen, "MINESWEEPER" text

**UI Font:**
- Style: Monospace, retro
- Size: 8pt
- Use: Menu items, scores, timer

**Number Font:**
- Style: Bold, sans-serif
- Size: Scales with cell size
- Use: Mine count numbers in cells

### 10.3 Sprite Assets Required

| Asset | Size | Description |
|-------|------|-------------|
| Bomb (large) | 32x32 | Start screen animation |
| Bomb (small) | Cell size | Game grid mine |
| Flag | Cell size | Flagged cell marker |
| Explosion frames | 8-32px | 5 frame animation |
| Cursor | Cell size | Yellow border/highlight |

---

## 11. Performance Requirements

### 11.1 Responsiveness
- Input lag: <50ms from button press to visual response
- Cursor movement: Smooth, immediate (no acceleration)
- Animation frame rate: Minimum 20 FPS for explosions
- Menu transitions: <100ms

### 11.2 Memory Management
- Maximum RAM usage: <200KB (PyGamer has 192KB RAM)
- Optimize sprite storage with shared palettes
- Unload unused assets when switching screens
- Efficient grid data structure (bit packing for mines)

### 11.3 Battery Efficiency
- Display brightness: Configurable (default 50%)
- Sleep mode: Dim screen after 30s of inactivity
- Audio: Optional, can disable to save power

---

## 12. Data Persistence

### 12.1 High Score File

**File:** `/high_scores.json`

**Format:**
```json
{
  "easy": [
    {"initials": "ACE", "time": 32},
    {"initials": "BOB", "time": 45},
    {"initials": "ZOE", "time": 58},
    {"initials": "MAX", "time": 71},
    {"initials": "SAM", "time": 89}
  ],
  "medium": [
    {"initials": "JOE", "time": 95},
    {"initials": "ANN", "time": 112},
    {"initials": "LEO", "time": 135},
    {"initials": "KIM", "time": 158},
    {"initials": "TOM", "time": 189}
  ],
  "hard": [
    {"initials": "PRO", "time": 245},
    {"initials": "GOD", "time": 289},
    {"initials": "WIZ", "time": 312},
    {"initials": "GEN", "time": 356},
    {"initials": "ACE", "time": 401}
  ]
}
```

**Error Handling:**
- If file corrupted: Generate default scores
- If file missing: Create with default scores
- Validate JSON structure on load
- Backup previous scores before writing

---

## 13. Edge Cases and Error Handling

### 13.1 Grid Edge Cases
- **Cursor wrapping:** Joystick at edge wraps to opposite side
- **Corner cells:** Properly handle 3-neighbor corners vs 5-neighbor edges vs 8-neighbor center
- **Single cell left:** Can still win if last cell is not a mine

### 13.2 User Input Edge Cases
- **Button mashing:** Debounce all inputs (50ms minimum between actions)
- **Simultaneous buttons:** Priority order: Start > Select > A > B
- **Joystick drift:** Dead zone in center (±10% of range)

### 13.3 Storage Edge Cases
- **Full flash:** Handle write failure gracefully, notify user
- **Corrupted save:** Revert to defaults, notify user once
- **First run:** Create high score file with default values

### 13.4 Game Logic Edge Cases
- **All flags used incorrectly:** Allow continued play, mine counter goes negative
- **Timer overflow:** Cap at 999 seconds, auto-lose
- **No valid moves:** Impossible in standard Minesweeper, but check on grid generation

---

## 14. Testing Requirements

### 14.1 Functional Testing

**Start Screen:**
- [ ] Difficulty cycles correctly with Select
- [ ] Start button launches game with selected difficulty
- [ ] 10s timeout shows high scores
- [ ] Bomb animation plays smoothly

**Gameplay:**
- [ ] Cursor movement in all 8 directions
- [ ] Cursor wraps at grid edges
- [ ] A button reveals cells correctly
- [ ] B button toggles flags correctly
- [ ] Cannot reveal flagged cells
- [ ] Zero-cell flood fill works correctly
- [ ] Mine counter updates with flags
- [ ] Timer starts on first move
- [ ] Timer counts accurately

**Win/Lose:**
- [ ] Win condition triggers when all non-mines revealed
- [ ] Lose condition triggers when mine revealed
- [ ] Explosion animation plays on loss
- [ ] All mines revealed on loss
- [ ] High score check works correctly
- [ ] High score entry accepts all characters
- [ ] High score saves persist across power cycles

### 14.2 Performance Testing
- [ ] Frame rate maintains 20+ FPS during explosions
- [ ] No input lag during gameplay
- [ ] High score file read/write <100ms
- [ ] Game loads in <2 seconds

### 14.3 Usability Testing
- [ ] Controls feel natural and responsive
- [ ] Visual cursor is easily visible
- [ ] Numbers are readable at all grid sizes
- [ ] Arcade aesthetic achieved
- [ ] High score entry is intuitive

---

## 15. Development Phases

### Phase 1: Core Engine (Week 1)
- Set up CircuitPython environment
- Implement grid data structure
- Mine placement algorithm
- Cell reveal logic (including flood fill)
- Basic display rendering

### Phase 2: UI Framework (Week 1-2)
- Start screen layout
- Game screen layout
- Status bar (timer, mine counter)
- Cursor movement and highlighting
- Button input handling

### Phase 3: Game Logic (Week 2)
- Win/lose detection
- Flag toggle system
- Pause menu
- Screen transitions

### Phase 4: Scoring System (Week 2-3)
- Timer implementation
- High score storage/retrieval
- High score entry interface
- High score display screen

### Phase 5: Visual Polish (Week 3)
- Explosion animation
- Start screen animations
- Victory effects
- Color scheme refinement
- Sprite creation

### Phase 6: Testing & Optimization (Week 3-4)
- Performance optimization
- Bug fixes
- Edge case handling
- User testing
- Final polish

---

## 16. Future Enhancements (Post-V1.0)

**Potential additions for future versions:**
- Custom difficulty mode (user selects grid size and mine count)
- Sound effects toggle in settings
- Display brightness adjustment
- Game statistics (total wins, losses, play time)
- "?" mark mode (B button cycles: empty → flag → ? → empty)
- Undo last move feature
- Hint system (reveal one safe cell)
- Speedrun mode (preset seed for competitive play)
- Multiplayer (pass-and-play)
- Daily challenge mode

---

## 17. Success Criteria

The Minesweeper game will be considered successful when:

1. ✅ All three difficulty modes are fully playable
2. ✅ Timer accurately tracks and displays game time
3. ✅ High scores persist across power cycles
4. ✅ Explosion animation plays smoothly on mine reveal
5. ✅ Controls are responsive and intuitive
6. ✅ Game feels like classic arcade Minesweeper
7. ✅ No critical bugs in core gameplay
8. ✅ High score entry is user-friendly
9. ✅ Performance maintains >20 FPS during all animations
10. ✅ User can play continuously without crashes

---

## 18. Appendix

### A. PyGamer Button Pin Mappings
```python
import board
from digitalio import DigitalInOut, Pull

button_a = DigitalInOut(board.BUTTON_A)
button_b = DigitalInOut(board.BUTTON_B)
button_start = DigitalInOut(board.BUTTON_START)
button_select = DigitalInOut(board.BUTTON_SELECT)

# Joystick uses analog input
joystick_x = analogio.AnalogIn(board.JOYSTICK_X)
joystick_y = analogio.AnalogIn(board.JOYSTICK_Y)
```

### B. Display Initialization
```python
import board
import displayio

display = board.DISPLAY
display.brightness = 0.5  # 50% brightness for battery savings
```

### C. Recommended Project Structure
```
/
├── code.py              # Main game loop
├── game_engine.py       # Core Minesweeper logic
├── ui_manager.py        # Screen rendering
├── high_scores.py       # Score persistence
├── sprites/             # Sprite assets
│   ├── bomb_large.bmp
│   ├── bomb_small.bmp
│   ├── flag.bmp
│   └── explosion/
│       ├── frame1.bmp
│       ├── frame2.bmp
│       └── ...
├── fonts/
│   ├── arcade.bdf
│   └── numbers.bdf
└── sounds/              # Optional
    ├── beep.wav
    └── explosion.wav
```

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-01-29 | Specification Team | Initial release |

---

**End of Functional Specification Document**
