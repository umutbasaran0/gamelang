# 🎲 GameLang

**A domain-specific language for designing board games — no programming experience required.**

GameLang lets tabletop game designers describe their game in a simple, readable script and instantly play it in a browser. Define your board tiles, players, card decks, turn phases, and event rules in plain text. The engine parses, type-checks, and interprets your code, then the web player renders a live, interactive board.

---

## Who is this for?

Board game designers who want to **prototype and test rules quickly** without learning a general-purpose programming language. If you can write a recipe or a rulebook, you can write a GameLang script.

---

## Features

- **Readable syntax** — designed to look like a rulebook, not code
- **Monopoly-style board** — tiles rendered around a perimeter, player tokens move in real time
- **Card decks** — define chance / community chest cards with effects
- **Turn phases** — multi-step turns with roll, move, combat, and loot phases
- **Cross-phase variables** — values set in phase 1 are visible in phase 5 and in event handlers
- **Built-in player state** — `player_hp`, `player_money`, `player_pos`, `player_id` auto-injected each turn
- **Events** — `on land` and `on pass` hooks that fire automatically
- **Variables** — assign and reuse values inside events
- **`if` / `else if` / `else`** — full branching chains, any depth
- **`while` + `break`** — loops with clean exit, used for combat simulations
- **Logical operators** — `and`, `or`, `not`
- **Types** — integers, floats, strings, booleans
- **Error reporting** — clear messages with line numbers
- **Web player** — open `web/game_player.html` in any browser, paste your code, hit Play

---

## Repository Layout

```
gamelang/
├── src/                    C source — lexer, parser, AST, interpreter
│   ├── lexer.c / lexer.h
│   ├── ast.c   / ast.h
│   ├── parser.c / parser.h
│   ├── typecheck.c / typecheck.h
│   ├── interpreter.c / interpreter.h
│   ├── environment.c / environment.h
│   └── main.c
├── examples/
│   ├── simple_starter.game     2-player beginner game
│   ├── monopoly.game           Full Monopoly-style game
│   ├── dungeon_crawler.game    HP-focused combat crawler
│   ├── kingdom_of_arath.game   Full showcase — every feature at maximum depth
|   ├── 01_minimal.game
|   └── 15_full_game.game
├── web/
│   └── game_player.html    Self-contained browser game engine
├── Makefile
└── README.md
```

---

## Build

Requires **GCC** (or Clang — change `CC` in the Makefile). No external libraries; C99 standard library only.

```bash
make
```

This produces the `gamelang` binary in the root directory.

```bash
make clean   # remove object files and binary
make test    # run all 20 tests
```

---

## Run

```bash
# Parse, type-check, and interpret a game script
./gamelang examples/simple_starter.game

# Print every token the lexer produced
./gamelang examples/monopoly.game --dump-tokens

# Print the full Abstract Syntax Tree
./gamelang examples/monopoly.game --dump-ast

# Export a JSON snapshot for the web player
./gamelang examples/monopoly.game --export-web
```

---

## Quick Start — Your First Game

Create a file called `mygame.game`:

```
players {
    count: 2,
    start_money: 500,
    start_hp: 100
}

arena Board {
    size: 12
}

tile Start {
    type: "start",
    bonus: 100,
    color: "green"
}

tile GoldMine {
    type: "property",
    price: 150,
    rent: 30,
    color: "yellow"
}

tile Swamp {
    type: "normal",
    color: "gray"
}

deck Events {
    card Windfall {
        effect: "transfer",
        amount: 80,
        text: "Lucky windfall! Gain 80 gold."
    }
    card StormDamage {
        effect: "pay",
        amount: 60,
        text: "Storm damages your property. Pay 60."
    }
}

turn_structure {
    phase move {
        roll(1, 6)
        advance(result)
    }
    phase effects {
        if (result > 4) {
            transfer(50)
        } else if (result > 2) {
            heal(10)
        } else {
            pay(30)
        }
    }
}

on land(player) {
    if (player_hp < 50) {
        heal(30)
        print("Low HP — recovered 30.")
    } else if (player_money < 200) {
        transfer(60)
        print("Low funds — emergency aid: +60 gold.")
    } else {
        pay(20)
    }
}

on pass(player) {
    transfer(100)
    print("Lapped the board! Collect 100 gold.")
}
```

