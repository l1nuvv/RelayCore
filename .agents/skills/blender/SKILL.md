---
name: blender
description: Use this skill to help AI agents read, interpret, and generate code against Blender’s Python API. It provides practical guidance for working with bpy, bpy.ops, bpy.data, bpy.context, bmesh, mathutils, gpu, properties, and depsgraph, with an emphasis on safe patterns, context handling, operator limitations, debugging, and script structure for reliable Blender automation.
---

## Purpose
Use this skill when an AI agent needs to read, write, explain, or generate **Blender Python API** code for scripting, automation, add-ons, geometry workflows, scene manipulation, rendering setup, UI panels/operators, mesh editing, or data inspection.

This skill is designed for agents that must produce **correct, context-aware Blender Python code** and avoid common Blender API failure modes around **context**, **mode**, **operators**, and **evaluated vs original data**.

## Canonical source
Primary reference:
- Blender Python API (current): `https://docs.blender.org/api/current/`

High-value sections:
- Quickstart: `https://docs.blender.org/api/current/info_quickstart.html`
- Gotchas: `https://docs.blender.org/api/current/info_gotcha.html`
- Using Operators: `https://docs.blender.org/api/current/info_gotchas_operators.html`
- `bpy.data`: `https://docs.blender.org/api/current/bpy.data.html`
- `bpy.context`: `https://docs.blender.org/api/current/bpy.context.html`
- `bpy.ops`: `https://docs.blender.org/api/current/bpy.ops.html`
- `bpy.props`: `https://docs.blender.org/api/current/bpy.props.html`
- `bmesh`: `https://docs.blender.org/api/current/bmesh.html`
- `mathutils`: `https://docs.blender.org/api/current/mathutils.html`
- `gpu`: `https://docs.blender.org/api/current/gpu.html`
- `bpy.types.Object`: `https://docs.blender.org/api/current/bpy.types.Object.html`
- `bpy.types.Depsgraph`: `https://docs.blender.org/api/current/bpy.types.Depsgraph.html`

## What this skill covers
- Scene/object creation and organization
- Selection, activation, linking, collections, view layers
- Mesh generation and editing with `bmesh`
- Transform math with `mathutils`
- Materials, modifiers, animation, cameras, lights, rendering setup
- Add-on scaffolding with `Operator`, `Panel`, `PropertyGroup`, registration
- Context-sensitive operator calls and overrides
- Data access via RNA (`bpy.data`, `bpy.types.*`) versus tool-style operator calls (`bpy.ops`)
- Dependency graph and evaluated data access
- Safe debugging and compatibility habits

## Core rule set for agents

### 1) Prefer direct data API over operators when possible
Blender operators (`bpy.ops`) are UI/tool oriented and depend on the current context. They are often less reliable in headless scripts, background runs, and automated agent workflows.

Prefer:
- `bpy.data` for creating/finding datablocks
- `bpy.types.*` and direct property assignment for editing datablocks
- `bmesh` for mesh editing
- `mathutils` for transforms and geometry math

Use `bpy.ops` only when:
- The task is inherently a tool action
- There is no practical direct RNA alternative
- You can satisfy the required context, area, region, selection, active object, and mode

### 2) Treat `bpy.context` as situational and mostly read-only
Context depends on where Blender is currently operating. Do not assume that `bpy.context.object`, selected objects, active area, or active mode are always valid.

Before any context-sensitive work, explicitly verify:
- active object
- selected objects
- current mode (`OBJECT`, `EDIT_MESH`, etc.)
- scene/view layer
- area/region if UI-dependent operator calls are needed

### 3) Be explicit about mode changes
Many API failures come from calling the right function in the wrong mode.

Typical expectations:
- object creation and most datablock edits: `OBJECT` mode
- mesh edit operations via edit mesh workflows: `EDIT_MESH`
- many `bmesh` workflows are safer by creating a standalone `BMesh`, editing it, then writing back

Pattern:
1. save the current mode if relevant
2. switch deliberately
3. do the work
4. update data
5. restore mode when reasonable

### 4) Distinguish original data from evaluated data
If modifiers, constraints, animation, or the dependency graph matter, evaluated state may differ from original datablocks.

Use the dependency graph when the task needs the post-evaluation result.

### 5) Write version-tolerant code where practical
Blender API details can change between versions. When generating reusable scripts or add-ons:
- avoid undocumented behavior
- keep API usage conservative
- isolate version-specific calls
- note assumptions when relying on features that may be version-sensitive

## Decision tree

### Use direct RNA/data API when the task sounds like:
- create an object, mesh, material, collection, camera, light
- rename, reparent, relink, hide, animate, assign modifier/material
- inspect scene data
- set render settings
- assign transforms or properties

