# Godot 4.x Game Project Development Context & Guidelines for AI Agents

## 1. Core Architectural Philosophy

* **Confirm Before Building**: For new games, broad features, or vague requests, first ask concise questions to confirm the game design, scope, controls, art style, target platform, and success criteria. After the design is clear, make a short implementation plan that includes separate scenes and scripts before editing the project.
* **Encapsulation & Independence**: Every scene (`PackedScene`) and node (`Node`) should be designed to function perfectly when run in isolation. Hardcoded paths or tight cross-layer references break system stability.
* **Loose Coupling & Anonymous Collaboration**: When cross-scene communication is unavoidable, prioritize **Signals**, **Groups**, or **Duck Typing/Method Checking** to interact anonymously rather than passing direct node references.
* **Adhere to OOP & Game Design Principles**: All custom classes, scripts, and systems must strictly follow SOLID, DRY (Don't Repeat Yourself), KISS (Keep It Simple, Stupid), and YAGNI (You Aren't Gonna Need It) principles.

## 2. Godot Classes & Script Selection (What are Godot Classes)

* **The Nature of Classes**: In Godot, a `Script` resource defines a class, and a `PackedScene` is a specialized class containing a pre-configured tree arrangement of nodes. Both extend engine base classes (e.g., `Node`, `Resource`).
* **Script vs. PackedScene Decision Matrix**:
* **Use a Pure Script**: When the class contains no spatial, visual, or complex node arrangement, and is purely responsible for underlying logic or data processing (e.g., state machines, math algorithms, inventory data definitions).
* **Use a PackedScene**: When the class requires visual configuration or node hierarchy cooperation (e.g., a Player with a Sprite, CollisionShape, and AnimationPlayer, or a complex UI widget).


* **Avoid Anonymous Types**: Always use the `class_name` keyword for reusable or fundamental scripts. This promotes them to named global types, enabling compile-time auto-completion and clean type-checking (using the `is` keyword).

## 3. Scene Tree Architecture & Flow (Scene Organization)

* **Relationship Trees vs. Spatial Trees**: Structure the `SceneTree` based on **lifecycle dependency relationships** rather than spatial positioning in the game world.
* **Parent-Child Relation**: Use this *only* if the child's lifecycle is strictly bound to the parent (i.e., if the parent is deleted, the child *should* logically be destroyed along with it).
* **Sibling/Independent Relation**: If two objects have different lifecycles (e.g., a Player and a Room), they must be separated in the tree hierarchy (as siblings or under different branches), even if they overlap spatially.


* **Spatial Decoupling Solutions**: If nodes are separated in the scene tree but need to share spatial transforms, use `RemoteTransform` / `RemoteTransform2D` nodes, or enable the `top_level = true` property on the CanvasItem/Node3D to ignore inherited parent transformations.
* **Standard Main Entry Point Framework**:
```
Main (main.gd - Global coordinator, handles cold boots, configures level transitions)
├── World (game_world.gd - Holds the active 2D/3D gameplay world, dynamically swaps sub-levels)
└── GUI (gui.gd - Top-level UI, main menu, HUD; persists independently of world swaps)

```


* **Self-Documentation & Configuration Warnings**: If a scene or a tool script (`@tool`) has a loose dependency on its external tree environment, implement the `_get_configuration_warnings()` virtual method. Return a `PackedStringArray` of warnings to let the editor display real-time errors, completely avoiding the need for external markdown readmes.

## 4. Global State & Singletons (Autoloads vs. Internal Nodes)

* **Anti-Singleton Bias**: Avoid creating master "Manager" singletons blindly (e.g., a global sound manager or a global particle pool). Prefer letting individual scenes contain and allocate their own resources (e.g., a coin pickup scene spawns its own `AudioStreamPlayer`, which frees itself once the sound ends).
* **The Cost of Global State**: When an Autoload manages global data, any script in the project can pollute it with corrupt values, making debugging extremely difficult.


* **Static Functions and Variables**:
* For pure utility/helper libraries that don't require member variables or `self` references, use `class_name` combined with `static func`.
* For sharing data variables across instances of a class without an active node instance, use `static var` (supported in Godot 4.1+).


* **Valid Autoload Use Cases**:
* Systems that span across the entire game lifetime and manage their own isolated data without mutative intrusion into other scenes (e.g., Achievement tracking, Save/Load systems, Global dialogue orchestrators, or Screen transition fades).



## 5. Interface Design & Cross-Object Communication (Godot Interfaces)

When interacting across object boundaries, select data and reference access strategies according to this priority hierarchy:

### A. Retrieving Object References

1. **Absolute/Relative Node Paths**: Use *only* inside a highly encapsulated scene between tight, permanent parent-child nodes.
2. **Scene Unique Nodes (`%`)**: Use to fetch deep nodes within the *same* encapsulated scene. Never use across scene boundaries.
3. **Signals**: **The most recommended approach for downward/outward communication.** A child node triggers a signal with payload data, allowing a parent or a third party to bind (`connect`) to it.
4. **Singletons (Autoloads)**：Only for globally unique, monolithic systems (e.g., `get_node("/root/GlobalSystem")`).
5. **Mediator / Third-Party Coordination**：Have a mutual parent or world controller inject references into the nodes.
6. **Groups**：Fetch references dynamically using `get_tree().get_nodes_in_group("group_name")`.

### B. Accessing Data & Logic (Decoupled Interfaces)

Godot lacks a native `interface` keyword. You must achieve object-oriented polymorphism using these two mechanisms:

* **Duck Typing / Explicit Method Verification**:
Before executing a method on an untyped or loosely typed target, verify its interface explicitly using `has_method()`.
```gdscript
if object.has_method("apply_damage"):
    object.apply_damage(amount)

```


