This is an automatic translation and may be incorrect in some places. See the source README and examples for authoritative information.

[![latest](https://img.shields.io/github/v/release/GyverLibs/FOR_MACRO.svg?color=brightgreen)](https://github.com/GyverLibs/FOR_MACRO/releases/latest/download/FOR_MACRO.zip)
[![PIO](https://badges.registry.platformio.org/packages/gyverlibs/library/FOR_MACRO.svg)](https://registry.platformio.org/libraries/gyverlibs/FOR_MACRO)
[![Foo](https://img.shields.io/badge/Website-AlexGyver.ru-blue.svg?style=flat-square)](https://alexgyver.ru/)
[![Foo](https://img.shields.io/badge/%E2%82%BD%24%E2%82%AC%20%D0%9F%D0%BE%D0%B4%D0%B4%D0%B5%D1%80%D0%B6%D0%B0%D1%82%D1%8C-%D0%B0%D0%B2%D1%82%D0%BE%D1%80%D0%B0-orange.svg?style=flat-square)](https://alexgyver.ru/support_alex/)
[![Foo](https://img.shields.io/badge/README-ENGLISH-blueviolet.svg?style=flat-square)](https://github-com.translate.goog/GyverLibs/FOR_MACRO?_x_tr_sl=ru&_x_tr_tl=en)  

[![Foo](https://img.shields.io/badge/ПОДПИСАТЬСЯ-НА%20ОБНОВЛЕНИЯ-brightgreen.svg?style=social&logo=telegram&color=blue)](https://t.me/GyverLibs)

# FOR_MACRO
Variadic for macro

### Compatibility
Compatible with all Arduino platforms (Arduino features are used)

## Contents
- [Use of use](#usage)
- [Versions](#versions)
- [Installation](#install)
- [Bugs and feedback](#feedback)

<a id="usage"></a>

## Use of use
`FOR_MACRO`A macro that allows you to apply a different macro to a variadic list of arguments essentially invokes the specified macro for each argument. By default supports a maximum of 512 arguments, you can generate a macro on any number of arguments using the attached Python script.

To use a macro, you need to create two macros of your own - one will be a macro function that applies to each argument, and the second will be the macro itself that will be used. The first macro function should be`F(N, i, p, val)`where:
- `N`- number of arguments
- `i`- counter, begins at the end of 1 due to the peculiarities of implementation
- `p`- parameter
- `val`- current argument

Himself.`FOR_MACRO`You need to call with the following list of arguments in the implementation of your macro:
- `func`previously announced macro-function
- `p`- parameter, maybe anything. Can I put it on?`0`if not
- `__VA_ARGS__`- list of arguments (from the ellipsis)

How this works will be clearer with examples:
```cpp
#define MF1(N, i, p, val) N,
#define FOR_1(...) FOR_MACRO(MF1, 0, __VA_ARGS__)

FOR_1(test, kek, string);   // turns around at 3, 3, 3,
```
```cpp
#define MF2(N, i, p, val) i,
#define FOR_2(...) FOR_MACRO(MF2, 0, __VA_ARGS__)

FOR_2(test, kek, string);   // turn around at 2, 1, 0
```
```cpp
#define MF3(N, i, p, val) p,
#define FOR_3(...) FOR_MACRO(MF3, 0, __VA_ARGS__)

FOR_3(test, kek, string);   // will turn to 0, 0, 0 (parameter)
```
```cpp
#define MF4(N, i, p, val) val,
#define FOR_4(...) FOR_MACRO(MF4, 0, __VA_ARGS__)

FOR_4(test, kek, string);   // It'll turn to test, kek, string,
```
```cpp
#define MF5(N, i, p, val) #val,
#define FOR_5(...) FOR_MACRO(MF5, 0, __VA_ARGS__)

FOR_5(test, kek, string);   // It'll turn into "test," "kek," "string,"
```

A more realistic example is the creation of lines.
```cpp
#define MF6(N, i, p, val) const char* p##_##i = #val;
#define FOR_6(name, ...) FOR_MACRO(MF6, name, __VA_ARGS__)

FOR_6(strings, test, kek, string);
// turn in
// const char* strings_2 = "test"; 
// const char* strings_1 = "kek"; 
// const char* strings_0 = "string";
```
```cpp
#define MF7(N, i, p, val) const char* val = #val;
#define FOR_7(name, ...) FOR_MACRO(MF7, name, __VA_ARGS__)

FOR_7(strings, test, kek, string);
// turn in
// const char* test = "test"; 
// const char* kek = "kek"; 
// const char* string = "string";
```

<a id="versions"></a>

## Versions
- v1.0

<a id="install"></a>
## Installation
- The library can be found under the name **FOR MACRO** and installed through the library manager in:
    - Arduino IDE
    - Arduino IDE v2
    - PlatformIO
- [Download the library](https://github.com/GyverLibs/FOR_MACRO/archive/refs/heads/main.zip).zip archive for manual installation:
    - Unpack and put in *C:\Program Files (x86)\Arduino\libraries* (Windows x64)
    - Unpack and put in *C:\Program Files\Arduino\libraries* (Windows x32)
    - Unpack and put in *Documents/Arduino/libraries/ *
    - (Arduino IDE) Automatic installation from .zip: *Sketch/Connect library/Add .ZIP library...* and specify downloaded archive
- Read more detailed instructions for installing libraries[here](https://alexgyver.ru/arduino-first/#%D0%A3%D1%81%D1%82%D0%B0%D0%BD%D0%BE%D0%B2%D0%BA%D0%B0_%D0%B1%D0%B8%D0%B1%D0%BB%D0%B8%D0%BE%D1%82%D0%B5%D0%BA)
### Update
- I recommend always updating the library: new versions fix errors and bugs, as well as optimize and add new features.
- Through the library manager IDE: find the library as when installing and click "Update"
- Manually: **Delete the folder with the old version** and then put the new one in its place. “Replacement” can not be done: sometimes new versions delete files that will remain when replaced and can lead to errors!

<a id="feedback"></a>

## Bugs and feedback
If you find bugs, create **Issue**, or better write to the mail immediately.[alex@alexgyver.ru](mailto:alex@alexgyver.ru)  
The library is open for revision and your **Pull Requests*!

When reporting bugs or incorrect work of the library, it is necessary to specify:
- Library version
- What is used by the IC
- SDK version (for ESP)
- Arduino IDE version
- Are embedded examples that use features and designs that cause bugs in your code working correctly?
- What code was downloaded, what work was expected from it and how it works in reality
- Ideally, attach the minimum code in which the bug is observed. Not a canvas of a thousand lines, but a minimum code.
