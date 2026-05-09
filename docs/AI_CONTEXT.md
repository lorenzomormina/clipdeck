# AI Context - ClipDeck

This file is at `docs/AI_CONTEXT.md`.

When ChatGPT generates a prompt for Codex, it MUST include a dedicated section at the top of the prompt with the following instruction:

"Before making any changes, read `docs/AI_CONTEXT.md` and use it as the authoritative context for architecture, behavior, and constraints. If the code and the document disagree, follow the code."

This instruction must always be included verbatim or with equivalent strength.

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

ClipDeck is a small WinAPI desktop utility for quickly inserting configured text snippets.

Today, in code, it:

* loads items from `settings.txt` located next to the built executable
* runs with a tray icon and a global hotkey
* uses one top-level main window for snippet selection
* uses one separate top-level settings window for raw settings editing
* copies the selected value to the clipboard
* optionally hides the main window and auto-pastes into the captured foreground/focused window snapshot, using per-item `AutoClose` / `AutoPaste` overrides when present
* supports copy-only activation with `Ctrl+double-click` or `Ctrl+Enter`, which copies the selected value without closing or auto-pasting
* auto-paste only runs in the effective `AutoClose=true` activation flow
* supports key-based filtering and optional value-based filtering through `SearchValues`, with separate key/value case-sensitivity controls

The app is meant to stay lightweight, explicit, and easy to change locally.

---

## 2) Observed runtime flow

This section reflects the current code in `main.cpp`, `ClipDeckApp.*`, `MainWindow.*`, `SettingsWindow.*`, `ClipListView.*`, `TrayIcon.*`, and `ClipboardUtils.*`.

1. `wWinMain` loads `AppConfig` via `LoadAppConfig()` and constructs `ClipDeckApp`.
2. `ClipDeckApp::Run()` asks `MainWindow` to create/start the main window, then owns the `GetMessageW` loop.
3. `MainWindow::Create()` creates the main HWND, then shows or hides it based on `[App].StartHidden`.
4. On `WM_CREATE`, the main window creates only the snippet-list child controls from `ClipListView`.
5. After window creation, the app adds a tray icon and registers a global hotkey.
6. The hotkey is registered from `[Hotkey].Open` in `settings.txt`.
   Invalid hotkey text shows a message box and does not fall back to a hidden default.
7. Hotkey press captures a foreground/focused window snapshot, then toggles the main snippet window.
8. Tray behavior:

   * left click opens the main window
   * right click shows a context menu with `Open`, `Config`, `Reload`, `Exit`
   * `Reload` reparses config, re-registers the global hotkey, updates the settings-window config, reapplies main-window sizing from config, and relayouts existing `ClipListView` controls
   * `Config` still shows a placeholder message box
9. The main window shows:

   * a `LISTBOX`
   * a filter `EDIT`
   * a settings icon button
10. Typing in the filter box starts a 100 ms debounce timer. Filtering always checks `ClipItem.key`. It also checks `ClipItem.value` when the effective `SearchValues` setting is true. Key matching uses the effective `CaseSensitiveKeys` setting, and value matching uses the effective `CaseSensitiveValues` setting. Each effective search setting is computed independently from the item first, falling back to `[Search]` when the item does not override it.
11. Double-clicking a list item or pressing `Enter` in the list activates the selected item.
12. Holding `Ctrl` while double-clicking or pressing `Enter` activates the selected item in copy-only mode.
13. Activation copies the selected value to the clipboard.
14. In normal activation, the app computes effective `AutoClose` and `AutoPaste` from the selected item first, falling back to `[Activation]` for each missing item field.
15. If effective `AutoClose=true`, the main window hides after normal activation.
16. If effective `AutoClose=true` and effective `AutoPaste=true`, the app attempts to restore the previously focused window and sends `Ctrl+V` via `SendInput`.
17. In copy-only activation, the app ignores `AutoClose` and `AutoPaste`: it does not hide the main window and does not auto-paste.
18. Clicking the settings button makes `MainWindow` request settings opening through `ClipDeckApp`, which owns the separate top-level settings window.
    If it does not exist yet, the app creates it first.
    If it already exists, the app shows it and brings it to the foreground.
