# Dialogue System Documentation

Client-side dialogue panel driven by `logic_dialogue` server entity and `.txt` dialogue files.

---

## Architecture

```
Hammer (logic_dialogue)
  │  StartDialogue input
  ▼
Server (logic_dialogue.cpp)
  │  DIALOGUE_MSG_SETTINGS  ──►  Client stores defaults
  │  DIALOGUE_MSG_START     ──►  Client loads file, shows node, opens panel
  │  DIALOGUE_MSG_STOP      ──►  Client closes panel
  │  DIALOGUE_MSG_FOCUS     ──►  Client receives entity focus position
  │  DIALOGUE_MSG_UNLOCK    ──►  Client unlocks a condition
  │  DIALOGUE_MSG_LOCK      ──►  Client locks a condition
  ▼
Client (DialoguePanel.cpp)
  │  Reads .txt dialogue file (KeyValues)
  │  Renders nodes: text, choices, typewriter
  │  Sends server commands for NPC actions
  ▼
Server (CON_COMMANDs)
```

---

## Hammer Entity: `logic_dialogue`

### Keyfields

| Keyfield             | Type    | Default      | Description                                          |
|----------------------|---------|--------------|------------------------------------------------------|
| `dialogue_file`      | string  | `""`         | Path to dialogue `.txt` file (e.g. `resource/dialogues/npc_01.txt`) |
| `start_node`         | string  | `node_start` | Node name to begin the dialogue at                   |
| `default_npc`        | string  | `""`         | Targetname of the NPC tied to this dialogue. If this NPC dies while the dialogue is active, it closes automatically and fires `OnDialogueStopped`. |
| `typewriter_enabled` | bool    | `1`          | Default typewriter mode (nodes can override)         |
| `typewriter_speed`   | float   | `1.0`        | Default typewriter speed multiplier (nodes can override) |
| `typewriter_sound`   | sound   | `""`         | Sound played per character while typewriter prints   |
| `open_sound`         | sound   | `""`         | Sound when dialogue panel opens                      |
| `close_sound`        | sound   | `""`         | Sound when dialogue panel closes                     |

### Inputs

| Input            | Parameter | Description                                       |
|------------------|-----------|---------------------------------------------------|
| `StartDialogue`  | —         | Opens the dialogue panel                          |
| `StopDialogue`   | —         | Closes the dialogue panel                         |
| `UnlockChoice`   | string    | Unlocks choices with this condition name          |
| `LockChoice`     | string    | Locks choices with this condition name            |

### Outputs

| Output               | Description                          |
|----------------------|--------------------------------------|
| `OnDialogueStarted`  | Fired when dialogue panel is opened  |
| `OnDialogueStopped`  | Fired when dialogue panel is closed (including auto-close from NPC death) |

---

## Dialogue File Format

KeyValues `.txt` file. Each top-level key is a **node name**.

### Example

```
"DialogueFile"
{
    "node_start"
    {
        "speaker"           "Barney"
        "focus"             "npc_barney_01"
        "anim"              "ACT_IDLE_ANGRY"
        "sound_npc"         "vo/barney/hello.wav"
        "color"             "255.220.180"
        "speed"             "0.8"
        "sound_typewriter"  "ui/type_click.wav"
        "text"              "Hey, <color=255.0.0>Gordon<color=255.220.180>! <speed=0.3>Long time no see..."

        "choice1"
        {
            "text"          "What happened here?"
            "next"          "node_explain"
        }
        "choice2"
        {
            "text"          "Open the gate and ask"
            "command"       "ent_fire gate_01 open"
            "next"          "node_gate"
        }
        "choice3"
        {
            "text"          "I have to go."
            "exit"          ""
            "sound_hover"   "ui/buttonrollover.wav"
            "sound_press"   "ui/button_close.wav"
        }

        "closable"          ""
    }

    "node_explain"
    {
        "speaker"           "Barney"
        "focus"             "npc_barney_01"
        "typewriter"        "0"
        "text"              "It all started when..."
    }
}
```

