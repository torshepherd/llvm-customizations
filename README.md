# Tor's LLVM modifications

This repo is my running contributions and personal changes to `clangd` and `clang-tidy`. I'll try to merge as many of my updates as are accepted upstream to https://github.com/llvm/llvm-project.

## `clangd` features:

### Fix all

![300539949-c419cf60-b689-4e87-8453-fecafba2d81b](https://github.com/torshepherd/llvm-customizations/assets/49597791/42eb465a-0684-47ac-80aa-363b2401cd59)

### swap binary operands

https://github.com/torshepherd/llvm-customizations/assets/49597791/778b35b9-0ab2-4378-b1bb-2bb990cec14d

### Inlay hints for lambda captures

## `clang-tidy` checks:

### `performance-vector-pessimization`

![image](https://github.com/torshepherd/llvm-customizations/assets/49597791/a6792592-53d0-46eb-9769-e1509bacc4b0)

## Planned future work:

- `modernize-use-views-enumerate`: transform index-based for loops with end condition of `.size()` that use the index and also use the value at the index to range-based for loops with `| std::views::enumerate`
- `modernize-use-views-iota`: transform index-based for loops that start from an integral `start` and go to an integral `end` to range-based for loops with `std::views::iota(start, end)`
- `modernize-use-views-zip`: transform index-based for loops with end condition of `.size()` that use the value from two ranges `r1` and `r2` at the index to range-based for loops with `std::views::zip(r1, r2)`
- `modernize-use-views-concat`: transform consecutive range-based for loops with the same element type to a single for loop with `std::views::concat(r1, r2)`
- `modernize-use-ctad`: transform `make_` functions to class-template-argument-deduction: `std::make_optional(2)` -> `std::optional(2)`
  - With config option to use braced-init or paren-init
- `misc-use-static-or-inline-constexpr`: suggest transforming global `constexpr` variables in header files to `inline constexpr` and global variables in source files or function-level variables to `static constexpr`
- `performance-use-array`: In cases with compile-known length of std::vector, change type to array instead
- `bugprone-move-reference-capture`: Suggest move captures for vars captured by reference which are later moved in the lambda iff the lambda is not immediately-invoked
- `bugprone-use-midpoint`: Suggest to use `std::midpoint(x, y)` instead of manual `x + y / 2`
- `performance-initializer-list`: In cases where vector is instantiated with the initializer-list ctor and the type is not trivially copyable, suggest using emplace back instead: `std::vector<T> vec{t1, t2, t3}` -> `std::vector<T> vec = [&]{std::vector<T> out{}; out.emplace_back(t1); out.emplace_back(t2); out.emplace_back(t3); return out;}()`
- Add "swap parameters" code action to clangd
- Add "extract constraints to concept" code action to clangd
- Add "move concept to requires clause" code action to clangd
- Add code action to convert include to forward-declarations
- Extract to lambda instead of function
- Inlay hints for size of included file
- Inlay hints for implicit default initialization
- Inlay hints for deduced template parameters
- Inlay hints for default arguments (and template args)
- Only publish block inlay hints for blocks bigger than a configurable line length
- Convert block comment to multiline comment & vice versa
- Convert member function qualifiers into explicit `this` parameter
- Convert eager monadic functions to lazy versions
- Autocomplete for #include should use fuzzy path finder
- Compute comptime enum underlying values on hover
  - For instance, see `llvm/include/llvm/ADT/SparseBitVector.h`, and hover over the enum values `BITS_PER_ELEMENT`
- Proper schema-based autocomplete for config.yaml
- Make "add missing includes" use the same path for the includes as autocomplete's "from _.h" functionality
- Make `-Wunused-but-set-variable` suggest removing the only-ever-set variable
