# Contributing to XylarJava

Thanks for helping out! Bug reports, ideas and pull requests are all welcome.
For questions, the quickest place is the [Xylar Discord](https://discord.gg/CQ6Et3Kws7).

## Building

The launcher is C++ with Qt 6 and CMake. On Linux:

```bash
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

`scripts/build-linux-appimage.sh` builds a portable AppImage (it needs `appimagetool`).
On Windows use the `windows_msvc` or `windows_mingw` preset.

XylarJava Client, the 1.8.9 PvP client, lives in `client/`. See [client/README.md](client/README.md) to build it.

## Pull requests

- Keep each pull request about one thing, and describe what it changes and how you tested it.
- Make sure the launcher builds and the tests pass.
- Don't commit build output, binaries or secrets (`auth-settings.ini` stays local).

## Code style

All files are formatted with `clang-format` using the configuration in `.clang-format`. Ensure it is run on changed files before committing!

Please also follow the project's conventions for C++:

- Class and type names should be formatted as `PascalCase`: `MyClass`.
- Private or protected class data members should be formatted as `camelCase` prefixed with `m_`: `m_myCounter`.
- Private or protected `static` class data members should be formatted as `camelCase` prefixed with `s_`: `s_instance`.
- Public class data members should be formatted as `camelCase` without the prefix: `dateOfBirth`.
- Public, private or protected `static const` class data members should be formatted as `SCREAMING_SNAKE_CASE`: `MAX_VALUE`.
- Class function members should be formatted as `camelCase` without a prefix: `incrementCounter`.
- Global functions and non-`const` global variables should be formatted as `camelCase` without a prefix: `globalData`.
- `const` global variables and macros should be formatted as `SCREAMING_SNAKE_CASE`: `LIGHT_GRAY`.
- enum constants should be formatted as `PascalCase`: `CamelusBactrianus`
- Avoid inventing acronyms or abbreviations especially for a name of multiple words - like `tp` for `texturePack`.
- Avoid using `[[nodiscard]]` unless ignoring the return value is likely to cause a bug in cases such as:
  - A function allocates memory or another resource and the caller needs to clean it up.
  - A function has side effects and an error status is returned.
  - A function is likely be mistaken for having side effects.
- A plain getter is unlikely to cause confusion and adding `[[nodiscard]]` can create clutter and inconsistency.

Most of these rules are included in the `.clang-tidy` file, so you can run `clang-tidy` to check for any violations.

Here is what these conventions with the formatting configuration look like:

```c++
#define AWESOMENESS 10

constexpr double PI = 3.14159;

enum class PizzaToppings { HamAndPineapple, OreoAndKetchup };

struct Person {
    QString name;
    QDateTime dateOfBirth;

    long daysOld() const { return dateOfBirth.daysTo(QDateTime::currentDateTime()); }
};

class ImportantClass {
   public:
    void incrementCounter()
    {
        if (m_counter + 1 > MAX_COUNTER_VALUE)
            throw std::runtime_error("Counter has reached limit!");

        ++m_counter;
    }

    int counter() const { return m_counter; }

   private:
    static constexpr int MAX_COUNTER_VALUE = 100;
    int m_counter;
};

ImportantClass importantClassInstance;
```

If you see any names which do not follow these conventions, it is preferred that you leave them be - renames increase the number of changes therefore make reviewing harder and make your PR more prone to conflicts. However, if you're refactoring a whole class anyway, it's fine.
