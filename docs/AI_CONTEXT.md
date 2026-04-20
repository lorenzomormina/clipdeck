# AI Context - Clipass

## 0) Reality over intent

This document is a working guide, not a source of truth above the code.

If the repository and this file disagree:

* trust the current codebase for implemented behavior
* update this file to match observed reality
* do not assume planned features are implemented just because they are mentioned here

For this repo, the primary source files are under `src/` and `resources/`.
Treat `build/` as generated output, not as architecture guidance.

---

## 1) App purpose

Clipass is a small WinAPI desktop utility for quickly inserting configured text snippets.

Today, in code, it:

* loads items from `config.txt` located next to the built executable
* runs with a tray icon and a global hotkey
* uses one top-level main window for snippet selection
* uses one separate top-level settings window for raw config editing
* copies the selected value to the clipboard
* optionally auto-pastes into the previously focused window, but only in the current `AutoClose=true` flow

The app is meant to stay lightweight, explicit, and easy to change locally.

---

## 2) Observed runtime flow

This section reflects the current code in `main.cpp`, `MainWindow.*`, `SettingsWindow.*`, `ClipListView.*`, `TrayIcon.*`, and `ClipboardUtils.*`.

1. `wWinMain` loads `AppConfig` via `LoadAppConfig()` and constructs `MainWindow`.
2. `MainWindow::Run()` creates the main HWND, then shows or hides it based on `GeneralSettings.startHidden`.
3. On `WM_CREATE`, the main window creates only the snippet-list child controls from `ClipListView`.
4. After window creation, the app adds a tray icon and registers a global hotkey.
5. The hotkey is currently hardcoded to `Ctrl+Shift+Grave` (`MOD_CONTROL | MOD_SHIFT` + `VK_OEM_3`).
   `General.Hotkey` is parsed from config but not used for registration.
6. Hotkey press captures the currently focused external window, then toggles the main snippet window.
7. Tray behavior:

   * left click opens the main window
   * right click shows a context menu with `Open`, `Config`, `Reload`, `Exit`
   * `Reload` reparses config and refreshes in-memory items
   * `Config` still shows a placeholder message box
8. The main window shows:

   * a `LISTBOX`
   * a filter `EDIT`
   * a settings icon button
9. Typing in the filter box starts a 100 ms debounce timer. Filtering currently matches `ClipItem.key` only, using a case-sensitive substring check.
10. Double-clicking a list item or pressing `Enter` in the list activates the selected item.
11. Activation copies the selected value to the clipboard.
12. If `AutoClose=true`, the main window hides after activation.
13. If `AutoClose=true` and `AutoPaste=true`, the app attempts to restore the previously focused window and sends `Ctrl+V` via `SendInput`.
14. Clicking the settings button opens a separate top-level settings window.
    If it does not exist yet, the app creates it first.
    If it already exists, the app shows it and brings it to the foreground.
15. After opening settings from the main window, the main window hides.
16. When shown from a hidden state, the settings window reloads the raw config file text into a multiline `EDIT` control.
17. `Save` writes the raw editor text back to disk, reloads `AppConfig`, and keeps the settings window open.
18. Leaving settings through `Cancel`, `Esc`, or `WM_CLOSE` goes through the dirty-check flow:

   * if not dirty, hide the settings window
   * if dirty, show `Yes / No / Cancel`
   * `Yes` saves, reloads config, then hides the settings window
   * `No` discards editor changes, then hides the settings window
   * `Cancel` keeps the settings window open
19. The main window auto-hides on focus loss while visible.
20. The settings window is its own top-level HWND and does not depend on mounting or unmounting controls inside the main window.

---

## 3) UI windows

The application currently has two concrete top-level windows:

* the main snippet window
* the settings window

### Main snippet window

**Purpose**

* fast selection of configured snippets

**Existing controls**

* `LISTBOX` for visible items
* single-line filter `EDIT`
* settings icon `BUTTON`

**Observed behavior**

