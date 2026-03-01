# Flow Language — VS Code Extension

Official language support for **Flow**, a modern compiled programming language designed for clarity, speed, and full-stack development.

---

## Features

- **Syntax highlighting** for `.flow` files
- **File icons** — `.flow` files get a distinct icon in the explorer
- **Bracket matching** and **auto-closing pairs**
- **Comment toggling** with `//` and `/* */`

---

## Language Overview

Flow is a statically-typed language with a clean, expressive syntax. It compiles to native binaries and **WebAssembly**, making it suitable for both desktop tools and web applications.

```flow
fn main() {
    print("Hello, Flow!")
}

@client
component App() {
    count = 0

    return Column {
        Text {
            value: "Count: {count}"
            style: style({ size: 24, weight: "bold" })
        }
        Button {
            text: "Increment"
            style: style({ bg: "#3b82f6", color: "#fff", radius: 8 })
        }
    }
}
```

---

## Getting Started

Install the [Flow compiler](https://github.com/flow-lang/flow) and run:

```bash
flow new my-app
cd my-app
flow dev
```

---

## More Information

- [Flow Language Repository](https://github.com/flow-lang/flow)
- Report issues directly in the repository