---

## Node Keys

All keys are optional. If not specified, entity defaults from `logic_dialogue` are used.

### Display

| Key         | Type   | Default            | Description                                |
|-------------|--------|--------------------|--------------------------------------------|
| `speaker`   | string | `"..."`            | Name shown in the character name label     |
| `text`      | string | `"..."`            | Text content (supports inline tags)        |
| `color`     | string | —                  | Default text color `R.G.B` or `R.G.B.A`   |

### Typewriter

| Key                | Type   | Default          | Description                                      |
|--------------------|--------|------------------|--------------------------------------------------|
| `typewriter`       | bool   | entity default   | `1` = typewriter mode, `0` = instant text        |
| `speed`            | float  | entity default   | Typewriter speed multiplier (higher = faster)    |
| `sound_typewriter` | string | entity default   | Typewriter tick sound (overrides entity default)  |

### Actions (executed once when node loads, before text)

| Key            | Type   | Description                                          |
|----------------|--------|------------------------------------------------------|
| `focus`        | string | Target entity name (NPC or `info_target`) — camera tracks it, NPC faces player, FOV zooms based on distance |
| `anim`         | string | Activity name to play on the focused NPC (e.g. `ACT_IDLE_ANGRY`) |
| `sound_npc`    | string | Sound played from the focused NPC's position (`CHAN_VOICE`) |
| `sound_world`  | string | Ambient sound played globally                        |
| `command`      | string | Console command executed immediately                 |
| `sound_action` | string | Overrides the panel close sound for this node        |

### UI

| Key        | Type   | Description                                    |
|------------|--------|------------------------------------------------|
| `closable` | flag   | If present, show the close (X) button on panel |

---

## Inline Tags

Used inside `"text"` values. Tags are not printed — they trigger actions mid-text.

**All tags use the format `<name=value>`.**

### Text Appearance

| Tag                     | Example                    | Description                             |
|-------------------------|----------------------------|-----------------------------------------|
| `<color=R.G.B>`        | `<color=255.0.0>`         | Change text color (RGB, 0–255)          |
| `<color=R.G.B.A>`      | `<color=255.0.0.128>`     | Change text color with alpha            |
| `<speed=N>`            | `<speed=0.3>`             | Change typewriter speed mid-text        |

### Camera & NPC

| Tag                     | Example                           | Description                          |
|-------------------------|-----------------------------------|--------------------------------------|
| `<focus=NAME>`          | `<focus=info_camera_angle_1>`    | Move camera to entity mid-text       |
| `<anim=ACT>`           | `<anim=ACT_GESTURE_WAVE>`        | Play NPC animation mid-text          |

### Sound

| Tag                           | Example                                 | Description                          |
|-------------------------------|------------------------------------------|--------------------------------------|
| `<sound_npc=PATH>`            | `<sound_npc=vo/barney/laugh.wav>`       | Play sound from focused NPC          |
| `<sound_world=PATH>`          | `<sound_world=ambient/explosion.wav>`   | Play global ambient sound            |
| `<sound_typewriter=PATH>`     | `<sound_typewriter=ui/glitch_tick.wav>` | Change typewriter tick sound         |

### Commands

| Tag                     | Example                               | Description                     |
|-------------------------|---------------------------------------|---------------------------------|
| `<command=CMD>`         | `<command=ent_fire relay trigger>`    | Execute console command mid-text |

---

## Choice Keys

Choices are sub-keys named `choice1` through `choice5`.

