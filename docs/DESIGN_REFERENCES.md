# RelayCore design and engineering references

Read for gameplay/system/level design or architecture decisions that need design rationale. This is background knowledge, not a mandatory reading list or an implementation queue.

## Design and engineering knowledge

- Apply relevant game design, systems design, level design, engine architecture, mathematics, graphics, collision detection, and tools UX knowledge to concrete project decisions. Use the references below as a foundation, not an exhaustive list or a mandatory reading sequence for every task.
- Draw on other relevant books, primary research, official documentation, production experience, and project evidence when they answer a concrete question. Do not assume a technique fits RelayCore merely because a respected source describes it.
- Game design references: Jesse Schell, *The Art of Game Design*; Tynan Sylvester, *Designing Games*; Richard Lemarchand, *A Playful Production Process*; Steve Swink, *Game Feel*; Katie Salen and Eric Zimmerman, *Rules of Play*.
- Systems design references: Michael Sellers, *Advanced Game Design: A Systems Approach*; Ernest Adams and Joris Dormans, *Game Mechanics: Advanced Game Design*; Christopher Barney, *Pattern Language for Game Design*; Geoffrey Engelstein and Isaac Shalev, *Building Blocks of Tabletop Game Design*.
- Level design references: Benjamin Bauer, *A Practical Guide to Level Design*; Christopher W. Totten, *An Architectural Approach to Level Design*; Scott Rogers, *Level Up! The Guide to Great Video Game Design*.
- Engine and tools references: Jason Gregory, *Game Engine Architecture* (foundations/core systems and graphics/motion/sound); Robert Nystrom, *Game Programming Patterns*; David Lightbown, *Designing the User Experience of Game Development Tools*.
- Mathematics, graphics, and collision references: Eric Lengyel, *Foundations of Game Engine Development*, Volumes 1 and 2; Steve Marschner and Peter Shirley, *Fundamentals of Computer Graphics*; Tomas Akenine-Moller et al., *Real-Time Rendering*; Matt Pharr, Wenzel Jakob, and Greg Humphreys, *Physically Based Rendering*; Christer Ericson, *Real-Time Collision Detection*.
- Start gameplay design from the intended player experience and observable player decisions. Evaluate mechanics together with resources, feedback loops, enemy behavior, and level layout; verify their interactions through a working slice and focused playtests.
- Treat control response, acceleration, camera motion, action timing, and audiovisual feedback as part of gameplay behavior. Physical correctness alone does not establish good game feel.
- Establish routes, sightlines, cover, landmarks, encounter pacing, and learning opportunities during blockout. Validate spatial readability and combat behavior before adding visual detail.
- Use explicit ownership, update order, coordinate conventions, units, and numerical assumptions in technical work. Choose architecture and patterns to solve demonstrated problems at the project's current scale.
- When a principle materially affects a decision, briefly explain the concrete benefit and tradeoff. Validate with the appropriate code checks, runtime observation, playtests, or measurements. Do not invent quotations, page references, or edition-specific claims; verify exact attribution when needed.
- When design principles conflict, prefer the option that reduces demonstrated change cost in this codebase. Explain the concrete benefit and tradeoff.

## Gameplay types and interfaces

- Shared operations do not automatically require inheritance or a virtual interface. Start with a concrete implementation; represent weapon or enemy variants that differ only in parameters through typed data using existing project conventions.
- Separate behavior when actual variants require different logic. Introduce a common interface when callers need to use those implementations through a meaningful shared contract. For a small closed set of variants, an enum and direct dispatch may be simpler; choose based on current call sites and ownership.
- Keep contracts specific to capabilities and responsibilities. Weapon firing and reloading belong to weapon behavior; enemy decision-making belongs to enemy behavior. Do not force every enemy or gameplay object to implement unrelated methods, or create a separate interface for each individual weapon or enemy.
- Add abstractions incrementally when they remove real duplication, encode an invariant, or clarify ownership. Do not build a hierarchy or extensibility framework for hypothetical future content.
