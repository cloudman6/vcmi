# RFC draft: Native controller UI for VCMI

Status: Internal draft and sequence candidate. It has not passed the evidence
and integration gates described below and is not approved for publication. This
document proposes a direction and review sequence; it does not freeze a public
API.

Source baseline: VCMI commit
[`4cb465b3522018517d138c38fc9ab9db00e4d25b`](https://github.com/vcmi/vcmi/commit/4cb465b3522018517d138c38fc9ab9db00e4d25b).

## Summary

VCMI already accepts SDL game-controller input, tracks the active input mode,
and maps controller buttons and axes to `EShortcut`. The missing part is a
native controller interaction model for menus, modal windows, the adventure
map, battle targeting, and complex panels. Today, virtual mouse and existing
shortcuts cover only part of those workflows.

This RFC proposes an evidence-first, incremental implementation:

- keep `InputHandler`/`InputMode` as the sole input-mode owner and the existing
  binding configuration plus `ShortcutHandler` as the sole physical
  binding/remap owner;
- add only the minimum focus lifecycle and navigation contracts required by a
  real UI consumer;
- use different navigation strategies for widgets, adventure-map objects,
  battle hexes, and inventory-like containers;
- keep the SDL client and Qt launcher as separate runtime integrations;
- deliver independently useful vertical pull requests with automated tests,
  hardware results, and keyboard/mouse regression evidence.

The names used below, including *focus scope*, are working terminology. The
spikes and maintainer review should determine the final interfaces.

## Motivation

The intended result is a controller-only path through normal play, not merely
pointer emulation. A player should eventually be able to start or load a game,
navigate the adventure map, complete battles, use towns and hero panels, save,
and exit without switching to a mouse or keyboard.

The first upstream changes should be much narrower. Their purpose is to prove
the lifecycle, input, and testing contracts on real screens without committing
VCMI to a large controller-specific framework.

### Final user acceptance contract

The final result is accepted only through a fixed journey, starting from a
cold launcher after legal game data has already been configured:

1. Start the client, then load the fixed non-commercial
   `controller-e2-load-win-v1` fixture.
2. Complete adventure movement, the N1 legal-build path, hero management,
   troop splitting, spell use, battle, and required modal interactions.
3. Name a save, confirm overwrite, exit, restart, and observe restored state.
4. Complete the fixture's deterministic win flow and exit.

Within this scope the required keyboard/mouse invocation count is `0`. Each run
also records cursor-fallback count, blockers, mistakes, recovery time, final
result, and success rate. Early pull requests deliver only the named task in
their slice and must not claim that this journey is complete.

## Current VCMI baseline

The following observations are tied to the source baseline above.

### Existing SDL input path

- [`InputHandler`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/eventsSDL/InputHandler.h)
  owns `InputMode`, including `CONTROLLER`, and dispatches input-mode changes.
- [`InputSourceGameController`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/eventsSDL/InputSourceGameController.cpp)
  handles SDL controller devices, buttons, axes, dead zones, and virtual
  pointer movement.
- [`ShortcutHandler`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/gui/ShortcutHandler.cpp)
  translates configured keyboard and controller inputs to `EShortcut`.
- [`WindowHandler`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/gui/WindowHandler.cpp)
  already supplies the push, deactivate, pop, and reactivate lifecycle that a
  modal focus contract must respect.

This path should be extended, not bypassed. A second global input-mode owner or
a second controller binding table would make remapping, glyphs, and behavior
diverge.

### Existing scene actions

- [`AdventureMapShortcuts`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/adventureMap/AdventureMapShortcuts.cpp)
  centralizes many adventure-map actions with enabled predicates and callbacks.
- [`MapViewActions`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/mapView/MapViewActions.cpp)
  maps pointer and gesture positions to adventure-map tiles, but does not own a
  controller-selected object or tile.
- [`BattleActionsController`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/battle/BattleActionsController.cpp)
  owns possible actions, legality checks, multi-step targeting, and action
  realization.
- [`BattleFieldController`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/battle/BattleFieldController.cpp)
  derives the current hex and attack direction from pointer position. It does
  not expose an independent controller-selected hex.

Controller navigation should select or preview a target, then call the existing
scene action and legality paths. It must not duplicate pathfinding, battle
legality, spell targeting, or server-authoritative game rules.

### Known baseline defect

The controller trigger threshold is read as `controllerTriggerThreshold` in
[`InputSourceGameController.cpp`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/client/eventsSDL/InputSourceGameController.cpp#L42),
while the settings schema uses `controllerTriggerTreshold` in
[`config/schemas/settings.json`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/config/schemas/settings.json#L391).
This should be corrected and covered by a configuration regression test before
larger input changes rely on the setting.

### SDL client and Qt launcher boundary

The game client receives input through SDL and VCMI's client event system. The
launcher starts its own `QApplication` in
[`launcher/main.cpp`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/launcher/main.cpp)
and uses Qt Widgets and a separate event loop.

Desktop builds normally provide separate launcher and client targets and
processes. Android and iOS use a single-app build with an activity or sequential
Qt-to-SDL handoff, as defined by the top-level
[`CMakeLists.txt`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/CMakeLists.txt),
[`clientapp/CMakeLists.txt`](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/clientapp/CMakeLists.txt),
and `launcher/main.cpp`.

Neither form may share live input handlers, window stacks, focus objects, or
input/focus truth across the runtime boundary. They may share pure data
definitions only after a concrete use case shows that doing so is useful. Qt
changes still have independent scope, review, and validation; mobile build
evidence may come from the composed app target. The existing open request for
minimal launcher support is [issue #4111](https://github.com/vcmi/vcmi/issues/4111).

## Goals

- Preserve keyboard, mouse, touch, and existing shortcut behavior.
- Make controller bindings remappable and derive displayed controls from the
  active binding and device profile.
- Give modal windows deterministic initial focus, containment, cancellation,
  invalidation, and restoration.
- Support scene-appropriate navigation rather than forcing every screen into a
  static widget graph.
- Keep unavailable actions inspectable when a useful reason can be shown, while
  preventing their submission.
- Revalidate focus and action legality after asynchronous server results or UI
  reconstruction.
- Provide a reliable cursor fallback for unadapted or third-party custom UI,
  without using pointer emulation as the primary experience.
- Keep one clear primary focus, never communicate focus by color alone, and
  keep required actions available without hold-only or chord-only input.
- Allow HUD and text scaling independently, reduce focus animation and camera
  snapping, and keep prompts consistent with the effective binding and device.
- Keep critical actions and unavailable reasons readable with long and
  localized strings without glyphs obscuring the text.
- Make each upstream pull request independently demonstrable and reversible.

## Non-goals

- Patching or injecting the original Heroes III executable.
- Replacing native controller UI with a Steam Input layout.
- Rewriting all existing VCMI windows into a second controller-only UI tree.
- Changing game rules or client/server authority to support navigation.
- Freezing one universal focus graph before widget, map, battle, complex-panel,
  and launcher prototypes have exercised it.
- Full native navigation for every third-party mod UI in the first delivery.
- Multi-controller hot-seat ownership in the first delivery.
- Distributing commercial Heroes III data, screenshots, saves, or other assets
  as public test fixtures.
- First installation, purchase verification, or commercial-data import as part
  of the controller-only acceptance journey.
- Promising one default physical button layout or bundling glyph artwork before
  its license and maintainer direction are established.
- Treating cursor fallback as completion evidence for adventure, battle, town,
  hero/inventory, troop transfer, or spellbook native navigation.
- Text fields outside the explicitly supported screen-keyboard or IME matrix.
  Such fields must remain clearly out of scope or expose an explicit keyboard
  fallback; they must never trap a controller-only user in an input state.

## Proposed design constraints

### One input and binding source of truth

Physical controller input continues through `InputSourceGameController`,
`InputHandler`, `ShortcutHandler`, and `EShortcut`. Navigation commands should
use the existing binding mechanism wherever it can represent the required
semantics.

The ownership chain is explicit:

- `InputHandler`/`InputMode` owns the active input mode;
- the existing binding configuration and `ShortcutHandler` own physical
  bindings and remapping;
- `InputSourceGameController` adapts SDL device events;
- mapping proceeds from physical input to configured `EShortcut`, then to an
  optional contextual UI command only when the prototypes prove a gap.

If prototypes show that `EShortcut` cannot express contextual focus movement or
multi-step UI states, a separate semantic command type may be proposed. Such a
type must still have a one-way mapping from the existing binding source; it must
not own physical bindings or input mode.

Glyph lookup needs the reverse direction: a visible action should resolve to
its source `EShortcut`, then the currently effective binding and device
profile. A contextual UI command cannot carry or own a physical binding.
Hard-coded Xbox-style button labels are not acceptable.

### Focus lifecycle contract

A minimal focus scope should define:

- ownership by the active window, modal, panel, or scene overlay;
- a stable identifier for initial focus and restoration;
- suspend and resume behavior across `WindowHandler` push/pop;
- deterministic fallback when the focused item is destroyed or hidden;
- focus containment so a modal cannot leak navigation to the scene below;
- separate states for hidden, non-focusable, enabled, disabled with a reason,
  and pending;
- confirm, cancel, inspect, and directional navigation hooks.

This contract should not contain adventure-map search or battle legality. It
coordinates lifecycle and observable state; scene adapters retain their own
selection algorithms.

### Navigation strategies

| UI family | Candidate navigation | Existing behavior to reuse |
|---|---|---|
| Buttons, lists, and modal windows | Explicit or computed widget neighbors | `WindowHandler`, widget activation, shortcuts |
| Adventure map | Directional object query plus explicit tile-cursor fallback | `AdventureMapShortcuts`, tile hover/click semantics |
| Battle | Legal hex navigation plus explicit attack-direction selection | `BattleActionsController`, battle callbacks |
| Spellbook and inventory-like panels | Grid, slot, page, filter, and quantity navigation | Existing window models and action callbacks |
| Unknown custom UI | Clearly indicated cursor fallback | Existing virtual pointer path |

Only the lifecycle and command-state protocol should be common. Candidate
storage and direction scoring remain specific to each UI family.

### Server-authoritative actions

Focus and presentation code may expose whether a command is enabled, why it is
disabled, and whether it is pending. It must submit gameplay changes through
existing controllers and callbacks. Revalidation is triggered only by existing
server-driven state/event updates or existing error feedback. On that trigger,
the current target, legality, focus, and default action are re-queried from the
scene owner.

The common command-state protocol is a non-owning projection for the current
consumer and target. It may expose enabled, reason, pending, submit, and
invalidate hooks, but it does not own legality, candidates, multi-step target
state, or a global command bus/store. Those remain with the existing scene
owners, including `AdventureMapShortcuts`, `AdventureMapInterface`, and
`BattleActionsController`.

Accepted, rejected, and superseded are UI-adapter result classifications; this
RFC does not require new server wire values, response bus, or game-state store.
After a callback or observed state change, the adapter re-queries the scene
owner instead of caching a second game truth. Pending suppresses duplicate
submission.

### Cursor fallback contract

Cursor fallback is for unadapted, third-party, or recovery UI. It has a
deterministic entry, a persistent visible mode indicator, and defined primary
click, secondary click, scroll, drag, modal, and screen-edge behavior. Exiting
restores the navigable item under the cursor, then the semantic position held
before entry, then the active scope's default item.

Switching between focus and cursor states must not close a modal, submit an
action, or mutate game state. A cursor-fallback PASS cannot replace native
navigation acceptance for the core UI families named in Non-goals.

### Input-mode and device-transition contract

Axis or trigger signals, and physical pointer movement that uses a threshold,
may take over input mode or drive navigation only after crossing their
respective configured thresholds. A discrete controller button is evaluated on
the actuation edge named by its evidence case through its current effective
binding, without an axis or pointer threshold. Each discrete-button case records
the initial input mode and prompt, physical input and effective binding,
actuation edge, expected semantic action, and expected post-edge mode and
prompt; those expected values are its takeover oracle. Dead-zone noise and
insignificant pointer movement must not cause mode flapping. A mouse click may
take over at the clicked semantic item without producing a ghost confirm or
changing unrelated business state. High-risk actions retain their existing
confirmation boundaries.

Disconnect must not confirm, cancel, or submit. Reconnect restores the same
semantic position only if it remains legal, otherwise the active scope chooses
an explainable default. A binding change updates both behavior and its glyph
from the same source of truth before the next action.

### Accessibility and localization outcomes

At any moment one primary focus is visually unambiguous without relying on
color alone. HUD and text scaling are independent. Long strings and
pseudolocalized content keep required actions and unavailable reasons readable;
focus and glyph presentation must not cover the text. Focus animation and
camera snapping can be reduced, and every required hold or chord has a
single-input or toggle alternative.

The oracle is task completion, not a screenshot. Applicable acceptance runs
cover handheld, desktop, and television viewing conditions, with long and
non-English strings. Screenshots only support the recorded task result.

## Evidence phase before API freeze

Five small prototypes should test the highest-risk assumptions:

1. A modal SDL window: directional navigation, confirm/cancel, disabled reason,
   nested modal containment, restoration, and mouse-click takeover.
2. Adventure map: object candidates, tile fallback, direction scoring, path
   preview, off-screen targets, disappearing objects, and camera interaction.
3. Battle: hex focus, double-wide units, multiple legal attack origins, explicit
   attack direction, spell targets, and post-action revalidation.
4. A complex panel such as the spellbook or hero exchange: paging, filtering,
   scrolling, source/target selection, quantities, and invalid destinations.
5. Qt launcher: controller event source on supported Qt versions, the minimal
   Start action, native widget focus, modal behavior, and build dependencies.

Each prototype should report the input sequence, reused VCMI interfaces,
observable result, failure conditions, and whether it introduced duplicated
state. Prototype code need not be mergeable. Its purpose is to reject bad API
assumptions before production changes are reviewed.

## Proposed pull request sequence

The sequence below is conditional on prototype and maintainer feedback. A shared
infrastructure change may be a separate commit, but it should be reviewed with
the first real consumer that needs it.

The controller threshold schema fix is a standalone baseline bug fix, not the
first member of a conditional production stack. It should land with schema and
round-trip coverage before the prototypes rely on the setting. The modal slice
does not depend on it unless implementation or test evidence shows a direct
dependency.

After the evidence phase, the candidate SDL graph is:

```text
standalone threshold bug fix

modal consumer + minimal lifecycle
  |
  +-- standard-widget second consumer
  |     `-- extract only proven widget behavior
  |
  +-- battle targeting slice
  |     `-- battle command and resolution completion
  |
  `-- adventure selection slice
        `-- adventure turn completion

independent container consumers
  +-- spellbook page/grid/filter slice
  +-- hero/equipment slot/backpack slice
  `-- troop transfer/split source-target/quantity slice
        `-- extract an exact repeated contract only after two consumers prove it

town building/recruitment slice
  `-- minimal lifecycle/command-state plus exact proven widget/container contracts;
      building-area candidates and geometry remain town-local

standalone text entry
  `-- SDL end-to-end closure (no first-use feature work)
```

Battle and adventure are sibling slices on the smallest proven lifecycle and
command-state baseline. They may be reviewed in either order, but must not have
a code dependency on each other or share a scene-navigation data model.

| Slice | Independently useful result | Main dependency | Required evidence |
|---|---|---|---|
| B0. Baseline threshold bug fix | The user's configured trigger threshold takes effect, preventing ignored settings and unintended activation | None | Schema and round-trip regression; real-controller boundary behavior; keyboard/mouse smoke |
| W1. Modal-window controller slice | One maintainer-approved existing modal, named before implementation, completes initial focus, unavailable-reason inspection, nested isolation, one-layer cancel, close/restore, and mouse takeover | Prototype-supported minimal lifecycle contract | Lifecycle tests; real controller recording; keyboard/mouse and conditional touch regression |
| W2. Standard-widget second consumer | One existing main-menu or save/load window flow proves repeated widget lifecycle and navigation behavior | W1; no scene-navigation dependency | Consumer integration tests; modal and pointer regression; exact repeated contract identified |
| B1. Battle targeting slice | One representative targeting task selects and commits a legal target, then recovers from invalidation or a server result and exits safely; it does not claim complete battle control | Proven minimal lifecycle and command state only | Pure hex tests; battle integration test; double-wide and multi-origin manual cases |
| B2. Battle command and resolution completion | Named battle flows add Wait, Defend, Inspect, autocombat, spell/multi-step targets, Retreat/Surrender, and result-modal handling without weakening B1 | B1 and existing battle legality/action paths | Command-state and server tests; one complete battle journey; safe cancellation and exit |
| A1. Adventure-map selection slice | One representative map task browses targets, previews and commits a path, recovers from invalidation or a server result, and exits safely; it does not claim complete adventure control | Proven minimal lifecycle and command state only | Direction-scoring tests; path/action reuse evidence; ten-turn manual script |
| A2. Adventure turn completion | A controller can switch heroes/towns, handle overlapping or off-screen objects, complete confirmed movement, and end a turn with safe invalidation recovery | A1 and existing adventure action paths | Turn-state tests; pointer/gesture regression; complete turn script |
| W3. Additional named widget consumer and extraction | A named new standard-window task becomes controller-complete while applying only behavior repeated by W1 and W2 | W1 and W2; not B1 or A1 | New consumer task and both prior consumers pass; touch coverage; no framework-only PR |
| C1. Spellbook container slice | Search, categories, disabled reasons, inspect, target entry, and restoration work with a controller | Its own complex-panel prototype and consumer-local behavior | Filter/state tests; large spell-list manual case; localization screenshots |
| C2. Hero and equipment slice | Equipment, backpack, and hero exchange work without drag-only input | Its own prototype; no C1 dependency unless two consumers prove one exact repeated contract | Slot and illegal-target tests; keyboard/mouse equipment and drag regression |
| C3. Troop transfer and split slice | Transfer, exchange, merge, split, and quantity selection form one complete journey | Its own prototype; no C1/C2 dependency unless two consumers prove one exact repeated contract | Quantity boundary tests; full/illegal destination cases; keyboard/mouse drag regression |
| N1. Town building and recruitment slice | One town visit can inspect state, complete both a legal build and a legal recruit, explain unavailable actions, and return to the map | Minimal lifecycle/command-state plus exact matching proven widget/container contracts; town-local building candidates; existing town legality/action paths | Building and recruitment state tests; server-response cases; keyboard/mouse regression |
| W4. Save/load or main-menu remainder | The standard window flow not selected for W2 becomes independently complete | Proven widget behavior only | Flow integration test; overwrite/cancel or menu-transition regression as applicable |
| T1. Standalone text entry | The agreed naming and text fields work through platform IME or screen-keyboard boundaries | Platform-specific spike and explicit scope | IME/screen-keyboard matrix; cancel/restore; keyboard text regression |
| E1. SDL end-to-end closure | The agreed SDL client journey is complete from its main menu | All required SDL feature slices | Fixed integration SHA; save/reload; zero required keyboard/mouse actions; no new feature first appears here |

Widget adjacency cannot be justified by battle or adventure consumers. Common
widget code should be extracted only after the modal and a second actual widget
flow demonstrate the same contract. Spellbook, hero/equipment, and troop
transfer are sibling vertical slices by default. A helper used by only one
remains consumer-local. A dependency or shared extraction appears only after
two real consumers prove the same observable page/grid, slot/source-target, or
quantity contract.

### Independent Qt launcher track

| PR | Independently useful result | Required evidence |
|---|---|---|
| L1. Minimal Start action | A controller can activate Start in the launcher, addressing the minimum requested in issue #4111 without claiming complete launcher navigation | Qt5/Qt6 build result as applicable; controller and keyboard activation smoke |
| L2. Launcher navigation | The agreed startup and configuration path has native Qt focus, cancel, modal restoration, unavailable reasons, visible prompts, and explicit text-entry boundaries | Page/modal/text-entry matrix; controller hardware results; keyboard/mouse regression |

Launcher changes keep independent scope, review, and validation from SDL client
focus changes. Desktop evidence uses the independent launcher/client targets;
mobile evidence may use its composed app target while still proving the runtime
handoff and absence of live shared focus/input state. Shared code requires a
demonstrated pure-data or pure-algorithm use in both runtimes. L1 may proceed in
parallel with the SDL sequence. L2 is not a dependency of an SDL pull request,
but it must be included before claiming a complete controller-only journey that
begins at a cold launcher start.

E2 is the final composed acceptance candidate, not a feature pull request. It
combines L2 and the accepted SDL slices at immutable SHAs and executes the full
cold-launcher journey. No behavior may appear for the first time in E2.

## Executable evidence contract

Evidence is reproducible only when a third party can replay it against the same
candidate and reach the same judgment. A green CI summary, a controller video,
or a prose completion claim is not sufficient by itself.

### Gate terms and pass criteria

- **M (Mandatory):** the evidence item must be present and pass for the slice.
- **C (Conditional):** the item becomes Mandatory when its stated diff or
  behavior trigger is present. Otherwise the evidence manifest must record
  `N/A` with a diff-based reason.
- **N/A:** the item is outside the fixed slice by construction; the reason is
  stated in the matrix. A blank cell is invalid.

All Mandatory items and all triggered Conditional items must report `PASS`.
Any `FAIL`, missing artifact, dirty source tree, SHA mismatch, unclassified row, or
unjustified `N/A` rejects the candidate. Screenshots and recordings may prove a
visible state but cannot replace assertions, raw logs, or a state-transition
ledger. CI proves only the automated surface it ran.

Touch is Mandatory when a change affects common widgets, event routing, or a
control shared with touch. Otherwise it requires an explicit diff-based `N/A`.
Every behavioral slice requires the current development platform and one real
controller. B1 and A1 use their declared slice rows; their separate
cross-platform milestone-promotion gate is defined below. The matrix need not
be a full Cartesian product, but every publicly claimed combination requires
its own evidence row.

Any slice that changes controller dispatch, input-mode presentation, or binding
resolution must replay threshold/debounce noise, controller disconnect and
reconnect, mouse-click takeover, remap, and glyph refresh. A mode flap, ghost
confirm, implicit cancel, duplicate submission, stale semantic restoration, or
binding/glyph mismatch is a failure.

### Per-slice applicability matrix

| Slice | Automation | Keyboard/mouse | Touch | Real controller | Server cases | Config migration | Locale/display | Cross-platform or E2E |
|---|---|---|---|---|---|---|---|---|
| B0 | M | M | N/A: no UI | M: threshold boundary | N/A: no request | M | N/A: no UI | M: repository CI |
| W1 | M | M | C: shared widget/event trigger | M | C: modal submits request | C: new setting | C: prompt/layout change | C: claimed platforms |
| W2 | M | M | C: shared widget/event trigger | M | C: selected flow submits request | C: new setting | M | C: claimed platforms |
| B1 | M | M: hover/click | C: shared battle input | M | M | C: new setting | C: new prompt/layout | C: claimed-platform slice rows |
| B2 | M | M: added battle commands | C: shared battle input | M | M | C: new setting | M: reasons/result modal | M: complete battle journey |
| A1 | M | M: map pointer | M: existing map gestures | M | M | C: new setting | C: new prompt/layout | C: claimed-platform slice rows |
| A2 | M | M: map shortcuts/pointer | M: existing map gestures | M | M | C: new setting | M: reasons/turn state | M: complete adventure turn |
| W3 | M: two consumers | M: two consumers | M | M | C: either consumer submits request | C: new setting | C: changed layout | C: claimed platforms |
| C1 | M | M | C: touched spellbook controls | M | M: spell target path | C: new setting | M: long text and non-English | C: claimed platforms |
| C2 | M | M: equipment/drag | C: shared controls | M | M: exchange/equip path | C: new setting | C: changed text/layout | C: claimed platforms |
| C3 | M | M: drag/quantity | C: shared controls | M | M: transfer/split path | C: new setting | C: changed text/layout | C: claimed platforms |
| N1 | M | M: town pointer/shortcuts | C: shared controls | M | M: build/recruit path | C: new setting | M: resource text and layout | C: claimed platforms |
| W4 | M: includes save compatibility | M | C: shared widget/event trigger | M | C: selected flow submits request | C: settings change only | M | C: claimed platforms |
| T1 | M: automation or replayable platform test | M: keyboard text | C: shared text control | M | C: text commits server action | C: new setting | M: IME and locale | M: every claimed OS/input method |
| E1 | M: fresh closure run plus constituent PASS references | M: target count is zero | C: every constituent touch trigger | M | M: fresh journey cases plus constituent references | M: constituent references | M | M: fixed SDL journey and platform rows |
| L1 | M: clean build and launch guard | M: keyboard Start | C: shared Qt control/event routing | M | N/A: no game-server request by construction | C: diff touches settings | C: changed prompt | M: every declared Qt-major/OS pair |
| L2 | M: automation or replayable UI matrix | M | C: shared Qt touch control | M | N/A: launcher has no game request | C: new setting | M | M: every declared Qt-major/OS pair |
| E2 | M: fresh closure run plus every constituent PASS bundle | M: fresh journey counts | C: every constituent touch trigger | M: fixed final matrix | M: fresh journey cases plus constituent references | M: constituent references | M | M: fixed cold-launcher journey |

Slice acceptance and milestone promotion are separate gates. B1 and A1 use the
Conditional claimed-platform rows above for their own pull requests. Promotion
of the corresponding battle or adventure milestone additionally requires
fresh Mandatory smoke rows on macOS with an external controller, Windows with
an external controller, desktop Linux with an external controller, and Steam
Deck with its integrated controller. These promotion rows do not retroactively
change the slice's M/C/N/A classification.

Before L1 or L2 evidence is generated, its manifest declares the exact
OS/Qt-major matrix. Every declared pair is Mandatory; an undeclared pair is
`N/A` with a reason and cannot enter a support claim.

### Slice-specific mandatory cases

#### B0 controller threshold

The proposed compatibility contract is:

Migration runs on the assembled settings object before schema maximize and
validation can remove the legacy key. It converts legacy to canonical according
to the table below, replaces invalid input with the default before any consumer
reads it or persistence writes it, then validates against a canonical schema
whose numeric range is `[0, 1]`. `InputSourceGameController` reads only the
canonical result. Validation warnings alone are not the migration mechanism.

| Input | Required result |
|---|---|
| Neither key present | Use schema default `0.3` |
| Canonical valid, legacy absent | Use the canonical value |
| Canonical absent, legacy valid | Use the legacy value in memory; on the next settings serialization write it as canonical and remove legacy |
| Canonical valid, legacy valid or invalid | Canonical wins; on the next serialization preserve its value and remove legacy |
| Canonical invalid, legacy valid or invalid | Canonical presence wins but its value is rejected; use `0.3`, then write canonical `0.3` and remove legacy on the next serialization |
| Canonical absent, legacy invalid | Reject legacy, use `0.3`, then write canonical `0.3` and remove legacy on the next serialization |
| Both invalid | Use the canonical-invalid rule above |

Valid means a JSON number in `[0, 1]`. Invalid file cases include below/above
range and representative JSON bytes for `null`, string, Boolean, array, and
object at each key. `NaN` and infinities are not JSON byte cases; if a
programmatic configuration ingress can carry them, an injected test rejects
them using the same invalid rule.

The serialization case is replayed as: load a fixed input fixture, assert the
in-memory value, invoke the normal settings serialization once, inspect the
serialized JSON, reload it, and assert the same value. The expected output has
one canonical key, no legacy key, and no invalid value. Schema-recognized
unrelated settings in the same file remain unchanged. Unknown-key behavior
follows the existing settings maximize/minimize contract and is recorded; this
RFC does not promise preservation of arbitrary unknown keys. Fixture bytes,
pre/post hashes, the serialization trigger, and the exact expected JSON are
evidence fields.

Automation covers the table, numeric bounds, and load/write/load round-trip.
Hardware evidence sets threshold `0.50`, records controller GUID and axis, raw
SDL values, the implementation's normalized values, and device quantization
tolerance, then supplies one stable sample in `[0.48, 0.49]` with no trigger
event and one in `[0.51, 0.52]` with exactly one event. Keyboard/mouse smoke is
Mandatory; server evidence is explicitly N/A.

#### W1 modal lifecycle

State-machine and integration coverage must include initial focus,
push/suspend, nested modal, pop/restore, hidden or destroyed fallback, disabled
reason, confirm, cancel, event containment, and mouse-click takeover. The same
existing modal must have step-by-step controller, keyboard, and mouse scripts.
Touch is Mandatory if common widget or event routing changes. If the modal sends
a game request, the complete server response matrix below is Mandatory;
otherwise the manifest records why it is N/A.

#### W2 and W3 widget consumers

W2 must exercise one real main-menu or save/load flow as the second widget
consumer. W3 must name and complete another real standard-window task; it may
extract a contract only when W1 and W2 both identify and test the same behavior.
All affected consumer lifecycle suites must pass after extraction. Mock-only
framework tests cannot release W3, and an extraction-only change stays a
stacked foundation commit reviewed with the named consumer rather than an
independent pull request. Keyboard/mouse, touch, input-mode switching,
disconnect, and reconnect are Mandatory for W3.

#### B1 battle targeting

Pure tests must cover deterministic hex movement, tie-breaking, double-wide
units, multiple attack origins, attack direction, spell targets, and invalid
targets. Integration evidence must show calls into existing legality and action
paths. The full server response matrix is Mandatory, including prevention of a
second submission and recomputation of target, default action, legality, and
focus. Existing mouse hover/click is a Mandatory regression path.

#### B2 battle command and resolution completion

B2 covers named user flows for Wait, Defend, Inspect, autocombat, spell and
other multi-step targets, Retreat or Surrender confirmation, cancellation, and
the result modal. Each command uses existing battle legality and action paths,
exposes a reason when a required action is unavailable, and applies the server
response matrix. The complete-battle script proves a safe exit after success,
rejection, or a target becoming invalid.

#### A1 adventure selection

Pure tests must cover direction scoring and ties, off-screen candidates, object
removal, camera movement, tile fallback, and path preview. Integration evidence
must show reuse of existing path and action semantics. The full server response
matrix is Mandatory for interactions. Existing map pointer and touch/gesture
paths are Mandatory regressions. The ten-turn script must list every action,
expected state, actual state, and failure condition.

#### A2 adventure turn completion

A2 adds the named remainder for a complete adventure turn: hero and town
switching, confirmed movement, overlapping and off-screen candidates, object
removal, and End Turn. It reuses existing adventure shortcuts and path/action
semantics. Automation and the replayable turn script cover invalidation, server
results, safe cancellation, and a deterministic exit without claiming that A1
alone completes adventure navigation.

#### C1 spellbook

Automation and replayable cases must cover categories, search, filtering, a
large list, disabled reasons, inspect, target entry, cancel, and restoration.
At least one non-English locale and long-string case is Mandatory. Spellcasting
requires pending, changed or rejected target state, duplicate suppression, and
post-response recomputation.

#### C2 hero/equipment and C3 troop transfer

C2 covers equipment, backpack, and hero exchange independently. C3 covers
transfer, exchange, merge, split, and quantities `0`, `1`, half, all, maximum,
full slots, illegal destinations, cancel, and restoration. Their actual game
requests require the full server response matrix. Keyboard/mouse drag
regression is Mandatory for both. Common container behavior may be extracted
only after any two real consumers prove the same observable contract. One-user
helpers stay local to their consumer.

#### N1 town building and recruitment

The slice covers one complete town visit: enter, move among the included town
areas, inspect building and recruitment state, complete both one legal build
and one legal recruit, explain unavailable actions, cancel without mutation,
and return to the adventure map. Both positive paths are Mandatory before E1
or E2, even though the fixed closure journey below uses the build path.

Stable cases are `N1-BUILD-OK`, `N1-BUILD-ALREADY`,
`N1-BUILD-PREREQUISITE`, `N1-BUILD-RESOURCES`, `N1-RECRUIT-OK`,
`N1-RECRUIT-UNAVAILABLE`, `N1-RECRUIT-FULL`, `N1-CANCEL-BUILD`, and
`N1-CANCEL-RECRUIT`. Cancel cases identify the exact modal and pre-submit state
and assert unchanged town, resource, and army hashes. Every case records its
initial hash, expected post-state hash, reset steps, and reset hash.

Gameplay mutations use existing town legality and action paths and require the
full server response matrix. Keyboard/mouse town shortcuts and pointer
activation are Mandatory regressions. Market, mage-guild, and other town
subsystems not exercised by this slice remain explicitly out of scope for N1
and require their own consumer slices before entering a support claim.

N1 is a hybrid adapter. Standard hall, recruitment, modal, button, card, and
slider behavior may reuse only exact contracts already proven elsewhere.
Building-area candidate discovery, transparent/irregular geometry, and spatial
navigation remain town-local. N1 neither forces building areas into common
widget adjacency nor counts as a second consumer proving a generic container
contract.

#### W4 and T1 remaining standard flows

W4 supplies the main-menu or save/load journey not used by W2. Together W4 and
T1 cover naming, editing, deleting, duplicate-name and overwrite confirmation,
cancel, load, exit, restart, and restored observable state for the agreed save
flow. T1 is independent and freezes supported fields and platform IME or
screen-keyboard boundaries. Each claimed OS/input-method pair needs a clean
build, input sequence, cancel, commit, and restoration result. When no usable
screen keyboard or IME exists, the field is explicitly outside the
controller-only claim and presents a clear keyboard fallback. Keyboard text
entry remains Mandatory.

Save compatibility is a separate Mandatory W4 gate, not configuration
migration. If the save format is untouched, it proves load/save/reload of the
fixed current-format fixture. If the format changes, it additionally fixes old
and current sample hashes, migration expectations, preservation rules, and
round-trip results. The configuration column remains only for settings-schema
changes.

#### L1 and L2 launcher slices

L1 requires a clean build for every claimed Qt major and controller and keyboard
activation of the same Start action. For every claimed platform, its named case
below is Mandatory. A logical launch is one Start dispatch followed by one
recorded target identity reaching the case's final state. Additional lifecycle
callbacks for that same identity are not additional launches, but a second
Start dispatch, target identity, or handoff is a duplicate failure. Each case
uses one configured Start input, a 30-second observation window beginning at
its recorded actuation edge, timestamped ordered events, and explicit cleanup
and reset evidence:

1. `L1-DESKTOP-START-ONCE` starts with the cold launcher ready and no candidate
   client PID. It records the launcher PID, candidate client executable, and
   pre-input matching PID set. The required order is one Start dispatch,
   exactly one new matching client PID, then that PID owning a visible client
   main window and remaining alive through the observation window. The window
   must contain one dispatch, one distinct new client PID, and zero additional
   matching client PIDs or handoffs. Reset terminates the client and restores
   launcher-ready with an empty matching PID set.
2. `L1-ANDROID-START-ONCE` starts with the cold launcher Activity resumed and no
   target client Activity instance. It records the target Activity class,
   launcher task identity, and pre-input target instance/task set. The required
   order is one Start dispatch, one target-Activity handoff creating one recorded
   task and instance identity, then that identity reaching resumed state with its
   client surface visible. Repeated lifecycle callbacks for the same identity
   are allowed only when the ledger records them; the window still requires one
   dispatch, one distinct target identity, one final resumed and visible client
   identity, and zero duplicate creations or handoffs. Reset finishes the
   recorded target client Activity instance while preserving the launcher task,
   restores the launcher Activity to resumed, and restores an empty target
   instance set. If a target platform can reset only by ending the whole task,
   the evidence harness instead cold-starts a new app run and verifies the same
   initial launcher-Activity condition; the contract does not require an
   in-process launcher restoration.
3. `L1-IOS-START-ONCE` starts with the cold launcher Qt event loop and launcher
   window active and no client window. The required order is one Start dispatch,
   Qt event-loop exit, launcher-window deactivation and handoff, then exactly
   one recorded client-window identity becoming active and visible. The window
   requires one dispatch, one handoff, one distinct active and visible client
   window, and zero duplicate handoffs or additional active client windows.
   After that app run ends, reset uses the evidence harness to cold-start a new
   app run and verifies the initial condition again: the Qt event loop and
   launcher window are active and no client window exists. Restoring the
   launcher event loop or window inside the completed client run is not an L1
   product behavior or evidence requirement.

Game-server evidence is N/A by construction; settings migration is Conditional
on the diff. L2 requires a replayable page/modal matrix for focus, cancel,
nested modal, restore, unavailable reasons, prompts, and the agreed text-entry
boundaries. Every claimed OS/Qt-major pair needs a build result; keyboard/mouse
and one real controller are Mandatory. New settings use the B0-level migration
and round-trip gate. SDL evidence cannot substitute for launcher evidence.

#### E1 and E2 closure

The fixed local fixture ID is `controller-e2-load-win-v1`, with deterministic
seed `0x56434D49`. Its non-commercial source, content hash, initial client/server
state hashes, and reset hash are frozen in the candidate manifest.

`E1-LOAD-WIN` starts at the SDL client main menu after A2, B2, N1, W4, T1, and
all other required SDL slices are present at fixed SHAs:

1. Reset to the fixture's initial hashes and load the fixture.
2. Execute the referenced A2 adventure steps and enter the fixture town.
3. Execute `N1-BUILD-OK`, then the referenced C2 hero, C3 troop-split, C1 spell,
   B2 battle, and W1 modal cases.
4. Through T1, name the save `controller-e2-save-v1` and overwrite the
   pre-created save of that name.
5. Exit, assert no client process remains, restart at the client main menu,
   load that save, and verify the recorded state invariants.
6. Follow the fixture's deterministic victory route, handle the result modal,
   exit, and assert no client process remains.
7. Run cleanup/reset and assert the original client/server hashes.

`E2-LOAD-WIN` begins from a cold, already-configured launcher, executes the
fixed L2 startup/configuration steps, and then executes the exact numbered E1
segment above. Creation, recruitment, and loss paths are not executor choices
inside these cases. N1 still proves recruitment separately; any create or loss
support claim requires its own named case and evidence bundle.

Each E1/E2 run freshly executes the closure journey and references every
constituent PASS bundle; references do not replace the fresh run. The ledger
separately records physical keyboard events, physical mouse events, synthetic
pointer events, and cursor-fallback entries. All four required counts are `0`
for `E1-LOAD-WIN` and `E2-LOAD-WIN`. It also records blockers, mistakes,
recovery time, save/reload invariants, final result, and success rate. Neither
closure may supply feature behavior for the first time.

The minimum E2 mapping is fixed; every row runs the full `E2-LOAD-WIN` case:

| Evidence row | OS/build | Device | Connection | Steam Input |
|---|---|---|---|---|
| `E2-WIN-XBOX-NATIVE` | Windows | Xbox-class external | USB | Off |
| `E2-WIN-XBOX-STEAM` | Same Windows build as previous row | Same device | USB | On |
| `E2-MAC-DS-NATIVE` | macOS | DualSense external | Bluetooth | Off |
| `E2-LINUX-XBOX-NATIVE` | Desktop Linux | Xbox-class external | USB | Off |
| `E2-DECK-NATIVE` | Steam Deck/Linux | Integrated controls | Integrated | Off |
| `E2-DECK-STEAM` | Same Steam Deck build as previous row | Integrated controls | Integrated | On |

This is a minimum mapping, not an interchangeable pool. Desktop Linux external
coverage and Steam Deck integrated coverage are distinct. Each Steam Input pair
uses the same OS, device, candidate build, fixture, and reset state. Untested
combinations cannot enter support claims.

Every applicable accessibility and localization case uses task completion as
its oracle. The final matrix covers handheld, desktop, and television viewing
conditions; independent HUD/text scaling; non-color focus; long and
pseudolocalized strings; reduced focus animation and camera snapping; and
single-input or toggle alternatives for required holds or chords.

### Server response matrix

Every slice with Mandatory or triggered Conditional server evidence records
server/client SHAs, topology, player slot, initial state/save hash, and four
separate Mandatory outcome cases: accepted, rejected, superseded by a
deterministic authority action, and a distinct state-changed/revalidation case.
If no wire value exists, superseded remains the UI-adapter classification
defined above and is generated through a test seam, second client, or fixed
script rather than a new protocol enum.

Each outcome records:

- a stable evidence case ID and the existing runtime request correlation when
  available; if production has no correlation ID, the field is `N/A` with a
  reason and the evidence case ID plus timestamp locates the transition;
- pre-submit legality, focus, default action, and initial snapshot hash;
- the authority-result generation method and exact trigger time;
- transition to pending and whether a second submission was suppressed;
- response classification and resulting game state, legality, target, focus,
  and default action;
- raw log location;
- cleanup/reset steps and the expected and actual reset snapshot hash.

Ledger fields may come from a test fixture, existing logs, or adapter-observed
state. They do not require a new public request, response, or action object in
production code.

All transitions must match the expected ledger. Missing or duplicate commands,
stale focus, stale legality, or an unhandled response is a failure.

### Reproducible evidence bundle

Each pull request produces a root manifest that lists only payload files by
relative path, byte size, and SHA-256. The root manifest itself and its detached
review envelope are not payload files and must not appear in that list. The
root manifest is UTF-8 JSON serialized exactly with the RFC 8785 JSON
Canonicalization Scheme (JCS), with no byte-order mark or trailing bytes. The
content-addressed bundle ID is the SHA-256 of those exact root-manifest bytes.

A detached review envelope records the bundle ID and immutable locator. Neither
value is written back into the root manifest. A mutable URL without that
detached locator and bundle ID is not an evidence bundle.

The bundle contains:

1. **Root manifest:** schema version; slice and case IDs; base, head, dependency,
   submodule, and composed integration SHAs as applicable; fixture/save/config
   hashes;
   `dirty=false`; build directory identity; required preset and build type; OS,
   version, architecture, compiler and version, Qt and SDL versions; start/end
   time and evidence-generation time; and the path, byte size, and SHA-256 of
   every payload file. It contains neither its own digest nor an immutable
   locator.
2. **Automation:** exact configure, build, and test commands; exit codes; raw
   stdout/stderr; CI run and job URLs with conclusions; SHA-256 for every log.
   A changed base, head, dependency, submodule, fixture, toolchain, required
   preset, or composition identity invalidates and reruns the complete bundle.
3. **Manual matrix:** one row per case and combination with case ID,
   precondition, legal local data/save identifier without commercial content,
   device name and GUID, connection, Steam Input or platform remap, display and
   scale, locale, setup/generation reference, input sequence, expected and
   actual result, PASS/FAIL, capture timestamp/link, defect ID, cleanup/reset
   steps, and expected/actual reset hash.
4. **Server cases:** the fields in the server response matrix, with raw log
   locations.
5. **Configuration migration:** source sample, target schema/version,
   canonical/legacy precedence, default and invalid behavior, expected/actual,
   unrelated and unknown-key handling, round-trip result, and exact command.
6. **Limitations:** ID, severity, affected SHA/platform/device/journey,
   reproduction, fallback or recovery, support-claim impact, and public
   follow-up when one exists.
7. **E2E ledger:** each frozen journey step, separate physical keyboard,
   physical mouse, synthetic-pointer, and cursor-fallback counts, mistakes,
   recovery time, save/reload invariants, final PASS/FAIL, success rate across
   recorded runs, and cleanup/reset hashes.

Automation, manual, server, configuration, limitation, E2E, capture, and raw
log files are all included in the per-file digest list. Captures map to a case
ID and timestamp. Required CI output is copied into the bundle; provider URLs
are supplementary and an expiring CI URL alone is invalid. The complete bundle
is retained with the pull request's review record for that review's lifetime.
Commercial data and saves remain local; public fixtures use only
project-approved data.

Every automation, manual, server, configuration, and E2E case records its setup
or generation reference, fixture/save/config hash, cleanup/reset steps, and
expected and actual reset state. A case without a reproducible reset is a
failure and cannot lend state to the following case.

### Limitation severity and support claims

- **Blocker:** evidence cannot be generated or replayed for a required gate.
  The candidate is rejected.
- **Critical:** a core journey has no reliable recovery, data or game state may
  be corrupted, or an input can submit the wrong action. The candidate is
  rejected.
- **Major:** an in-scope journey fails but has a reliable documented recovery.
  If it touches a Mandatory item, triggered Conditional item, or fixed minimum
  platform/device row, the candidate is rejected. Only an extra, non-required,
  unclaimed combination may retain a Major limitation without passing it.
- **Minor:** the journey completes and state remains correct, but presentation
  or efficiency is degraded. The limitation must still be disclosed.

`Blocker`, `Critical`, `Major`, and `Minor` are the only severity values. An
entry named only "known issue" is invalid. No pull request may claim an untested
platform/device combination. Framework-only code cannot merge without a real
consumer and lifecycle coverage.

## Candidate promotion gate

This document and its sequence are discussion evidence, not an integrated or
publishable implementation candidate. Promotion requires all of the following:

- the source map, reproducible build, user-journey matrix, test matrix, and
  threshold fix are tied to the current source baseline and evidence;
- changing the base SHA revalidates every positive code reference, negative
  search, and semantic baseline conclusion, not only the build and tests;
- the five prototypes report input sequences, reused interfaces, failure
  conditions, and duplicated-state checks before a common contract is frozen;
- every production slice has a fixed SHA, explicit dependencies, a real
  consumer, independently observable value, and applicable automated,
  regression, and hardware evidence;
- a helper needed by only one consumer remains consumer-local and is reviewed
  with it; a shared or public contract requires two real consumers proving the
  same observable behavior;
- controller code does not duplicate bindings, input mode, scene rules, or
  server authority;
- Qt candidates keep independent scope, review, and validation, using desktop
  targets or the mobile composed app target as appropriate, and do not carry
  live SDL client runtime objects into the launcher side of the handoff;
- an exact composed SHA set is validated in a clean workspace, and any SHA
  change triggers revalidation;
- an artifact hash fixes only the reviewed document bytes and cannot substitute
  for a Git candidate SHA;
- after the five prototypes and maintainer decisions, any contextual UI
  command, focus lifecycle/command-state contract, SDL/Qt sharing rule, or
  widget/container extraction intended as a public interface is recorded in a
  maintainer-acceptable design record before freeze;
- public material contains no private paths or process details, commercial
  game data, or untested platform and device claims.

Until these conditions are satisfied, the accurate status is *internal draft*
or *sequence candidate*, not *publishable*.

## Maintainer decisions requested

1. Is extending `EShortcut` sufficient for navigation and contextual commands,
   or is a small semantic UI command layer acceptable if prototypes prove a
   gap?
2. Where should the minimal focus lifecycle contract live relative to
   `WindowHandler`, `CIntObject`, and `EventDispatcher`?
3. Which existing modal window is the best first upstream consumer?
4. How should a required but unavailable action remain focusable and expose a
   concrete reason without changing existing pointer and keyboard behavior?
   Decorative, hidden, and irrelevant items remain outside navigation.
5. Is adventure-map object focus plus tile fallback an acceptable direction,
   provided it reuses current click/hover/action semantics?
6. Should battle controller selection extend `BattleFieldController` with an
   explicit selected hex while all legality remains in existing battle paths?
7. What is the minimum acceptable first scope for issue #4111: Start only,
   pointer emulation, or native Qt widget navigation?
8. Which platforms and devices must be demonstrated before the first consumer
   PR is considered mergeable?
9. Should configurable widgets gain optional navigation metadata after two
   native consumers prove the schema, or should the first version remain code
   only?

## Feedback and revision loop

1. Discuss this RFC before freezing an interface or opening the production PR
   stack.
2. Record which questions are decided, which require a prototype, and which are
   rejected.
3. Run the bounded prototypes and publish evidence without presenting them as
   production-ready code.
4. Revise the sequence and contracts to match the evidence and maintainer
   direction.
5. Submit one vertical pull request at a time, with dependencies and observable
   value stated explicitly.
6. Keep requested changes in the same pull request unless maintainers ask for a
   new split; update evidence after every behavioral revision.
7. If a proposed abstraction is rejected, remove or narrow it around the real
   consumer instead of preserving it for hypothetical future use.

## Repository rules used by this draft

- VCMI's [repository guidelines](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/AGENTS.md)
  define the architecture, build, and source conventions relevant here.
- The [coding guidelines](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/docs/developers/Coding_Guidelines.md)
  require C++20-compatible code across supported compilers and specify the
  project's formatting conventions.
- The [pull-request workflow](https://github.com/vcmi/vcmi/blob/4cb465b3522018517d138c38fc9ab9db00e4d25b/.github/workflows/github.yml)
  is the authoritative automated build, test, and validation surface at this
  baseline.

At this baseline the repository contains no `CONTRIBUTING.md` or pull-request
template. This draft therefore does not assume an undocumented rebase, squash,
review, or commit-message policy. Maintainer direction should override any
workflow assumption in this document.