19. After opening settings from the main window, the main window hides only when `HideOnBlur=true` and `KeepVisibleWhileConfiguring=false`.
20. When shown from a hidden state, the settings window reloads the raw settings file text into a multiline `EDIT` control.
21. `Save` or `Ctrl+S` writes the raw editor text back to disk, reloads `AppConfig`, and keeps the settings window open.
22. Leaving settings through `Cancel`, `Esc`, or `WM_CLOSE` goes through the dirty-check flow:

   * if not dirty, hide the settings window
   * if dirty, show `Yes / No / Cancel`
   * `Yes` saves, reloads config, then hides the settings window
   * `No` discards editor changes, then hides the settings window
   * `Cancel` keeps the settings window open
23. The main window auto-hides on focus loss only when `HideOnBlur=true`. When the focus loss is caused by opening the settings window, `KeepVisibleWhileConfiguring=true` keeps the main window visible.
24. The settings window is its own top-level HWND and does not depend on mounting or unmounting controls inside the main window.

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
* showing or activating the main window through the app flow focuses the filter textbox
* showing or activating the main window selects the first visible list item when any filtered items are present
* pressing Up/Down while the filter textbox is focused moves the listbox selection without moving focus out of the filter textbox
* printable text typed while the list box has focus is redirected into the filter textbox
* filtering is debounced
* filtering uses effective item-level search settings with `[Search]` fallbacks
* after filtering, the selected underlying item is preserved if it remains visible; otherwise the first visible result is selected when any results are present
* activation is by double click or `Enter` in the list box
* holding `Ctrl` during double click or `Enter` performs copy-only activation: copy to clipboard without closing or auto-pasting
* selected item value is copied to clipboard
* hidden items are still present in the list, but their displayed value is masked as `*****`
* the window uses tool-window style and is hidden from the taskbar
* losing focus hides the window only when the effective `HideOnBlur` setting is true
* when opening settings, `KeepVisibleWhileConfiguring=true` prevents the main window from hiding even if `HideOnBlur=true`

---

### Settings window

**Purpose**

* edit the raw text of `settings.txt`

**Existing controls**

* multiline `EDIT` control with scrollbars
* icon-only `Save` button with tooltip
* icon-only `Cancel` button with tooltip

**Observed behavior**

* settings has its own top-level HWND and lifecycle
* the editor shows the file text as stored on disk
* `Save` writes the raw editor text to disk and reloads config
* `Cancel` means "leave settings", not "clear text"
* dirty state is tracked by comparing current editor text with the original loaded text
* programmatic text loads suppress the dirty flag via `suppressEditChange`
* `Esc` is handled through message pre-translation in the main loop so it works even when focus is inside child controls

**Current limits**

* layout uses hardcoded constants, and `WM_SIZE` calls the layout routine
* `Save` and `Cancel` icon buttons are positioned from the top-left using `[SettingsWindow].Margin`; the editor sits below them and fills the remaining client area with the same margin inset
* size is driven by the separate `[SettingsWindow]` section (`settingsWindowSettings`), and the current layout routine reapplies that configured size
* there is no config validation or parse error feedback before reload

---

## 4) Functional scope

### Implemented

* one main WinAPI top-level window for snippet selection
* one separate WinAPI top-level settings window for raw settings editing
* tray icon with `Open`, `Reload`, and `Exit`
* config-driven global hotkey registration
* searchable key/value list UI
* clipboard copy on item activation
* copy-only activation with `Ctrl+double-click` or `Ctrl+Enter`
* optional auto-close after activation, with per-item `AutoClose` overrides
* optional auto-paste attempt back to the previously focused window, with per-item `AutoPaste` overrides
* raw settings text editor with save/discard confirmation
* config reload after settings save or tray reload
* value search is implemented through effective `SearchValues`, with per-item overrides falling back to `[Search]`
* key/value search case sensitivity is configurable through effective `CaseSensitiveKeys` and `CaseSensitiveValues`