* filter textbox sits at the bottom of the client area
* settings button sits next to the filter textbox
* list box fills the upper area
* filtering is debounced
* activation is by double click or `Enter` in the list box
* selected item value is copied to clipboard
* hidden items are still present in the list, but their displayed value is masked as `*****`
* the window uses tool-window style and is hidden from the taskbar
* losing focus hides the window

---

### Settings window

**Purpose**

* edit the raw text of `config.txt`

**Existing controls**

* multiline `EDIT` control with scrollbars
* `Save` button
* `Cancel` button

**Observed behavior**

* settings has its own top-level HWND and lifecycle
* the editor shows the file text as stored on disk
* `Save` writes the raw editor text to disk and reloads config
* `Cancel` means "leave settings", not "clear text"
* dirty state is tracked by comparing current editor text with the original loaded text
* programmatic text loads suppress the dirty flag via `suppressEditChange`
* `Esc` is handled through message pre-translation in the main loop so it works even when focus is inside child controls

**Current limits**

* layout is fixed with hardcoded coordinates
* size is still based on `WindowSettings.width` and `WindowSettings.height`
* there is no config validation or parse error feedback before reload

---

## 4) Functional scope

### Implemented

* one main WinAPI top-level window for snippet selection
* one separate WinAPI top-level settings window for raw config editing
* tray icon with `Open`, `Reload`, and `Exit`
* global hotkey registration
* searchable key/value list UI
* clipboard copy on item activation
* optional auto-close after activation
* optional auto-paste attempt back to the previously focused window
* raw config text editor with save/discard confirmation
* config reload after settings save or tray reload

### Partial / limited

* `General.Hotkey` is parsed and stored, but not used; registration is still hardcoded
* `ClipItem.EnableValueSearch` is parsed and stored, but filtering does not use it
* `Window` settings affect main-window size and main-list layout creation, but relayout on later size changes is incomplete
* settings editing is functional but very basic
* config save path only reports file-write failure; malformed config content is not validated for the user
* auto-paste depends on best-effort focus restoration (`SetForegroundWindow`, `SetFocus`, `SendInput`)

### Planned / not implemented

* configurable hotkey registration from `General.Hotkey`
* value-based search using `EnableValueSearch`
* tray `Config` action opening the real settings window
* stronger config validation and user-facing parse errors

---

## 5) Configuration model

The runtime config file is `<exe directory>\config.txt`.
The default copy is stored in `resources/config.txt` and copied beside the executable by CMake post-build steps.

`AppConfig.*` is the source of truth for what is currently parsed.

Example shape:

```ini
[General]
Hotkey="Ctrl+Shift+Space"
StartHidden=false
AutoClose=true
AutoPaste=false

[Window]
Width=400
Height=300
Margin=4
TextBoxMargin=6

[[Item]]
Key="firma"
Value="Cordiali saluti,\nMario Rossi"
Hidden=false
EnableValueSearch=true
```

### Current parser behavior

* sections recognized: `[General]`, `[Window]`, `[[Item]]`
* inline comments beginning with `;` or `#` are stripped when outside quotes
* quoted strings support `\n`, `\"`, and `\\`
* unknown sections and unparseable lines are ignored
* invalid values usually fall back silently to defaults rather than surfacing an error

### Fields currently consumed by app behavior

#### `[General]`

* `StartHidden`: used
* `AutoClose`: used
* `AutoPaste`: used, but only within the current `AutoClose=true` activation branch
* `Hotkey`: parsed only

#### `[Window]`

* `Width`, `Height`: used for main-window creation, reload sizing, and settings-window sizing
* `Margin`, `TextBoxMargin`: used by `ClipListView` layout

#### `[[Item]]`

* `Key`: used for display and filtering
* `Value`: used for clipboard/paste output
* `Hidden`: used only to mask the visible display text
* `EnableValueSearch`: parsed only

### Important note about config editing

The settings window edits the raw config file text, not a structured form.

That means:

* save does not validate syntax before writing
* reload reparses whatever can be parsed
* future AI changes must keep the distinction between raw text editing and structured config parsing explicit

---

## 6) Current product state

The current application is usable for the main snippet-selection flow and for raw config editing.

What is stable enough to rely on:

