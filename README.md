# Virtools Script Deobfuscation

[中文文档](README_ZH.md)

## What's this anyway?

This project is the result of some reverse engineering of Virtools.
It comes as a CK2 plugin.

The CK2 plugin adds the following two building blocks to Virtools:
 - BBDecoder  
 A modifed version of Object Load which tries to reveal editing
 interface for all "hidden scripts".
 - FreeBlock  
 This block does nothing other than helping the reverse engineering
 process. You can add as many bIO/pIO's as you want.

## How does it work?

When you save a composition file with the "hide script representation
in schematic view" option disabled, Virtools will save extra
information together with the script logic. The extra information saved
includes  dimension and placement of building blocks, shape of behavior
links and parameter links etc. It is obvious that these data are vital
to display the script schematic correctly in Virtools.

If a composition file is saved with the previously mentioned option
enabled, scripts will be saved without this information, leaving the
script logic alone. This effectively protects the script from being
copied easily. However such protection is similiar to obfuscated Java
bytecode -- it is possible to produce human-readable script from an
obfuscated one, hence the name "Virtools Script Deobfuscation".

What this plugin does is fairly simple: it adds the information required
to display the script back. That's it!

## How to use it?

Create a new Virtools file and add a script. Then, drag BBDecoder
(Located in category `Custom/VirtoolsScriptDeobfuscation`) into
Schematic View. Connect script start arrow with BBDecoder's In 0.
Now, double-click to edit the only Parameter and set it as the file
which you want to decode and load (Editable format needed. Use tools
to convert VMO file before executing). At last click Run button at the
right-bottom corner of Virtools window. The decoded result will be
written in current document.

## Build

### Environment

* You need the SDK component of Virtools to build this project.
* At least Visual Studio 2017. Visual Studio 2019 and 2022 suggested.

### Quick Build

This method is suit for beginner, especially for who just want to use
this project.

0. Use Git to clone this repository in the folder of Virtools SDK 
example Behaviors (eg. 
`Virtools Dev 3.5\Sdk\Samples\Behaviors\VirtoolsScriptDeobfuscation`)
1. Copy `VirtoolsScriptDeobfuscation.props.template` and rename it
as `VirtoolsScriptDeobfuscation.props`
2. Open `VirtoolsScriptDeobfuscation.sln` with Visual Studio.
3. Build under **Release** mode.

### Multi-target Build

Quick Build borrow the attached projects of Virtools SDK to
compile itself. If you don't like this, or you need compile for
different Virtools version. You should follow these steps.

0. Use Git to clone this repository in anywhere you like.
1. Copy `VirtoolsScriptDeobfuscation.props.template` and rename it
as `VirtoolsScriptDeobfuscation.props`
2. Edit `VirtoolsScriptDeobfuscation.props`. Set macros with
proper values.
3. Open `VirtoolsScriptDeobfuscation.sln` with Visual Studio.
4. Build under **Release** mode.
5. Repeat step 2 to 4 until all targets has been built.

An example macros defination of `VirtoolsScriptDeobfuscation.props`
is here.

```xml
<VIRTOOLS_PATH>E:\Virtools\Virtools Dev 5.0</VIRTOOLS_PATH>
<COMPILE_TEMP_PATH>Temp</COMPILE_TEMP_PATH>
<VIRTOOLS_INCLUDE_PATH>E:\Virtools\Virtools Dev 5.0\Sdk\Includes</VIRTOOLS_INCLUDE_PATH>
<VIRTOOLS_LIB_PATH>E:\Virtools\Virtools Dev 5.0\Sdk\Lib\Win32\Release</VIRTOOLS_LIB_PATH>
```

* VIRTOOLS_PATH: Path to Virtools root folder.
* COMPILE_TEMP_PATH: Path to compiler temporary folder.
* VIRTOOLS_INCLUDE_PATH: Path to Virtools SDK Include folder.
* VIRTOOLS_LIB_PATH: Path to Virtools SDK Lib folder.

## Log and Debug

This plugin have log system tracking the internal data of script. Log
system will spend IO and disk space. So it is only enabled in **Debug**
mode in default and served for debugging mainly.  
Log system is actually enabled by macro `VSD_ENABLE_LOG` during
building. You also can directly define this macro to enable log system
forcely as you wish.

The difference with old version is that new version use Virtools
Temporary Folder to log data. This address is dynamic. However, it
will be printed in Debug Window and Virtools Log Window. You
can easily find it.  
According to Virtools' design, this folder will be clean
automatically by Virtools. However, this folder may still in there if
Virtools crashed or Visual Studio terminate Virtools during debugging.
Developer should clean it manually.

The performance of this plugin will significantly drop when using Debug
mode. So do not use the plugin compiled with Debug mode in production
environment. When a bug occurs, we also highly recommend you create a
minimalist reproducing file to reduce the time consumption of debugging.

## Notice

- The data structures are reverse engineered and tested against Virtools
Dev 3.5. However after some tests, this plugin may work correctly on
any Virtools version.
- Check for missing DLLs before you decode a script. If a script contain
parameter types unknown to Virtoos, the resulting script might be
unusable.
- Due to a limitation of Virtools, level scripts are currently not
loaded into the scene.
