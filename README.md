# Virtools Script Deobfuscation

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

## Build

0. You need the SDK component of Virtools Dev 3.5 to build this project
1. Put everything in Virtools Dev 3.5\Sdk\Samples\Behaviors\Custom
2. Add Custom.vcxproj to Behaviors.sln
3. Build with VS2017 under Debug mode

## Notice

- The data structures are reverse engineered and tested against Virtools
Dev 3.5. It may not work in other Virtools versions.
- Check for missing DLLs before you decode a script. If a script contain
parameter types unknown to Virtoos, the resulting script might be
unusable.
- Due to a limitation of Virtools, level scripts are currently not
loaded into the scene.

# Virtools 脚本反混淆

中文readme内容不完整，如果你有能力请阅读英文版。

## 这是什么？
这是最近对Virtools进行的反向工程一部分成果。它可以将在Virtools中显示为
--Script Hidden--的脚本变回可查看和编辑的形式。

## 原理？
当启用隐藏脚本选项时，Virtools会丢弃脚本中行为模块的位置和尺寸数据。
这有些类似于Java的字节码混淆——可以通过一些手段将脚本恢复成人类可读的形态。在这里，
我们只需要重新生成这些尺寸和位置数据就可以了。

## 注意

- 整个反向工程过程均以Virtools Dev 3.5为对象。本工程对其他Virtools版本可能不适用。
- 如果脚本中包含Virtools未知的类型的参数，生成的脚本可能无法使用。
- Level script目前会被无视。