### Use `bmesh` when the task sounds like:
- create/edit vertices, edges, faces
- extrude, inset, split, bridge, dissolve, recalculate normals
- procedural mesh generation
- topology-aware mesh manipulation

### Use `bpy.ops` when the task sounds like:
- invoke a built-in interactive tool
- do something normally triggered from a menu, toolbar, or shortcut
- perform an operation that is only exposed as an operator

### Use depsgraph/evaluated data when the task sounds like:
- apply or inspect modifier results without destructively applying modifiers
- inspect animated/constraint-driven transforms
- export or measure the final evaluated geometry/state

## Module map for agents

### `bpy`
Top-level Blender Python entry point.
Use for global access to context, data, app state, types, props, ops, utils.

### `bpy.data`
Use for datablock access.
Examples:
- `bpy.data.objects`
- `bpy.data.meshes`
- `bpy.data.materials`
- `bpy.data.collections`
- `bpy.data.images`

Typical uses:
- create datablocks
- iterate through scene assets
- fetch by name
- remove unused data carefully

### `bpy.context`
Use to inspect current operational state.
Typical fields include current scene, view layer, active object, selected objects, window/area/region.
Do not treat it as a stable source across unrelated execution contexts.

### `bpy.ops`
Use for operator calls.
Important traits:
- keyword arguments only
- returns a status set, not rich data
- relies on context

### `bpy.types`
Use for class definitions and RNA types such as:
- `Operator`
- `Panel`
- `PropertyGroup`
- `Object`
- `Scene`
- `Material`

### `bpy.props`
Use to define properties for registered classes and add-on configuration.
Common property types:
- `BoolProperty`
- `IntProperty`
- `FloatProperty`
- `StringProperty`
- `EnumProperty`
- `PointerProperty`
- `CollectionProperty`

### `bmesh`
Use for mesh editing with connectivity awareness.
Best for procedural or topology-aware work.
Common pattern:
- create/get `BMesh`
- edit geometry
- write back to mesh
- free/update resources

### `mathutils`
Use for geometric math.
Common types:
- `Vector`
- `Matrix`
- `Euler`
- `Quaternion`
- `Color`

Use for transforms, normals, projections, rotations, coordinate conversions.

### `gpu` and `gpu_extras`
Use only for viewport/GPU drawing tasks, custom drawing, overlays, shaders, batches.
Avoid unless the user explicitly needs viewport drawing or shader-level functionality.

## Standard workflow for Blender agent tasks

### Workflow A: Scene/datablock task
Use for tasks like creating objects, assigning materials, parenting, transforms.

1. Identify target scene/view layer/collection.
2. Fetch or create datablocks with `bpy.data`.
3. Link objects to the correct collection.
4. Set transforms and datablock properties directly.
5. Only use selection/activation if the next step truly requires operators.

### Workflow B: Procedural mesh task
1. Create a mesh datablock and object.
2. Link the object to a collection.
3. Build/edit geometry with `bmesh`.
4. Write the `BMesh` back to the mesh datablock.
5. Validate/update normals if needed.
6. Avoid `bpy.ops.mesh.*` unless the workflow specifically requires operator behavior.

### Workflow C: Operator-driven task
1. Determine exact operator name and expected arguments.
2. Check required mode, active object, selection, and UI context.
3. Set up the context deliberately.
4. Call the operator with keyword arguments only.
5. Check the returned status set.
6. Clean up selection/mode side effects.

### Workflow D: Add-on task
1. Define `bl_info`.
2. Implement `Operator`/`Panel`/`PropertyGroup` classes.
3. Register properties and classes.
4. Keep business logic separate from UI code.
5. Use `self.report` for operator feedback when appropriate.
6. Ensure `register()`/`unregister()` are complete and symmetric.

## Required checks before returning Blender code
The agent should verify all of the following:
- Is the code using `bpy.ops` where direct data access would be safer?
- Does the code assume an active object or selection without setting/checking it?
- Is the mode correct for each operation?
- Are objects linked into a collection so they actually appear in the scene?
- Is evaluated data needed instead of original data?
- Are mesh edits followed by appropriate updates?
- Are add-on classes and properties registered correctly?
- Are operator arguments passed as keywords only?
- Does the code avoid destructive actions unless explicitly requested?
- Is the script safe in background/headless execution if the user asked for automation?

## Common pitfalls
- Creating datablocks but forgetting to link them into a collection
- Using `bpy.context.object` without ensuring it exists
- Calling mesh operators outside the required mode
- Assuming selected/active objects are what the script expects
- Using operators inside automation when direct RNA access is available
- Expecting operators to return objects or rich values
- Editing original data when the task actually needs evaluated geometry
- Forgetting to update mesh data after `bmesh` edits
- Writing add-ons with registration gaps or stale property definitions
- Depending on viewport/UI context in background mode

