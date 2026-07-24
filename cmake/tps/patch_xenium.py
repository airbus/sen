"""Add the aarch64 cases xenium is missing.

xenium implements `getticks` for __sparc__, __x86_64__ and _M_AMD64 and #errors
otherwise, at file scope, so the error fires on include even though nothing here
calls it: `getticks` only feeds `utils::random()`, used by the kfifo queues, while
this code uses `harris_michael_list_based_set`. `hardware_pause` has the same gap
and its #warning is promoted by -Werror=cpp.

Patched rather than dropping the arm job, which is the only non-x86 coverage.
Worth sending upstream: the library is missing both for all of aarch64.
"""

import pathlib
import sys

GETTICKS = """#elif defined(__aarch64__)
  static inline std::uint64_t getticks(void) {
      std::uint64_t ret;
      __asm__ volatile("mrs %0, cntvct_el0" : "=r"(ret));
      return ret;
  }
"""

PAUSE = """#elif defined(__aarch64__)
    __asm__ volatile("yield");
"""


def patch(path: pathlib.Path, marker: str, addition: str) -> int:
    """Insert `addition` before `marker`, or report if the marker has moved."""
    text = path.read_text(encoding="utf-8")
    if "__aarch64__" in text:
        return 0
    if marker not in text:
        print(f"patch_xenium: marker not found in {path}", file=sys.stderr)
        return 1
    path.write_text(text.replace(marker, addition + marker), encoding="utf-8")
    return 0


def main() -> int:
    """Patch both headers in the xenium source tree given as the first argument."""
    root = pathlib.Path(sys.argv[1])
    result = patch(
        root / "xenium" / "utils.hpp",
        '#else\n  // TODO - add support for more compilers!\n  #error "Unsupported compiler"',
        GETTICKS,
    )
    result |= patch(
        root / "xenium" / "detail" / "hardware.hpp",
        '#else\n    #warning "No hardware_pause implementation available',
        PAUSE,
    )
    return result


if __name__ == "__main__":
    sys.exit(main())