Run it:

```
$ ./gamelang mygame.game
```

Expected output:

```
=== SEMANTIC ANALYSIS ===
OK: no type errors.

=== INTERPRETER ===

>>> Turn structure (2 phases)
  >> Phase: move
[GAME ENGINE] roll(1 d6) = 4
[GAME ENGINE] advance: +4 spaces
  >> Phase: effects
[GAME ENGINE] heal: +10 HP
  -- Player state: HP=110  Gold=500
<<< turn done

>>> Event: on land
    param player = 100
    -> Executing...
[GAME ENGINE] pay: -20 gold
<<< done

>>> Event: on pass
    param player = 100
    -> Executing...
[GAME ENGINE] transfer: +100 gold
Lapped the board! Collect 100 gold.
<<< done

=== EXECUTION COMPLETE ===

Parse+Typecheck+Eval OK — 8 decl(s) in 'mygame.game'
```

Then open `web/game_player.html` in your browser, paste the code, and click **▶ Load Game**.

---

## Language Reference

### Players

```
players {
    count: 4,
    start_money: 1500,
    start_hp: 100
}
```

| Property | Type | Description |
|---|---|---|
| `count` | int | Number of players (2–4) |
| `start_money` | int | Starting gold for each player |
| `start_hp` | int | Starting HP for each player |

---

### Arena (the Board)

```
arena BoardName {
    size: 40,
    name: "Classic Board"
}
```

`size` determines how many tiles the board has. Tiles loop; `advance` wraps around automatically.

---

### Tiles

```
tile TileName {
    type: "property",
    price: 300,
    rent: 50,
    color: "blue"
}
```

| Property | Values | Description |
|---|---|---|
| `type` | `"start"` `"jail"` `"chance"` `"property"` `"normal"` | Visual style in web player |
| `color` | `"green"` `"red"` `"blue"` `"yellow"` `"orange"` `"gray"` or hex `"#ff6b6b"` | Tile color bar |
| `price` | int | Displayed on property tiles |
| `rent` | int | Displayed on property tiles |
| `bonus` | int | Displayed on start tiles |

Tiles cycle — if you define 10 tiles on a 40-tile board, the pattern repeats.

---

### Resources

```
resource Bank {
    amount: 20000,
    shared: true
}
```

Named value pools; visible in the web player sidebar.

---

### Card Decks

```
deck ChanceCards {
    card BankBonus {
        effect: "transfer",
        amount: 200,
        text: "Collect 200 from the bank!"
    }
    card GoToJail {
        effect: "move_to",
        target: "Jail",
        text: "Go directly to Jail."
    }
}
```

**Card effects:**

| Effect | Description |
|---|---|
| `"heal"` | Restore HP by `amount` |
| `"damage"` | Lose HP by `amount` |
| `"transfer"` | Gain gold by `amount` |
| `"pay"` | Lose gold by `amount` |
| `"move_to"` | Move to the tile named by `target` |

---

### Turn Structure

```
turn_structure {
    phase roll_dice {
        roll(2)
        advance(result)
    }
    phase combat {
        if (result > 9) {
            heal(20)
        } else if (result > 5) {
            pay(30)
        } else {
            damage(15)
        }
    }
}
```

Phases execute in order every turn. **Variables assigned in any phase are visible in all later phases and in event handlers** — they share a single turn-level scope.

---

### Events

Events fire automatically when a player lands on a tile or passes the start.

```
on land(player) {
    if (player_hp < 40) {
        heal(30)
    } else {
        pay(25)
    }
}

on pass(player) {
    transfer(200)
    print("You passed Go!")
}
```

`player` is a numeric parameter encoding the player's state. The built-in variables `player_hp`, `player_money`, `player_pos`, and `player_id` give you the current player's live stats directly.

---

### Built-in Player Variables

These are automatically available inside every phase and event handler. They update in real time as `heal`, `damage`, `pay`, and `transfer` execute.