| Key            | Type   | Default                           | Description                          |
|----------------|--------|-----------------------------------|--------------------------------------|
| `text`         | string | `"..."`                           | Button label                         |
| `next`         | string | —                                 | Node name to navigate to on press    |
| `command`      | string | —                                 | Console command to execute on press  |
| `exit`         | flag   | —                                 | If present, closes dialogue on press |
| `enabled`      | string | —                                 | Condition for button availability (see below) |
| `sound_hover`  | sound  | `ui/buttonrollover.wav`           | Sound on mouse hover                 |
| `sound_press`  | sound  | `common/bugreporter_succeeded.wav`| Sound on click                       |

`next`, `command`, and `exit` can be **combined freely**. Execution order: **command → next → exit**.

### `enabled` — Condition System

The `enabled` key controls whether a choice button is clickable:

| Value          | Behavior                                                    |
|----------------|-------------------------------------------------------------|
| absent / empty | Button is always enabled (default)                          |
| `"0"`          | Button is always disabled                                   |
| any string     | Button is disabled until that condition name is unlocked    |

Conditions are unlocked/locked via the `UnlockChoice` / `LockChoice` inputs on `logic_dialogue`. The condition state persists across nodes — once unlocked, it stays unlocked for the lifetime of the dialogue panel. When a node is shown, each button checks if its condition is currently unlocked.

If an unlock/lock happens while the dialogue is open, buttons update immediately.

**Example:** A choice that requires the player to find a key item:
```
"choice2"
{
    "text"      "Use the keycard"
    "enabled"   "has_keycard"
    "next"      "node_door_opened"
}
```
In Hammer, fire `UnlockChoice` with parameter `has_keycard` on the `logic_dialogue` entity when the player picks up the keycard.

### Examples

Navigate to another node:
```
"choice1"
{
    "text"    "Tell me more."
    "next"    "node_explain"
}
```

Execute a command and navigate:
```
"choice2"
{
    "text"      "Open the door"
    "command"   "ent_fire door_01 open"
    "next"      "node_door_opened"
}
```

Exit button (closes dialogue):
```
"choice3"
{
    "text"    "Goodbye."
    "exit"    ""
}
```

Execute a command and exit:
```
"choice4"
{
    "text"      "Take the item and leave"
    "command"   "ent_fire item_01 kill"
    "exit"      ""
}
```

---

## Priority / Override Chain

Settings flow from broadest to most specific. Each level overrides the previous:

```
logic_dialogue (Hammer entity)     ← broadest defaults
    ▼
Node keys (dialogue .txt file)     ← per-node overrides
    ▼
Inline tags (<tag=value>)          ← per-character overrides
```

**Example:** Entity sets `typewriter_speed = 1.0` → node sets `speed = 0.5` → inline `<speed=2.0>` overrides mid-text.

---

## Sound Playback

### Channels

Different sound types use different playback methods so they don't cut each other off:

| Sound Type       | Playback Method              | Notes                                |
|------------------|------------------------------|--------------------------------------|
| Open / Close     | `EmitSound` on `CHAN_ITEM`   | UI feedback, 2D (no attenuation)     |
| Typewriter tick  | `vgui::surface()->PlaySound` | VGUI surface sound, repeats per char |
| NPC voice        | `EmitSound` on `CHAN_VOICE`  | Spatialized from NPC, with lip sync  |
| World ambient    | `EmitAmbientSound`           | Global, no position                  |

### NPC Sound Details

NPC sounds (`sound_npc` / `<sound_npc=...>`) are played server-side via `internal_dialogue_sound`. The engine precaches the sound at runtime and emits it from the entity using `CHAN_VOICE` at `SNDLVL_TALKING`, which enables automatic lip sync on NPCs that support it.

---

## Focus System

The `focus` key / `<focus=...>` tag targets any entity by `targetname`:

- **NPC:** Camera tracks head bone (`ValveBiped.Bip01_Head1`), NPC body turns to face player, NPC eyes look at player (via `AddLookTarget`)
- **`info_target`:** Camera tracks entity position from server — useful for cinematic camera angles without NPC involvement
- **Other entities:** Camera tracks `WorldSpaceCenter()` (client-side) or `GetAbsOrigin()` (server fallback)