* **Base Class Virtual Method Inheritance**:
Define an abstract or basic script with a registered `class_name` containing empty virtual methods. Have concrete sub-classes override them. The caller can safely cast the instance (`as TargetBaseClass`) and run the method.

## 6. Notifications & Lifecycle Callbacks (Godot Notifications)

Strictly isolate frame loops and setup code into their designated callbacks:

* **`_init()` vs. `@export` Variables vs. `_ready()**`:
* **`_init()`**: Runs when the object is allocated in memory. **At this stage, child nodes are not in the tree and `@export` inspector values are not yet assigned.** Do not fetch child references or rely on inspector configuration here.
* **`@export` Variables**: Populated by the engine right after `_init()` and before `_ready()`.
* **`_ready()` / `_enter_tree()**`: Runs when the node and all its children have safely entered the active scene tree. **This is the safest place to fetch sub-nodes, connect internal signals, and process exported variables.**


* **`_process` vs. `_physics_process` vs. Input Callbacks**:
* **`_process(delta)`**: Ticks every rendered frame; variable framerate. Use *only* for cosmetic tracking, visual smoothing, UI updates, or non-critical timers.
* **`_physics_process(delta)`**: Ticks at a fixed rate (default 60Hz). **All physics calculations, movement, velocity manipulations, raycasting, and area checks must execute here.**
* **`_input(event)` / `_unhandled_input(event)**`: Event-driven loop. Do not poll input actions inside `_process` unless tracking a continuous joystick axis vector. Use input callbacks to catch distinct, single-frame trigger actions (e.g., jumping, opening menus).



## 7. Data vs. Logic Preferences

### A. Data Layout Selection

* **Built-in Compound Types**:
* **Array**: Use for sequential, linear collections of uniform types.
* **Dictionary**: Use for key-value maps. If the keys are static strings, consider migrating to a strongly typed `Resource` or `Object` to gain static compile-time safety.
* **Custom Resource (Resource Class)**: **Highly Recommended.** For stats configurations, items lists, level data, or skill tree parameters, define an extension of `Resource` using `class_name`. They save natively into `.tres` files, allowing visual adjustments and reusability directly inside the Inspector.


* **Enumeration (Enums) Standards**:
* Do not use arbitrary string matching ("Idle", "Walk") to branch state machine logic.
* Always use explicit named enums (`enum State { IDLE, WALK }`). In Godot 4.x, enums resolve to integers, ensuring extreme speed and strict static type check safety.



### B. Logic & Execution Sequence

* **Instantiation Order of Operations**:
* **Rule: Modify properties first, append to tree last.**
* When spawning an instance via code (`.instantiate()`), you must inject configuration variables and initialize member values **before** calling `add_child()`.
* *Reasoning*: Some nodes rely on initialization parameters during their `_ready()` execution. Putting a node into the tree first triggers its `_ready()` loop immediately, running it with stale or empty defaults.


* **Loading (`load`) vs. Preloading (`preload`)**:
* **`preload("res://...")`**: Loads the asset at compile time/script parsing time. Use for tiny, persistent, or rapidly instantiated objects (e.g., projectiles, small particle systems, fundamental UI fragments).
* **`load("res://...")`**: Loads the asset dynamically at runtime when the execution hits the line. Use for bulky level geometry, massive textures, or specific cutscene animations to prevent memory bloat, preferably via multi-threaded background loaders (`ResourceLoader`) to dodge main thread micro-stutters.



## 8. Directory Layout & VCS Standards (Project Organization)

### A. Project Structure

Maintain a flat or modular root layout. Never drop stray files into the root folder. Choose one of the following two file directory philosophies and stick to it:

* **Game Project Shape**: Even small playable games should normally use separate scenes for main entry, gameplay world, player/actors, UI/HUD, menus, and reusable objects where relevant. Avoid placing the whole game into one scene and one script unless the user explicitly asks for a tiny prototype.
* **Logic Boundaries**: Split scripts by responsibility. Typical boundaries include player control, enemy or obstacle behavior, spawning, scoring, game state, UI presentation, save/load, and reusable resources. Do not hide unrelated systems inside a single monolithic script.
* **Categorized Layout (Best for small-to-mid projects)**:
`res://scenes/`, `res://scripts/`, `res://assets/textures/`, `res://assets/audio/`
* **Feature/Modular Layout (Best for large projects, high cohesion)**:
`res://src/player/` (Encapsulates `player.tscn`, `player.gd`, `player_sprite.png`, `player_hurt.wav` all in one spot)
* **Strict Case Sensitivity**:
* Production exports (e.g., Linux, Android) are case-sensitive, while development environments like Windows are not.
* **Enforced Convention**: Folder names and filenames must strictly use **snake_case** (lowercase with underscores), e.g., `main_menu.tscn`.
* Inside code, script `class_name` definitions must use **PascalCase** (UpperCamelCase), while variables and functions use **snake_case**.



### B. Version Control Systems (VCS / Git)

To protect repository size and eliminate diff merge locks, maintain strict `.gitignore` configurations.

* **Mandatory Git Exclusions**:
* The `.godot/` folder: Contains local cache, imported asset metadata, and local engine state. **Never commit this directory.**
* `*.translation`: Binary export artifacts generated automatically from localization sources.


* **Large File Storage (Git LFS)**:
Heavy assets such as `.wav`, `.ogg`, `.png`, `.jpg`, `.fbx`, `.gltf`, or `.blend` files must be managed by `git-lfs` rules to avoid repository explosion.
* **Text Format Leverage**: Godot's `.tscn` (scenes) and `.tres` (resources) are native INI-like text structures. When the Agent generates or parses these, it must respect their text formatting so version control changes can be tracked easily line-by-line via standard Git Diff logs.
