# Godot 4.x Best Practices

## 1. Core Workflow
- **Confirm before building**: New game / broad feature / vague request -> ask: gameplay, scope, controls, art style, target platform, success criteria. Then make a short implementation plan (scenes + scripts).
- **Scene independence**: Every `PackedScene` should run in isolation. Avoid hardcoded paths or cross-scene direct references.
- **Loose coupling**: Prefer **signals**, **groups**, or **duck typing** (`has_method`) for cross-scene communication. Avoid direct node references.
- **Strict OOP**: Follow SOLID, DRY, KISS, YAGNI.

## 2. Script vs. Scene Selection
- **Pure script** (`class_name`): No visual/spatial logic, pure data or algorithms (state machines, inventory data).
- **Scene** (`.tscn`): Requires visual node hierarchy (player, UI widgets).
- **No anonymous types**: Reusable scripts must declare `class_name`.

## 3. Scene Tree Architecture
- **Parent-child**: Only when child's lifecycle strictly depends on parent (parent deleted -> child deleted).
- **Independent objects**: Different lifecycles (player vs. room) must be siblings or in separate branches.
- **Spatial decoupling**: Use `RemoteTransform` or `top_level = true`.
- **Recommended entry structure**:
  ```
  Main (global coordinator)
  |-- World (swaps levels dynamically)
  `-- GUI (top-level UI, independent of world)
  ```
- **Configuration warnings**: Implement `_get_configuration_warnings()` for scenes with loose dependencies.

## 4. Global State & Singletons
- **Avoid singleton bias**: Prefer self-contained scenes (coin pickup spawns its own `AudioStreamPlayer` that frees itself).
- **Static members**: For pure utilities -> `class_name` + `static func`. For shared data -> `static var`.
- **Only allowed singleton use cases**: Systems that live across the whole game without polluting other scenes (save/load, achievements, global dialogue).

## 5. Cross-Object Communication (Priority order)
1. **Signals** - preferred, child -> parent or third party.
2. **Node paths** - only for tight permanent parent-child inside an encapsulated scene.
3. **Scene unique nodes (`%`)** - only within the same scene.
4. **Singletons** - only for globally unique systems.
5. **Injection** - parent or world controller injects references.
6. **Groups** - dynamic fetch with `get_tree().get_nodes_in_group()`.

- **Duck typing**: Use `has_method("apply_damage")` instead of type casting.
- **Virtual method inheritance**: Define `class_name` base with empty virtual methods, override in subclasses.

## 6. Lifecycle Callbacks
- **`_init`**: Object allocated - no children, no `@export` values. **Do not fetch children**.
- **`@export`**: Assigned after `_init`, before `_ready`.
- **`_ready`**: Safest place to fetch children, connect signals, process exported variables.
- **`_process(delta)`**: Only for visuals / UI / non-critical timers.
- **`_physics_process(delta)`**: All physics, movement, raycasting.
- **`_input` / `_unhandled_input`**: Event-driven - do not poll input actions.

## 7. Data & Logic
- **Data containers**: Prefer custom `Resource` (`.tres` files) over `Dictionary`.
- **Enums**: Use `enum State { IDLE, RUN }`, never raw strings.
- **Instantiation order**: After `.instantiate()`, set properties **before** `add_child()`.
- **Loading**: `preload` for small/frequent objects; `load` for large assets (use `ResourceLoader` background loading).

## 8. Directories & Naming
- **Two directory styles** (pick one):
  - Categorized: `scenes/`, `scripts/`, `assets/`
  - Modular: `src/player/` (contains `.tscn`, `.gd`, textures, audio)
- **Strict naming**:
  - Files/folders: `snake_case`
  - `class_name`: `PascalCase`
  - Variables/functions: `snake_case`
