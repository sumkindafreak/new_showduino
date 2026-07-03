# CYD SD Browser Design

The SD browser is a core part of the Showduino v1 controller experience.

## Important Architecture Rule

The CYD is the controller.

The Mega is the executor.

The SD browser lives on the CYD UI, but scenes are executed by the Mega.

For the first version, the CYD can browse its own SD card and send scene commands to the Mega.

Later, if scene files live on the Mega SD card, the CYD can request a file list from the Mega using serial commands.

## SD Browser Goals

The CYD should allow the user to:

- Open the Scene Browser from the main menu.
- List `.shdo` scene files.
- Scroll through files.
- Select a scene.
- See basic scene information.
- Send `SCENE:LOAD:<filename>` to the Mega.
- Send `SCENE:PLAY` to the Mega.
- Send `SCENE:STOP` to the Mega.
- Trigger `EMERGENCY:STOP` at any time.

## Recommended SD Card Layout

```text
/scenes/
  chamber_intro.shdo
  portal_test.shdo
  whitechapel_scare.shdo

/audio/
  001.wav
  002.wav
  heartbeat.mp3

/shows/
  chamber_full_show.shdo
```

## Browser Screens

### Main Menu

Buttons:

```text
Scenes
Manual
Diagnostics
Settings
Emergency Stop
```

### Scene Browser

Shows a list of `.shdo` files from:

```text
/scenes/
```

Example:

```text
> chamber_intro.shdo
  portal_test.shdo
  whitechapel_scare.shdo
```

Buttons:

```text
Up
Down
Open
Back
Emergency Stop
```

### Scene Detail

Shows selected scene:

```text
Scene: chamber_intro.shdo
Status: Ready
```

Buttons:

```text
Load
Play
Stop
Back
Emergency Stop
```

## CYD to Mega Serial Commands

When selecting a scene:

```text
SCENE:LOAD:chamber_intro.shdo
```

When playing:

```text
SCENE:PLAY
```

When stopping:

```text
SCENE:STOP
```

Emergency:

```text
EMERGENCY:STOP
```

## Mega to CYD Responses

Expected examples:

```text
OK:SCENE:LOAD:chamber_intro.shdo
OK:SCENE:PLAY
OK:SCENE:STOP
ERR:SCENE:FILE_NOT_FOUND
STATUS:EMERGENCY_ACTIVE
STATUS:READY
```

## Local vs Remote File Browsing

There are two possible browser models.

### Model A — CYD SD Browser

CYD reads its own SD card and lists files directly.

Pros:

- Fast UI.
- Simple file list.
- Good for early testing.

Cons:

- Scene files must also exist on Mega SD card if Mega executes locally.
- Risk of CYD and Mega SD cards getting out of sync.

### Model B — Mega SD Browser Through Serial

CYD asks Mega for the scene list.

Commands:

```text
SCENE:LIST
```

Mega replies:

```text
SCENE:FILE:chamber_intro.shdo
SCENE:FILE:portal_test.shdo
SCENE:FILE:whitechapel_scare.shdo
SCENE:LIST:END
```

Pros:

- CYD always shows what Mega can actually run.
- Best final design.

Cons:

- Needs more serial protocol work.

## v1 Recommendation

Use Model B as the final target:

```text
CYD asks Mega for scene files.
Mega lists files from its SD card.
CYD displays the returned list.
CYD tells Mega what to load/play.
```

This keeps the Mega as the true show brain.

## Weekend Test Version

For the weekend test, implement this minimum:

```text
CYD button sends SCENE:TEST
CYD button sends SCENE:STOP
CYD button sends SD:STATUS
CYD displays Mega replies
```

Then upgrade to:

```text
SCENE:LIST
SCENE:FILE:<name>
SCENE:LIST:END
SCENE:LOAD:<name>
SCENE:PLAY
```

## Final Design Statement

The CYD SD browser should become a scene browser for the Mega executor.

The user should never need to type scene commands manually during a show.