## Preferred code patterns

### Pattern: create and link an object
```python
import bpy

mesh = bpy.data.meshes.new("MyMesh")
obj = bpy.data.objects.new("MyObject", mesh)

collection = bpy.context.scene.collection
collection.objects.link(obj)

obj.location = (0.0, 0.0, 0.0)
```

### Pattern: procedural mesh with `bmesh`
```python
import bpy
import bmesh

mesh = bpy.data.meshes.new("BM_Mesh")
obj = bpy.data.objects.new("BM_Object", mesh)
bpy.context.scene.collection.objects.link(obj)

bm = bmesh.new()
try:
    v1 = bm.verts.new((0.0, 0.0, 0.0))
    v2 = bm.verts.new((1.0, 0.0, 0.0))
    v3 = bm.verts.new((0.0, 1.0, 0.0))
    bm.faces.new((v1, v2, v3))
    bm.to_mesh(mesh)
finally:
    bm.free()

mesh.update()
```

### Pattern: context-sensitive operator call
```python
import bpy

obj = bpy.context.active_object
if obj is None:
    raise RuntimeError("No active object")

if bpy.context.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

result = bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
if 'FINISHED' not in result:
    raise RuntimeError(f"Operator failed: {result}")
```

### Pattern: evaluated object access
```python
import bpy

depsgraph = bpy.context.evaluated_depsgraph_get()
obj = bpy.context.active_object
obj_eval = obj.evaluated_get(depsgraph)

# obj is original data, obj_eval is evaluated state
print(obj_eval.matrix_world)
```

### Pattern: simple add-on operator skeleton
```python
bl_info = {
    "name": "Example Add-on",
    "blender": (4, 0, 0),
    "category": "Object",
}

import bpy

class OBJECT_OT_example(bpy.types.Operator):
    bl_idname = "object.example_operator"
    bl_label = "Example Operator"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        if obj is None:
            self.report({'ERROR'}, "No active object")
            return {'CANCELLED'}
        obj.location.x += 1.0
        return {'FINISHED'}


def register():
    bpy.utils.register_class(OBJECT_OT_example)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_example)


if __name__ == "__main__":
    register()
```

## Output requirements for the agent
When answering a Blender API request, the agent should usually provide:
1. A brief statement of the chosen API approach
   - direct data API
   - `bmesh`
   - operator-based
   - depsgraph/evaluated data
2. The code
3. Any assumptions about mode, active object, selected objects, Blender version, or UI context
4. A short note on how to run it
5. A warning if the task is destructive or context-sensitive

## Style guide for generated Blender code
- Import only needed modules
- Use clear object/datablock names
- Prefer explicit variable names over one-letter names
- Validate assumptions early with clear exceptions
- Keep context-sensitive code isolated
- Avoid unnecessary selection changes
- Keep add-on UI code separate from core logic
- Comment only where the Blender-specific behavior is non-obvious

## When the agent should refuse or narrow scope
The agent should not bluff when:
- the requested call depends on an unknown Blender version detail
- the task requires a UI context that is unavailable
- the operator name or property names are uncertain
- the user requests a destructive batch action without backups or scope limits

In those cases, the agent should say what is known, identify the uncertainty, and provide the safest partial solution.

## Recommended response template for agent use
```md
Approach: [direct RNA / bmesh / bpy.ops / depsgraph]

Assumptions:
- Blender version: [if relevant]
- Mode: [OBJECT / EDIT_MESH / etc.]
- Active object requirement: [yes/no]
- UI context required: [yes/no]

Code:
```python
# code here
```

Notes:
- [how to run]
- [context or destructive caveats]
- [version caveats if any]
```

## Compact heuristics
- If it sounds like editing data, prefer `bpy.data` / `bpy.types`.
- If it sounds like editing topology, prefer `bmesh`.
- If it sounds like clicking a Blender tool, use `bpy.ops` carefully.
- If modifiers/animation matter, inspect evaluated data through depsgraph.
- If the script depends on selection or active object, set/check them explicitly.
- If the script depends on mode, switch mode deliberately.
- If the task is for automation, avoid UI-bound operators unless unavoidable.

## Summary
Blender scripting is most reliable when the agent:
- uses direct datablock access by default,
- treats context and mode as explicit preconditions,
- reserves operators for true tool actions,
- uses `bmesh` for procedural mesh work,
- uses depsgraph for evaluated results,
- and clearly states assumptions instead of guessing.