| Variable | Type | Description |
|---|---|---|
| `player_id` | int | Current player index (1-based) |
| `player_hp` | int | Current player's HP |
| `player_money` | int | Current player's gold |
| `player_pos` | int | Current player's board position |
| `result` | int | Last value from `roll()` or `random()` |

```
// Check live HP inside an event
if (player_hp < 30) {
    heal(50)
    print("Critical — emergency heal applied.")
}

// Scale reward by current wealth
if (player_money > 2000) {
    transfer(200)
} else {
    transfer(500)
}
```

---

### Statements

#### Assignment

```
score -> 100
total -> score + bonus * 2
```

#### If / Else If / Else

Full chains, any length:

```
if (threat > 9) {
    damage(60)
    print("Elite zone — brutal.")
} else if (threat > 6) {
    damage(30)
    print("High zone — dangerous.")
} else if (threat > 3) {
    damage(10)
    print("Mid zone — manageable.")
} else {
    heal(20)
    print("Safe zone — rest up.")
}
```

#### While Loop

```
enemy_hp -> 60
rounds   -> 0

while (enemy_hp > 0) {
    rounds -> rounds + 1

    if (rounds > 6) {
        print("Stalemate — retreat!")
        break
    }

    random(1, 20)
    if (result > 12) { enemy_hp -> enemy_hp - 25 }
    else             { enemy_hp -> enemy_hp - 8  }
}
```

`break` exits the loop immediately and cleanly — no workarounds needed.

> **Note:** Loops are capped at 10,000 iterations to prevent infinite loops.

---

### Built-in Actions

| Action | Arguments | Description |
|---|---|---|
| `roll(n)` | dice count | Roll `n` six-sided dice → stored in `result` |
| `roll(n, sides)` | dice count, sides | Roll `n` dice with `sides` faces |
| `advance(n)` | spaces | Move the current player forward `n` spaces |
| `move_to(name)` | tile name or index | Jump to the named tile |
| `heal(n)` | amount | Restore `n` HP (updates `player_hp`) |
| `damage(n)` | amount | Deal `n` HP damage (updates `player_hp`) |
| `pay(n)` | amount | Deduct `n` gold (updates `player_money`) |
| `transfer(n)` | amount | Add `n` gold (updates `player_money`) |
| `draw()` | — | Draw a random card from the first deck |
| `draw(name)` | deck name | Draw from a specific named deck |
| `random(lo, hi)` | range | Random integer in [lo, hi] → stored in `result` |
| `print(value)` | any | Print a value to the console / log |
| `eliminate()` | — | Remove the current player from the game |
| `end_phase()` | — | Stop executing the current phase early |

---

### Expressions and Operators

```
// Arithmetic
total -> base + omen_bonus + march_bonus + conquest_bonus

// Comparison
if (score == 100) { ... }
if (hp != 0) { ... }
if (money >= 500) { ... }

// Logical
if (hp > 20 and money > 100) { heal(10) }
if (hp < 10 or money < 0)    { eliminate() }
if (not alive)               { end_phase() }

// String concatenation
msg -> "Round " + "complete"
print(msg)
```

**Operator precedence** (highest to lowest):

| Level | Operators |
|---|---|
| 1 | `not` (unary) |
| 2 | `*` `/` |
| 3 | `+` `-` |
| 4 | `==` `!=` `<` `>` `<=` `>=` |
| 5 | `and` `or` |

---

### Types

| Type | Examples | Notes |
|---|---|---|
| `int` | `42`, `0`, `-10` | Default numeric type |
| `float` | `3.14`, `0.5` | For rates and ratios |
| `string` | `"hello"`, `"Go"` | Use `+` to concatenate |
| `bool` | `true`, `false` | Result of comparisons; used with `and`/`or`/`not` |

---

## Error Messages

GameLang reports every error with a line number:

```
SyntaxError on line 7: unexpected token 'EOF' (expected })
SyntaxError on line 3: action 'heal' requires at least one argument
SyntaxError on line 2: unexpected token 'heal' (expected top-level declaration)
TypeError [Line 12]: String mixed with numeric type in '+' operation.
Runtime Error: Undefined variable 'score' at line 18
Runtime Error: Division by zero at line 22
Domain Error [Line 9]: 'heal' amount must be positive.
```