### Partial / limited

* `MainWindow` settings affect main-window size and `ClipListView` layout; user-driven main-window resizing is temporary and not persisted back to `settings.txt`
* settings editing is functional but very basic
* settings save path only reports file-write failure; malformed settings content is not validated for the user
* auto-paste depends on best-effort focus restoration (`SetForegroundWindow`, `SetFocus`, `SendInput`)

### Planned / not implemented

* tray `Config` action opening the real settings window
* stronger config validation and user-facing parse errors

---

## 5) Configuration model

The runtime settings file is `<exe directory>\settings.txt`.
The default copy is stored in `resources/settings.txt` and copied beside the executable by CMake post-build steps.

`AppConfig.*` is the source of truth for what is currently parsed.

Example shape:

```ini
[App]
StartHidden=false

[Hotkey]
Open="Ctrl+Shift+Backtick"

[MainWindow]
Width=400
Height=300
Margin=4
TextBoxMargin=6
HideOnBlur=true
KeepVisibleWhileConfiguring=true

[SettingsWindow]
Width=800
Height=600
Margin=4

[Activation]
AutoClose=true
AutoPaste=false

[Search]
SearchValues=false
CaseSensitiveKeys=false
CaseSensitiveValues=true

[[Item]]
Key="firma"
Value="Cordiali saluti,\nMario Rossi"
Hidden=false
AutoClose=false
AutoPaste=true
SearchValues=true
CaseSensitiveKeys=false
CaseSensitiveValues=true
```

### Current parser behavior

* sections recognized: `[App]`, `[Hotkey]`, `[MainWindow]`, `[SettingsWindow]`, `[Activation]`, `[Search]`, `[[Item]]`
* inline comments beginning with `;` or `#` are stripped when outside quotes
* quoted strings support `\n`, `\"`, and `\\`
* unknown sections and unparseable lines are ignored
* invalid values usually fall back silently to defaults rather than surfacing an error
* empty item-level optional override values are parsed as "no override" for that item field

### Fields currently consumed by app behavior

#### `[App]`

* `StartHidden`: used

#### `[Hotkey]`

* `Open`: used for global hotkey registration

#### `[MainWindow]`

* `Width`, `Height`: used for main-window creation and reload sizing
* `Margin`, `TextBoxMargin`: used by `ClipListView` layout
* `HideOnBlur`: used to decide whether the main window hides when it loses focus
* `KeepVisibleWhileConfiguring`: used when opening the settings window; if true, the main window remains visible while settings is open

#### `[SettingsWindow]`

* `Width`, `Height`: used for settings-window creation and reload sizing
* `Margin`: used by settings layout for top/left button placement, spacing between the buttons, editor inset, and spacing below the buttons

#### `[Activation]`

* `AutoClose`: used as the global default for activation close behavior
* `AutoPaste`: used as the global default for paste behavior, but paste still only runs inside the effective `AutoClose=true` activation branch

#### `[Search]`

* `SearchValues`: used as the global default for value-based filtering
* `CaseSensitiveKeys`: used as the global default for key filter matching case sensitivity
* `CaseSensitiveValues`: used as the global default for value filter matching case sensitivity

`HideOnBlur` and `KeepVisibleWhileConfiguring` work together only for the settings-window flow:

| HideOnBlur | KeepVisibleWhileConfiguring | Main window hides when opening settings |
|---|---|---|
| false | false | false |
| false | true | false |
| true | false | true |
| true | true | false |

Outside the settings-window flow, `HideOnBlur` controls normal focus-loss hiding by itself.