* startup through `main.cpp`
* `MainWindow` as the main snippet-window orchestrator
* `SettingsWindow` as the separate config-editor window
* `ClipListView` as the main-list control owner
* `AppConfig` as the config parser / loader
* tray and clipboard plumbing

What is still rough:

* settings UX and layout
* config error handling
* honoring all declared config fields
* resize / relayout behavior

---

## 7) Architecture principles

### Goals

* keep WinAPI usage explicit
* keep feature edits local
* reduce regressions in the main selection flow
* make small AI-assisted changes predictable

### Practical design rules for this repo

* `MainWindow` should orchestrate the snippet-list window, tray actions, hotkey events, config reload, and item activation
* `SettingsWindow` should own the separate settings HWND, settings controls, and dirty-check/save behavior
* `ClipListView` should own main-list controls and filter behavior
* `AppConfig` should remain the config parser / loader boundary
* clipboard and paste mechanics should stay in `ClipboardUtils`
* tray and hotkey logic should stay in their existing modules unless the task directly changes them
* avoid inventing new layers unless the current file is clearly overloaded and the new boundary matches actual responsibilities

---

## 8) File responsibilities

These reflect the current repository, not an idealized structure.

### `src/main.cpp`

* WinAPI entry point
* loads config
* constructs and runs `MainWindow`

### `src/MainWindow.*`

* owns the main snippet-window HWND
* handles `WndProc` dispatch for that window
* creates main-list controls through `ClipListView`
* coordinates tray actions, hotkey events, config reload, and item activation
* opens the settings window when requested
* provides message-loop pre-translation to support settings `Esc` handling

### `src/SettingsWindow.*`

* owns the separate settings HWND
* creates the multiline `EDIT`, `Save`, and `Cancel` controls
* loads raw config text from disk
* writes raw config text back to disk
* handles dirty-check flow and hide/close behavior
* notifies `MainWindow` after successful save so config can be reloaded

### `src/ClipListView.*`

* creates main-list child controls
* owns filter debounce timer behavior
* populates and filters the list box
* exposes selection and settings-button events back to `MainWindow`

### `src/AppConfig.*`

* locates runtime files relative to the executable
* parses `config.txt`
* decodes quoted values and escaped characters
* returns the in-memory config model

There is still no separate save/validate API here; raw-text settings save writes directly to disk in `SettingsWindow.cpp`.

### `src/ClipboardUtils.*`

* captures current focus targets before overlay activation
* copies Unicode text to the Windows clipboard
* attempts paste-back via focus restore + `SendInput`

### `src/GlobalHotkey.*`

* thin wrapper around `RegisterHotKey` / `UnregisterHotKey`

### `src/TrayIcon.*`

* tray icon add/remove
* tray callback translation
* tray context menu creation and command mapping

### `resources/resource.h` / `resources/resource.rc`

* icon resource identifiers and icon bundle declarations

### `resources/config.txt`

* default sample config copied to the output directory after build

### `docs/AI_CONTEXT.md`

* AI-facing repository context
* should be updated when implemented behavior or module boundaries materially change

---

## 9) Recommended module boundaries

Keep these boundaries unless the task explicitly improves one of them:

* main-list control behavior belongs in `ClipListView.*`
* raw config parsing belongs in `AppConfig.*`
* separate raw config editing behavior belongs in `SettingsWindow.*`
* snippet-window orchestration belongs in `MainWindow.*`
* clipboard / paste behavior belongs in `ClipboardUtils.*`
* tray menu behavior belongs in `TrayIcon.*`

If you introduce a new module, it should remove a concrete overload from an existing file, not add abstract architecture.

---

## 10) State and behavior expectations

Important current state includes:

* captured external focus in `lastFocus_`
* full config model in `config_`
* visible filtered item index mapping in `ClipListView`
* settings editor state in `SettingsWindow`:

  * `originalText`
  * `isDirty`
  * `suppressEditChange`

Behavior expectations future edits should preserve unless the task explicitly changes them:

* hotkey toggles the main snippet window
* tray left click opens the main window
* main window auto-hides on focus loss
* settings dirty-check blocks accidental leave when user chooses `Cancel`
* settings `Save` writes raw text first, then reloads config
* item activation always copies first; paste is additional behavior

