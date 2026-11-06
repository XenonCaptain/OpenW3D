# OpenW3D Sandbox — implementation roadmap

This branch is a **foundation**, not yet a playable Garry's Mod-style mode.
It keeps original Renegade gameplay untouched while introducing a testable,
server-authoritative model for sandbox prop operations.

## Player experience (target)

From the launcher/main menu:

- **Single Player > Sandbox**: start a local sandbox session, choose a map,
  spawn and arrange props, save and load creations.
- **Multiplayer > Sandbox**: browse or host dedicated build servers, collaborate,
  manage permissions, and share creations.
- Preserve the existing Renegade mission and C&C multiplayer options.

Proposed toolgun: spawn, move, rotate, freeze/unfreeze, duplicate, remove; later
weld, constraints, physics manipulation, color/material, cleanup, and undo.
Proposed spawn menu: categorized allowlisted presets, preview, rotation, search.
Use a dedicated sandbox map/mode rather than changing C&C victory logic.

## What this first patch implements

`Code/Sandbox/` contains a standalone C++20 gameplay **state model** with:
- explicit single-player / multiplayer session selection;
- allowlisted preset identifiers (never trust an arbitrary client-supplied preset);
- per-player ownership and configurable per-player/world object caps;
- validated spawn, move, rotate, freeze, unfreeze, duplicate, and remove requests;
- non-finite/out-of-bounds position/rotation rejection;
- CTest-based rule tests.

The model intentionally does **not** instantiate Renegade objects, send packets,
display menus, run physics, or persist worlds yet. Its IDs are sandbox record IDs,
not W3D network IDs.

## Engine integration (next steps, in order)

1. **Game selection:** add Sandbox to the main menu in
   `Code/Commando/dlgmainmenu.*` and game initialization in
   `Code/Commando/gameinitmgr.*`. Keep underlying Renegade mission/multiplayer
   classification working while tracking sandbox mode separately.
2. **World adapter:** map validated sandbox requests to
   `ObjectLibraryManager::Create_Object` in `Code/Combat/objlibrary.*`,
   `PhysicalGameObj` transforms, and `GameObjManager` lifecycle.
   Keep a mapping from sandbox record IDs to real engine objects. Reject failed
   object creation without committing a successful sandbox request.
3. **Network protocol:** define versioned client-to-server tool requests,
   authenticated player identity, server-side authorization, reliable object
   creation/deletion events, late-join snapshots, and incremental updates.
   Never accept a player ID or ownership assignment from the client packet.
   Run `World::Apply` on the server only in multiplayer.
4. **Physics/tools:** implement raycast selection, prop placement validation,
   rigid-body manipulation, freeze state, collision checks, and UI previews.
5. **Build servers:** configure an FDS sandbox mode with per-player limits,
   public/private options, moderation, cleanup, and optional persisted maps.
6. **Persistence:** versioned, atomic serialization of preset, transform,
   owner and freeze state. Validate allowlisted presets and caps on loading;
   implement save/load/undo with permission checks.
7. **Testing:** compile all affected targets on supported platforms; integration
   test single player, hosted multiplayer, headless FDS, permissions and
   late-joining clients.

## Running standalone rules tests

Configure the usual OpenW3D build with testing enabled, then run:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target openw3d_sandbox_tests
ctest --test-dir build -R openw3d_sandbox_rules --output-on-failure
```

Note that full project configuration still requires the engine's platform
dependencies and asset data for gameplay. The rules code can also be tested
independently with a C++20 compiler.

## Security and licensing

- Servers must be authoritative for all edits and object lifecycles.
- Add rate limiting, preset budgets, player cleanup, and admin permissions
  before exposing build servers publicly.
- Do not distribute original Renegade proprietary game assets; the project
  README documents game-data requirements.
- Preserve the repository's GPLv3 and additional licensing terms.