---

## Web Player

Open `web/game_player.html` in any modern browser — no server, no installation.

**How to use:**

1. Paste your `.game` code into the editor
2. Click **▶ Load Game**
3. The board renders automatically from your tile definitions
4. Click **🎲 Roll Dice / Take Turn** to play
5. Click **🃏 Draw Card** to draw from your deck
6. Watch the Players panel for live HP/gold changes and the Log for event history


---

## Showcase — Kingdom of Arath

`examples/kingdom_of_arath.game` is the most complete example in the repository. It demonstrates every language feature at once:

| Stat | Value |
|---|---|
| Source lines | 552 |
| Top-level declarations | 27 |
| Tiles | 16 |
| Card decks | 2 |
| Cards | 20 |
| Turn phases | 5 |
| `if` / `else if` / `else` statements | 41 (24 are `else if`) |
| `while` loops | 1 (6-round combat simulation) |
| `break` calls | 9 |
| Variable assignments | 40 |
| AST nodes | 828 |

Key patterns shown:

**Cross-phase variable chain** — `omen` is set in phase 1, used in phases 2, 3, 4, and 5, and in both event handlers:

```
// Phase 1
random(1, 12)
omen -> result

// Phase 3 — omen still visible
if (attack > 15 and omen > 6) {
    enemy_hp -> enemy_hp - 35
}

// Phase 4 — omen still visible
if (omen > 9) { omen_bonus -> 150 }
else if (omen > 6) { omen_bonus -> 80 }
```

**Combat loop with break** — no round-counter hacks:

```
while (enemy_hp > 0) {
    rounds -> rounds + 1
    if (rounds > 6) { print("Stalemate!") break }
    // attack / defend ...
    if (enemy_hp < 1) { conquered -> 1  break }
}
```

**Live player stats in events** — `player_money` reflects changes from phases that already ran:

```
on pass(player) {
    if (player_money > 3000) {
        transfer(300)
        print("Triumphant return! +300 gold.")
    } else {
        transfer(800)
        print("Emergency relief: +800 gold.")
    }
}
```

---

## Architecture

```
Source (.game)
     │
     ▼
  Lexer         lexer.c       Tokenizes source into a flat token list
     │
     ▼
  Parser        parser.c      Recursive-descent → Abstract Syntax Tree
     │                        Handles else-if chains and break natively
     ▼
  Typecheck     typecheck.c   Static type analysis, scope-aware
     │
     ▼
  Interpreter   interpreter.c Tree-walk executor
     │                        Turn scope wraps all phases + events
     │                        break propagates via g_break_loop flag
     ▼
  Output        stdout / game_data.json / web player
```

**Key design decisions:**

- **Tagged-union AST** (`ast.h`) — all 24 node types share one `ASTNode` struct with a `union data` field selected by `NodeType`.
- **Linked-list scopes** (`environment.c`) — `push_scope` / `pop_scope` chains environments; lookup walks outward.
- **Turn scope** (`interpreter.c`) — a scope pushed once per turn wraps all phases. Phase bodies execute via `exec_phase_body()`, which skips the inner `push_scope` so variables land in the turn scope and persist across phases and into events.
- **Control-flow flags** — `g_phase_ended` (for `end_phase()`) and `g_break_loop` (for `break`) are module-level booleans checked at every block iteration and while-loop tick.
- **Built-in player variables** — `player_hp`, `player_money`, `player_pos`, `player_id` are declared into the turn scope at the start of each turn and updated in-place by the action handlers for `heal`, `damage`, `pay`, and `transfer`.

---

## Known Limitations

- No `else if` depth limit — chains can be arbitrarily long, but deeply nested chains hurt readability
- `result` is a global variable overwritten by every `roll()` and `random()` call; save it to a named variable if you need to keep it
- `while` loops are capped at 10,000 iterations
- String comparison supports `==` and `!=` only (no ordering)
- The web player runs entirely in the browser via a JavaScript re-implementation of the parser; the C binary is a separate tool for validation and debugging

---

## License

MIT — do whatever you like with it.