#### `[[Item]]`

* `Key`: used for display and filtering
* `Value`: used for clipboard/paste output and, when effective `SearchValues=true`, filtering
* `Hidden`: used only to mask the visible display text
* `SearchValues`: optional per-item override for `[Search].SearchValues`
* `CaseSensitiveKeys`: optional per-item override for `[Search].CaseSensitiveKeys`
* `CaseSensitiveValues`: optional per-item override for `[Search].CaseSensitiveValues`
* `AutoClose`: optional per-item override for `[Activation].AutoClose`
* `AutoPaste`: optional per-item override for `[Activation].AutoPaste`

Missing or empty item-level `SearchValues`, `CaseSensitiveKeys`, `CaseSensitiveValues`, `AutoClose`, and `AutoPaste` fields fall back independently to `[Search]` or `[Activation]` as appropriate.
Filtering always checks `ClipItem.key`. It also checks `ClipItem.value` when the effective `SearchValues` setting is true. Key matching uses the effective `CaseSensitiveKeys` setting, and value matching uses the effective `CaseSensitiveValues` setting. Each effective search setting is computed independently from the item first, falling back to `[Search]` when the item does not override it.
On activation, the selected value is always copied first. The effective `AutoClose` value then decides whether the main window hides. The effective `AutoPaste` value decides whether paste is attempted, but only inside the effective `AutoClose=true` flow.

### Important note about settings editing

The settings window edits the raw settings file text, not a structured form.

That means:

* save does not validate syntax before writing
* reload reparses whatever can be parsed
* future AI changes must keep the distinction between raw text editing and structured config parsing explicit

---

## 6) Current product state

The current application is usable for the main snippet-selection flow and for raw settings editing.

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
* config validation and user-facing error reporting
* resize / relayout behavior

---

## 7) Architecture principles

### Goals

* keep WinAPI usage explicit
* keep feature edits local
* reduce regressions in the main selection flow
* make small AI-assisted changes predictable

### Practical design rules for this repo

* `ClipDeckApp` should own application lifetime, the top-level message loop, top-level window objects, and app-level config reload coordination
* `MainWindow` should orchestrate the snippet-list window, tray actions, hotkey events, and item activation
* `MainWindow` should request app-owned settings opening and app-level config reload through callbacks
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
* constructs and runs `ClipDeckApp`

### `src/ClipDeckApp.*`

* owns application startup/lifetime wrapper state
* stores the initial `HINSTANCE` and canonical `AppConfig`
* owns `MainWindow`
* owns `SettingsWindow`
* owns the top-level `GetMessageW` loop
* delegates startup and main-window message pre-translation to `MainWindow`
* delegates settings message pre-translation to `SettingsWindow`
* coordinates config reload into both windows

### `src/MainWindow.*`

* owns the main snippet-window HWND
* handles `WndProc` dispatch for that window
* creates main-list controls through `ClipListView`
* coordinates tray actions, hotkey events, and item activation
* reads the app-owned `AppConfig` and applies current values to main-window state
* requests app-level config reload through a callback
* requests settings-window opening through a callback
* provides startup and message pre-translation hooks to `ClipDeckApp`
* does not own `SettingsWindow`

### `src/SettingsWindow.*`

* owns the separate settings HWND
* creates the multiline `EDIT`, `Save`, and `Cancel` controls
* loads raw settings text from disk
* writes raw settings text back to disk
* handles dirty-check flow and hide/close behavior
* notifies `ClipDeckApp` after successful save so config can be reloaded

### `src/ClipListView.*`

* creates main-list child controls
* owns filter debounce timer behavior
* populates and filters the list box
* exposes selection and settings-button events back to `MainWindow`

### `src/AppConfig.*`

* locates runtime files relative to the executable
* parses `settings.txt`
* decodes quoted values and escaped characters
* returns the in-memory config model

