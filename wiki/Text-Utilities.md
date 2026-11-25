# Text utilities

## UTF-8 strings

`ustring` and `ustring_view` are validated UTF-8 types. Mutation methods use
code-point indices and return `kei::result<>` rather than splitting a code
point. The widgets and runtime use plain `std::string` values for widget text,
and the editing helpers in `sleet/widget/control.hh` keep byte offsets on code
-point boundaries.

```cpp
#include <sleet/text/ustring.hh>

auto text = sleet::txt::ustring::from_utf8("Hello, world!");
if (!text) { /* invalid UTF-8 */ }
```

## Line breaking and wrapping

`txt::break_opportunity::of` exposes libunibreak's Unicode line-break
opportunities. `txt::wrap` splits text at the last break that fits while
preserving mandatory newlines. See [[Layout]] for how wrapping integrates with
layout and [[Rendering-and-Themes]] for text rasterization through SDL3_ttf.