---

## 11) Coding conventions

### General

* preserve explicit WinAPI types and message names
* prefer small helpers over large speculative abstractions
* keep `std::wstring` usage consistent with the current Unicode codebase
* keep control IDs and message routing easy to trace

### Existing style cues

* use `CreateWindowExW`, `ShowWindow`, `SendMessageW`, `MessageBoxW`, etc.
* use small helper structs for grouped HWND state
* keep file-local helpers in anonymous namespaces when they are not shared

### What to avoid

* global redesigns
* framework-like wrappers around every HWND
* moving unrelated behavior across modules just for style

---

## 12) Implementation constraints

* no unrelated refactors
* no broad architecture rewrite
* preserve current behavior unless the task explicitly changes it
* keep changes local to the relevant module(s)
* prefer editing tracked source files under `src/`, `resources/`, and `docs/`
* do not treat generated `build/` files as the place to implement source changes

---

## 13) Known issues / weak points

Only issues justified by the current code are listed here.

* `General.Hotkey` is misleading today: it is parsed but ignored for registration.
* `EnableValueSearch` is misleading today: it is parsed but never used by filtering.
* tray menu `Config` exists but only shows `Config option not implemented yet.`
* filtering is key-only and case-sensitive
* hidden items are still selectable; only their displayed value is masked
* settings save writes raw text without validation; malformed config can silently reload as partial defaults
* there is no `WM_SIZE` handling; control layout is not recomputed on user resize
* `ReloadConfig()` resizes the main window and updates settings-window sizing, but existing child-control layout is not fully recomputed afterward
* settings control positions are hardcoded and do not use `WindowSettings.margin` or `WindowSettings.textBoxMargin`
* file I/O uses explicit UTF-8 write output with UTF-8/ACP read fallback, but there is still no user-facing encoding validation

---

## 14) Near-term priorities

1. Make config-driven hotkey behavior real or remove the misleading `Hotkey` claim from config/docs.
2. Decide the intended search semantics, then implement them locally in `ClipListView` and `AppConfig` without broad UI changes.
3. Improve settings robustness: validation/error reporting before reload, and clarify desired raw-text editing behavior.
4. Add local relayout handling for resize/reload instead of expanding the window architecture.
5. Replace or remove the tray `Config` placeholder entry.

---

## 15) Feature roadmap notes

These are directionally consistent with the current codebase, not commitments.

* configurable hotkey support based on parsed config
* value-aware search
* better config validation and error messages
* incremental cleanup of settings UI layout

Do not assume any of these are implemented unless the code shows them.

---

## 16) Prompt contract for AI

Every future AI task should state:

### Task

* the exact behavior to change

### Allowed files

* the smallest practical set of files to modify

### Constraints

* no global refactor
* preserve unrelated behavior
* keep WinAPI explicit
* do not invent new architecture unless the task requires a very small, concrete extraction

### Acceptance criteria

* a testable behavior change or code cleanup outcome
* no regressions in tray / hotkey / main-list selection unless those are part of the task

### Behavior preservation reminders

* if editing settings logic, keep dirty-check behavior explicit
* if editing item activation, preserve clipboard copy behavior
* if editing config parsing, keep current file format compatibility unless the task explicitly changes it

---

## 17) Preferred workflow

1. inspect the target file and its direct callers
2. confirm what is already implemented
3. choose the smallest local change
4. implement only that change
5. verify behavior locally if feasible
6. update this file if behavior or boundaries changed

Avoid speculative rewrites.

---

## 18) Non-goals

The project is not:

* a multi-window framework
* a plugin architecture
* a generic settings engine
* a showcase for abstraction-heavy C++

The project is:

* small
* WinAPI-explicit
* configuration-driven
* suitable for narrow, local edits

---

## 19) Summary for AI

Clipass is a small WinAPI snippet inserter with:

* one main snippet HWND
* one separate settings HWND
* tray + hotkey entry points
* raw-text config editing
* a narrow set of source modules

Future AI work should stay local, preserve the current flow, and prefer code reality over old prompts or aspirational notes.
