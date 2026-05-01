# Sharp Runtime

**Sharp Runtime** is a C++ reimplementation of a small C#/.NET runtime subset (mainly used by CNA and game ports).

The goal of this project is to provide a lightweight, .NET-inspired foundation layer for C++ projects, with a focus on:

* familiar API design (`System::*`-like namespaces)
* clean and modern C++ implementation
* compatibility with higher-level frameworks (e.g. CNA)

> ⚠️ This is **not** a full .NET runtime or CLR implementation.
> It is a pragmatic subset designed for use in native C++ applications.

---

# 🚀 Goals

* Recreate useful parts of `.NET` API in idiomatic C++
* Provide building blocks such as:

    * exceptions
    * events / delegates
    * basic system types
* Serve as a foundation for higher-level frameworks (e.g. CNA)
* Keep the codebase simple, readable, and well-documented

---

# 🛠️ Build

```bash
cmake -S . -B build
cmake --build build
```

---

# 📊 Implementation Status Convention

This project uses a **documentation-only status system** to track implementation progress of classes and functions.

These statuses are written in Doxygen comments and are not enforced by the compiler.

## Status values

* **Todo** — not implemented yet
* **Stub** — skeleton only, returns placeholder or fails
* **Partial** — partially implemented, may be incomplete
* **Implemented** — functionally complete
* **Verified** — validated against expected .NET behavior

---

# 📝 Comment Format

Each class or function may include a status note:

```cpp
/**
 * @note Status: Partial
 */
```

Full example:

```cpp
/**
 * @brief Provides 2D sprite rendering functionality.
 *
 * @note Status: Partial
 */
class SpriteBatch
{
public:
    /**
     * @brief Begins a sprite drawing batch.
     *
     * @note Status: Implemented
     */
    void Begin();

    /**
     * @brief Draws a texture at the specified position.
     *
     * @note Status: Partial
     */
    void Draw(Texture2D& texture, Vector2 position, Color color);

    /**
     * @brief Ends a sprite drawing batch.
     *
     * @note Status: Todo
     */
    void End();
};
```

---

# 🧠 Design Philosophy

* Prefer clarity over completeness
* Avoid unnecessary complexity
* Keep APIs close to .NET where it makes sense
* Use modern C++ (RAII, strong typing, clear ownership)

---

# ⚠️ Scope

Sharp Runtime intentionally **does not aim to implement:**

* the CLR (Common Language Runtime)
* JIT compilation
* full .NET standard compatibility

Instead, it focuses on a **practical subset** useful for native development.

---

# 🔗 Related Projects

* CNA — C++ reimplementation of XNA 4.0 (built on top of this library)

