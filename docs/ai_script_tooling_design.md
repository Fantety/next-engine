# AI Script Tooling Design

## Goal

AI script tooling lets the agent create, edit, bind, unbind, inspect, and delete GDScript files through editor-side services. The tools must stay inside the project boundary, use Godot editor/resource APIs where possible, and keep high-risk actions behind explicit user approval.

## Architecture

Script operations are centralized in `AIScriptEditingService` under `editor/ai_component/tools/editor`. Tool classes only validate JSON arguments, log execution, call the service, and return structured metadata.

The service owns:

- project path normalization and boundary checks
- GDScript syntax parsing through `GDScriptParser`
- function-level source range lookup using AST node line metadata
- file creation, overwrite, patch, and deletion
- node script bind/unbind through loaded `Script` resources and node `set_script`
- editor filesystem refresh and current scene save where needed

## Tools

- `script.inspect`: parses a `.gd` file and returns extends/class/functions with line ranges.
- `script.create`: creates a new GDScript file from explicit source or a minimal `extends` template.
- `script.write`: replaces a script file after syntax validation.
- `script.patch_function`: replaces an existing function or appends a new function using AST line ranges.
- `script.bind_to_node`: loads a script resource and assigns it to a node in the edited scene.
- `script.unbind_from_node`: clears the script on a node in the edited scene.
- `script.delete`: deletes a script file after explicit approval.

## Permission Model

Read-style tools are available in Ask/Plan mode. Mutating script tools are available in Write mode. `script.delete` is always an ask-gated tool, even in Write mode.

When a tool requires approval, runtime creates a pending approval record instead of executing it. The dock shows a confirmation dialog/card. Approval resumes execution of the exact pending tool call; rejection records a tool result so the model can continue with that denial.

## Function-Level Edits

`script.patch_function` uses `GDScriptParser::parse()` and scans the root `ClassNode` members for a `FunctionNode`. For replacements, the service replaces the full source line range reported by the function node. For insertions, it appends a normalized function block to the file after validating that the complete script still parses.

This first version intentionally does not generate source from arbitrary AST nodes. Godot's GDScript parser is suitable for structure and location, but it is not a formatting-preserving rewrite engine.
