
This document outlines coding guidelines for contributions to the [Unreal Live Link for Maya](https://github.com/Autodesk/LiveLink/) project.

# C++ Coding Guidelines
## Foundation/Critical
### License notice
Every file should start with the MIT licensing statement:
```cpp
// MIT License
//
// Copyright (c) 2022 Autodesk 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
```

### Copyright notice
* Every file should contain at least one Copyright line at the top, which can be to Autodesk or the individual/company that contributed the code.
* Multiple copyright lines are allowed, and if a significant new contribution is made to an existing file then an individual or company can append a new line to the copyright section.
* The year the original contribution is made should be included but there is no requirement to update this every year.
* There is no requirement that an Autodesk copyright line should be in all files. If an individual or company contributes new files they do not have to add both their name and Autodesk's name.
* If existing code is being refactored or moved within the repo then the original copyright lines should be maintained. You should only append a new copyright line if significant new changes are added. For example, if some utility code is being moved from a plugin into a common area, and the work involved is only minor name changes or include path updates, then the original copyright should be maintained. In this case, there is no requirement to add a new copyright for the person that handled the refactoring.

### Unreal Engine coding standard
The code follows the Unreal Engine coding standard found [here](https://docs.unrealengine.com/4.27/en-US/ProductionPipelines/DevelopmentSetup/CodingStandard/)

### Naming (file, type, variable, constant, function, namespace, macro, template parameters, schema names)
**General Naming Rules**
The plugin strives to use “camel case” naming. That is, each word is capitalized, except possibly the first word:
* UpperCamelCase
* lowerCamelCase

While underscores in names (_) (e.g., as separator) are not strictly forbidden, they are strongly discouraged.
Optimize for readability by selecting names that are clear to others (e.g., people on different teams.)
Use names that describe the purpose or intent of the object. Do not worry about saving horizontal space. It is more important to make your code easily understandable by others. Minimize the use of abbreviations that would likely be unknown to someone outside your project (especially acronyms and initialisms).

**File Names**
Filenames should be UpperCamelCase and should not include underscores (_) or dashes (-).
C++ files should end in .cpp and header files should end in .h.
In general, make your filenames as specific as possible. For example:
```
CameraSubject.cpp
CameraSubject.h
```

**Type Names**
All type names (i.e., classes, structs, type aliases, enums, and type template parameters) should use UpperCamelCase, with no underscores. For example:
```cpp
class CameraSubject;
enum Roles;
```

**Variable Names**
Variables names, including function parameters and data members should use UpperCamelCase, with no underscores. For example:
```cpp
const MDagPath& DagPath
const MVector& RayDirection
bool* DrawRenderPurpose
```

**Class/Struct Data Members**
Non-static data members of classes/structs are named like ordinary non-member variables in UpperCamelCase. For example:
```cpp
MVector Position;
std::map<int, MBoundingBox> BoundingBoxCache;
```

**Function/Method Names**
All functions should be UpperCamelCase. For example:
```cpp
MString Name() const override;
void RegisterExitCallback();
```

**Namespace Names**
Namespace names should be UpperCamelCase. Top-level namespace names are based on the project name.
```cpp
namespace MayaAttrs {}
```

**Macro Names**
In general, macros should be avoided (see [Modern C++](https://docs.google.com/document/d/1Jvbpfh2WNzHxGQtjqctZ1K1lnpaAtHOUwm0kmmEcxjY/edit#heading=h.ynbggnv41p3) ). However, if they are absolutely needed, macros should be all capitals, with words separated by underscores.
```cpp
#define ROUND(x) …
#define PI_ROUNDED 3.0
```

### Namespaces

#### In header files (e.g. .h)

* **Required:** to use fully qualified namespace names. Global scope using directives are not allowed.  Inline code can use using directives in implementations, within a scope, when there is no other choice (e.g. when using macros, which are not namespaced).

```cpp
// In File.h
void File::PrintHelloWorld()
{
    using namespace std;
    cout << "Hello World" << endl;
}
```

#### In implementation files (e.g. .cpp)

* **Recommended:** to use fully qualified namespace names, unless clarity or readability is degraded by use of explicit namespaces, in which case a using directive is acceptable.
* **Recommended:** to use the existing namespace style, and not make gratuitous changes. If the file is using explicit namespaces, new code should follow this style, unless the changes are so significant that clarity or readability is degraded.  If the file has one or more using directives, new code should follow this style.

### Include directive
For source files (.cpp) with an associated header file (.h) that resides in the same directory, it should be `#include`'d with double quotes and no path.  This formatting should be followed regardless of with whether the associated header is public or private. For example:
```cpp
// In Foobar.cpp
#include "Foobar.h"
```

All included public header files from outside the project should be `#include`’d using angle brackets using the `THIRD_PARTY_INCLUDES_START/_END` macro. For example:
```cpp
THIRD_PARTY_INCLUDES_START
#include <maya/MAnimMessage.h>
THIRD_PARTY_INCLUDES_END
```

Private project’s header files should be `#include`'d using double quotes, and a relative path. Private headers may live in the same directory or sub-directories, but they should never be included using "._" or ".._" as part of a relative path. For example:
```cpp
#include "PrivateUtils.h"
#include "Pvt/HelperFunctions.h"
```

### Include order
Headers should be included in the following order, with each section separated by a blank line and files sorted alphabetically:

1. Related header
2. All private headers
3. All public headers from this repository
4. Unreal Engine headers
5. Autodesk + Maya headers
6. Other libraries' headers
7. C++ standard library headers
8. C system headers
9. Conditional includes

```cpp
#include "LiveLinkProvider.h"
 
THIRD_PARTY_INCLUDES_START
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
THIRD_PARTY_INCLUDES_END

#include <string>

#ifndef MAYA_OLD_PLUGIN
#include <maya/MCameraMessage.h>
#endif // MAYA_OLD_PLUGIN
```

## Cross-compiling Linux plugin on Windows
It's possible to cross-compile the Linux plugin on a Windows machine.

You need to install the necessary Clang cross-compiler found [here](
https://docs.unrealengine.com/4.27/en-US/SharingAndReleasing/Linux/GettingStarted/)

You must also build the Unreal Engine (even on Windows) from source code after merging this [PR](https://github.com/EpicGames/UnrealEngine/pull/8921).

## Cross-platform development
* Consult the [build.md](build.md) compatibility table to ensure you use the recommended tool (i.e., compiler, operating system, etc.) versions.
* Before pull requests (PRs) are considered for integration, they must compile and all tests should pass on all supported platforms.

## std over boost
The Boost library should not be used.

Only use functions and classes using C++ 14 or lower.

## Modern C++
Our goal is to develop the plugin following modern C++ practices. We’ll follow the [C++ Core Guidelines](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) and pay attention to:
* `using` (vs `typedef`) keyword
* `virtual`, `override` and `final` keyword
* `default` and `delete` keywords
* `auto` keyword
* initialization - `{}`
* `nullptr` keyword
* …

<br>

# Coding guidelines for Python
Python classes use UpperCamelCase while functions use lowerCamelCase.

## Unit Test
1. Use the provided framework found in the `test` folder.
2. To add a new test, create a file with its name starting `test_`.
3. Use the included tests to learn how to use the test framework.