There is still no separate save/validate API here; raw-text settings save writes directly to disk in `SettingsWindow.cpp`.

### `src/ClipboardUtils.*`

* captures current focus targets before overlay activation
* copies Unicode text to the Windows clipboard
* attempts paste-back via focus restore + `SendInput`

### `src/HotkeyParser.*`

* parses `[Hotkey].Open` text into `RegisterHotKey` generic modifier flags and a WinAPI virtual-key code
* accepts side-specific modifier names such as `LCtrl`, but maps them to generic `RegisterHotKey` flags because WinAPI does not support side-specific global hotkey modifiers
* accepts `NumpadEnter` as `VK_RETURN` because `RegisterHotKey` has no distinct numpad-enter virtual-key constant

### `src/GlobalHotkey.*`

* thin wrapper around `RegisterHotKey` / `UnregisterHotKey`

### `src/TrayIcon.*`

* tray icon add/remove
* tray callback translation
* tray context menu creation and command mapping

### `resources/resource.h` / `resources/resource.rc`

* icon resource identifiers and icon bundle declarations

### `resources/settings.txt`

* default sample settings file copied to the output directory after build

### `docs/AI_CONTEXT.md`

* AI-facing repository context
* should be updated when implemented behavior or module boundaries materially change

---

## 9) Recommended module boundaries

Keep these boundaries unless the task explicitly improves one of them:

* main-list control behavior belongs in `ClipListView.*`
* raw config parsing belongs in `AppConfig.*`
* separate raw settings editing behavior belongs in `SettingsWindow.*`
* snippet-window orchestration belongs in `MainWindow.*`
* clipboard / paste behavior belongs in `ClipboardUtils.*`
* tray menu behavior belongs in `TrayIcon.*`

If you introduce a new module, it should remove a concrete overload from an existing file, not add abstract architecture.

---

## 10) State and behavior expectations

Important current state includes:

* captured foreground/focused window snapshot in `lastFocus_`
* full config model in `ClipDeckApp::config_`
* visible filtered item index mapping in `ClipListView`
* settings editor state in `SettingsWindow`:

  * `originalText`
  * `isDirty`
  * `suppressEditChange`

Behavior expectations future edits should preserve unless the task explicitly changes them:

* hotkey toggles the main snippet window
* tray left click opens the main window
* main window auto-hides on focus loss only when `HideOnBlur=true`
* opening settings keeps or hides the main window according to `HideOnBlur` and `KeepVisibleWhileConfiguring`
* settings dirty-check blocks accidental leave when user chooses `Cancel`
* settings `Save` writes raw text first, then reloads config
* item activation always copies first; paste is additional behavior
* copy-only activation must ignore `AutoClose` / `AutoPaste` and keep the main window open

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

* tray menu `Config` exists but only shows `Config option not implemented yet.`
* settings save writes raw text without validation; malformed settings can silently reload as partial defaults
* main-window resize is handled locally for `ClipListView`, but resized dimensions are still temporary and never persisted to `settings.txt`
* settings-window `WM_SIZE` calls the layout routine, but that routine reapplies `[SettingsWindow]` configured size, so user-driven resizing is not currently preserved or persisted to `settings.txt`
* file I/O uses explicit UTF-8 write output with UTF-8/ACP read fallback, but there is still no user-facing encoding validation

---

## 14) Near-term priorities

1. Improve settings robustness: validation/error reporting before reload, and clarify desired raw-text editing behavior.
2. Replace or remove the tray `Config` placeholder entry.
3. Clarify or improve resize behavior for the settings window.

---

## 15) Feature roadmap notes

These are directionally consistent with the current codebase, not commitments.

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

ClipDeck is a small WinAPI snippet inserter with:

* one main snippet HWND
* one separate settings HWND
* tray + hotkey entry points
* raw-text settings editing
* a narrow set of source modules

Future AI work should stay local, preserve the current flow, and prefer code reality over old prompts or aspirational notes.