Camera tracking is continuous with exponential smoothing (`1 - e^(-speed * dt)`) for frame-rate independent movement. The client re-reads the entity position every tick so it follows moving targets.

For server-only entities (no client representation), the client requests position updates from the server each tick via `internal_dialogue_focus_update`.

### FOV Zoom

FOV zoom is automatic based on distance. The FOV is linearly interpolated between the near and far bounds:

| Distance    | FOV  | Feel              |
|-------------|------|-------------------|
| ≤ 64 units  | 30°  | Extreme close-up  |
| 256 units   | ~45° | Medium distance   |
| ≥ 512 units | 65°  | Far away, minimal zoom |

Formula: `FOV = RemapValClamped(distance, 64, 512, 30, 65)`, clamped to `[30, 75]`.

---

## Typewriter Behavior

- The panel ticks at a fast fixed rate (**10ms**); characters are printed based on **elapsed realtime**, not tick count
- Base interval: **50ms** per character (20 chars/sec at speed 1.0)
- Interval formula: `interval = 50ms / speed`
- At speed 0.25 → 200ms per char (slow, dramatic); at speed 5.0 → 10ms per char (very fast)
- Tags inside text are processed instantly (not printed) when the typewriter cursor reaches them
- **Left-click** on the panel **skips** the typewriter — all remaining text appears instantly, all pending tags execute
- Typewriter tick sound plays via `vgui::surface()->PlaySound` once per character revealed
- When typewriter finishes (or is skipped), the sound stops automatically
- Supports UTF-8: multi-byte characters are printed as a single unit

---

## Panel Animations

The dialogue panel uses a multi-phase animation system:

### Slide-In (ANIM_SLIDE_IN)

When the panel opens, it slides up from below the screen to its final position over **0.35 seconds** with a smooth ease-out curve. Panel alpha fades from 0 to 255 simultaneously. Choice buttons remain hidden (alpha 0) during this phase.

The typewriter does **not** start until the slide-in animation completes.

### Button Fade-In (ANIM_BUTTONS_FADE)

After typewriter text finishes (or immediately if typewriter is disabled), choice buttons fade in over **0.25 seconds** with an ease-in curve.

### Slide-Out (ANIM_SLIDE_OUT)

When dialogue closes (via choice button, close button, or `StopDialogue`), the panel slides down off-screen over **0.35 seconds** with an ease-in curve. Panel alpha fades from 255 to 0 simultaneously. Input is disabled immediately so the player can't click during the animation.

### Deferred Hide

When a choice button or the close button is pressed, the panel waits **0.15 seconds** before starting the slide-out animation. This allows button press/release sounds to finish playing.

---

## Auto-Close Behavior

The dialogue panel closes automatically in several situations:

| Trigger                  | Mechanism                                                         |
|--------------------------|-------------------------------------------------------------------|
| **Player death**         | Client `OnTick` checks `IsAlive()` every tick; closes immediately if player is dead. Works in SP where the `player_death` event doesn't fire. |
| **NPC death**            | If `default_npc` is set on `logic_dialogue`, the server `Think` checks the NPC every 0.1s. If the NPC is dead or removed, the dialogue stops and `OnDialogueStopped` fires. |
| **Map change / load**    | Client listens for `game_newmap` game event; closes immediately.  |

All auto-close paths call `HidePanelImmediate()` which restores FOV, HUD, and save/load access without playing the slide-out animation.

---

## Save/Load Blocking

While the dialogue panel is open, `save`, `load`, `quicksave`, and `quickload` commands are blocked. The original engine commands are shadowed by overrides that check a global lock flag. When dialogue closes (by any means), the lock is released and save/load works normally again.

---

## HUD Behavior

The HUD is automatically hidden when the dialogue panel opens and restored when it closes. This is done via the `internal_dialogue_hud` server command which sets/clears the `HIDEHUD_ALL` flag on the player.
