# Smol

Read code for [`lite`](https://github.com/rxi/lite) to learn how to build a good IDE.

## Geometry of text editing

- Manifold/Simplicial Complex: Editor space (`Loc`)
- Tangent vector/stalk: Characters (`char` / `string`)

## Associated Programming Language

A tiny language with tiny memory footprint for human size **prototyping**.
- Functions and dicationries are interchangable. because `smol`.
- immutable, purely functional. Inbuilt data structures for set, map, list. Because `smol`.
- Forces normal form. Only variables can occur at binding sites. Because `smol`.
- Only small types allowed: A type may have at most `4096` inhabitants.
- All data is represented with succinct data structures. Can do for `smol`.
- Debugger which keeps a trace of the entire history of the program.
  Can do this because `smol`.
- Common lisp-like REPL interface that permits hot reloading. Maintains
  history across reloads. Is possible because `smol`.
- Can exhaustively check invariants about function spaces. Because `smol`.
- Zombie: https://www.seas.upenn.edu/~sweirich/papers/congruence-extended.pdf


- https://github.com/benwr/glean
- https://github.com/Kindelia/HVM/blob/master/HOW.md
- https://github.com/lucy/tewi-font

##### Common Lisp IDE information

- [swank:encode-message](https://github.com/slime/slime/blob/c5342a3086367c371e8d88b3140e6db070365d43/swank.lisp#L865-L870)
- [swank:write-message](https://github.com/slime/slime/blob/68c58c0194ff03cd147fcec99f0ee90ba9178875/swank/rpc.lisp#L100-L107)
