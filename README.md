# Tiger Compiler Labs in C++

## Contents

- [Tiger Compiler Labs in C++](#tiger-compiler-labs-in-c)
  - [Contents](#contents)
  - [Overview](#overview)
  - [Lab 1: Straight-line Program Interpreter](#lab-1-straight-line-program-interpreter)
  - [Lab 2: Lexical Analysis](#lab-2-lexical-analysis)
  - [Lab 3: Parsing](#lab-3-parsing)
  - [Lab 4: Type Checking](#lab-4-type-checking)
  - [Lab 5: Tiger Compiler without register allocation](#lab-5-tiger-compiler-without-register-allocation)
  - [Lab 6: Register Allocation](#lab-6-register-allocation)
  - [Commands](#commands)

## Overview

Course project of SJTU SE3355, 2022.

The project covers a series of labs, ultimately resulting in a complete compiler that translates Tiger language into x86 assembly. 

TAs provided C++ frameworks for these labs, and I implemented the contents.

## Lab 1: Straight-line Program Interpreter

> related files: slp.h/cc

A “warm-up” exercise. Implement a simple program analyzer and interpreter for the straight-line programming language. 

## Lab 2: Lexical Analysis

> related files: src/tiger/lex/*

Use [flexc++](https://fbb-git.github.io/flexcpp/manual/flexc++.html) to **implement a lexical scanner** for the Tiger language.

The lexical scanner for the Tiger language includes the following parts:
- Basic specifications;
- Comments handling;
- Strings handling;
- Error handling;

## Lab 3: Parsing

> related files: src/tiger/absyn/absync.\*, src/tiger/parse/tiger.y, src/tiger/parse/parser.\*,  src/tiger/errormsg/errormsg.\*, src/tiger/symbol/symbol.\*

Use [Bisonc++](https://fbb-git.gitlab.io/bisoncpp/manual/bisonc++.html) to **implement a parser** for the Tiger language.

Bisonc++ is a general-purpose parser generator converting grammar descriptions for LALR(1) context-free grammars into C++ classes whose members can parse such grammars. I wrote the correct rules in tiger.y that describes a parser, which generated a proper abstract syntax tree for every given tiger source file.

## Lab 4: Type Checking

> related files: src/tiger/semant/semant.\*, src/tiger/semant/type.\*, src/tiger/env/env.\*

Write **a type-checking phase** for your compiler, a module **semant** describes this phase.

## Lab 5: Tiger Compiler without register allocation

> related files: src/tiger/frame/.\*, src/tiger/translate/.\*, src/tiger/runtime/runtime.c, src/tiger/env/env.\*, src/tiger/escape/escape.\*, src/tiger/canon/.\*, src/tiger/codegen/.\*, src/tiger/output/output.\*

### Part 1: Escape Analysis and Translation

Do the **escape analysis** and **translation** of tiger compiler, which leads to a complete IR tree.

I worked on the following modules: { x64 stack frame } { IR tree translation} { escape analysis }.

### Part 2: Code Generation

Write a complete runnable tiger compiler, my goal is to make the compiler **generate working code** that runs on x86-64 platform (without register allocation).

I worked on the following modules: { x64 stack frame } { IR tree translation} { code generation }.

## Lab 6: Register Allocation

> related files: src/tiger/liveness/.\*, src/tiger/regalloc/.\*

Finish **register allocation** in the tiger compiler.

The job of the register allocator is to **assign the many temporaries to a small number of machine registers**, and, where possible, to **assign the source and destination of a MOVE to the same register** so that the MOVE can be deleted.

I worked on the following modules: { liveness analysis } { register allocation }.

## Commands

### Installing Dependencies
```bash
# Run this command in the root directory of the project
docker run -it --privileged -p 2222:22 -v $(pwd):/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest  # or make docker-run
```

### Compiling and Debugging

There are five makeable targets in total, including `test_slp`, `test_lex`, `test_parse`, `test_semant`,  and `tiger-compiler`.

1. Run container environment and attach to it

```bash
# Run container and directly attach to it
docker run -it --privileged -p 2222:22 \
    -v $(pwd):/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest  # or `make docker-run`
# Or run container in the backend and attach to it later
docker run -dt --privileged -p 2222:22 \
    -v $(pwd):/home/stu/tiger-compiler ipadsse302/tigerlabs_env:latest
docker attach ${YOUR_CONTAINER_ID}
```

2. Build in the container environment

```bash
mkdir build && cd build && cmake .. && make test_xxx  # or `make build`
```

3. Debug using gdb or any IDEs

```bash
gdb test_xxx # e.g. `gdb test_slp`
```

### Testing and Grading

Use `make`
```bash
make gradelabx
```

You can test all the labs by
```bash
make gradeall
```