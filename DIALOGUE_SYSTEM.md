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
  ▼
Client (DialoguePanel.cpp)
  │  Reads .txt dialogue file (KeyValues)
  │  Renders nodes: text, choices, typewriter
  │  Sends server commands for NPC actions
  ▼
Server (CON_COMMANDs)
  sv_dialogue_lookatplayer  ──  NPC faces player
  sv_dialogue_animate       ──  NPC plays activity
  sv_dialogue_zoom          ──  Player FOV change
  sv_dialogue_hud           ──  Hide/show HUD
```

---

## Hammer Entity: `logic_dialogue`

### Keyfields

| Keyfield             | Type    | Default      | Description                                          |
|----------------------|---------|--------------|------------------------------------------------------|
| `dialogue_file`      | string  | `""`         | Path to dialogue `.txt` file (e.g. `resource/dialogues/npc_01.txt`) |
| `start_node`         | string  | `node_start` | Node name to begin the dialogue at                   |
| `typewriter_enabled`  | bool    | `1`          | Default typewriter mode (nodes can override)         |
| `typewriter_speed`    | float   | `1.0`        | Default typewriter speed multiplier (nodes can override) |
| `typewriter_sound`    | sound   | `""`         | Sound played each tick while typewriter prints       |
| `open_sound`          | sound   | `""`         | Sound when dialogue panel opens                      |
| `close_sound`         | sound   | `""`         | Sound when dialogue panel closes                     |

### Inputs

| Input            | Description                |
|------------------|----------------------------|
| `StartDialogue`  | Opens the dialogue panel   |
| `StopDialogue`   | Closes the dialogue panel  |

### Outputs

| Output               | Description                          |
|----------------------|--------------------------------------|
| `OnDialogueStarted`  | Fired when dialogue panel is opened  |
| `OnDialogueStopped`  | Fired when dialogue panel is closed  |

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
            "command"       "gotonode node_explain"
        }
        "choice2"
        {
            "text"          "I have to go."
            "command"       "turnoff"
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
| `sound_typewriter` | string | entity default   | Typewriter tick sound (overrides entity default) |

### Actions (executed once when node loads, before text)

| Key            | Type   | Description                                          |
|----------------|--------|------------------------------------------------------|
| `focus`        | string | Target entity name (NPC or `info_target`) — camera tracks it, NPC faces player, FOV zooms based on distance |
| `anim`         | string | Activity name to play on the focused NPC (e.g. `ACT_IDLE_ANGRY`) |
| `sound_npc`    | string | Sound played from the focused NPC's position (`CHAN_VOICE`) |
| `sound_world`  | string | Ambient sound played globally                        |
| `command`      | string | Console command executed immediately                 |

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

### Other

| Tag                     | Example                               | Description                     |
|-------------------------|---------------------------------------|---------------------------------|
| `<command=CMD>`         | `<command=ent_fire relay trigger>`    | Execute console command mid-text |

---

## Choice Keys

Choices are sub-keys named `choice1` through `choice5`.

| Key            | Type   | Default                           | Description                     |
|----------------|--------|-----------------------------------|---------------------------------|
| `text`         | string | `"..."`                           | Button label                    |
| `command`      | string | `""`                              | Command on press (see below)    |
| `enabled`      | bool   | `1`                               | Whether the button is clickable |
| `exit`         | flag   | —                                 | If present, marks as exit button (default command: `turnoff`) |
| `sound_hover`  | sound  | `ui/buttonrollover.wav`           | Sound on mouse hover            |
| `sound_press`  | sound  | `common/bugreporter_succeeded.wav`| Sound on click                  |

### Choice Commands

| Command                  | Description                              |
|--------------------------|------------------------------------------|
| `gotonode NODE_NAME`     | Navigate to another dialogue node        |
| `turnoff`                | Close the dialogue panel                 |
| `cmd CONSOLE_COMMAND`    | Execute a console command                |
| `startdiag FILE_PATH`   | Load a different dialogue file           |

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

## Sound Channels

Sounds use separate engine channels so they don't cut each other off:

| Sound Type       | Channel       | Notes                                |
|------------------|---------------|--------------------------------------|
| Open / Close     | `CHAN_ITEM`   | UI feedback sounds                   |
| Typewriter tick  | `CHAN_BODY`   | Repeats each tick while printing     |
| NPC voice        | `CHAN_VOICE`  | Spatialized from NPC position        |
| World ambient    | Ambient       | Global, no position                  |

---

## Focus System

The `focus` key / `<focus=...>` tag targets any entity by `targetname`:

- **NPC:** Camera tracks head bone (`ValveBiped.Bip01_Head1`), NPC body turns to face player, NPC eyes look at player
- **`info_target`:** Camera tracks entity origin — useful for cinematic camera angles without NPC involvement

**FOV zoom** is automatic based on distance:

| Distance | FOV  | Feel              |
|----------|------|-------------------|
| 64 units | 30°  | Extreme close-up  |
| 256 units| 55°  | Across a room     |
| 512+ units| 65° | Far away, minimal zoom |

---

## Typewriter Behavior

- Text is revealed character-by-character at `TYPEWRITER_BASE_SPEED × speed` characters per tick (100ms tick rate)
- Tags inside text are processed instantly (not printed) when the typewriter cursor reaches them
- **Left-click** on the panel **skips** the typewriter — all remaining text appears instantly, all tags execute
- Typewriter tick sound plays each tick while at least one character is revealed
- When typewriter finishes (or is skipped), the sound stops automatically
