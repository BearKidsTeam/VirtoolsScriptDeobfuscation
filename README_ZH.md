# Virtools 脚本反混淆

[English document](README.md)

## 这是什么？

这是最近对Virtools进行的反向工程一部分成果。它可以将在Virtools中显示为`--Script Hidden--`的脚本变回可查看和编辑的形式。它以一个CK2插件的形式被提供。

此CK2插件在Virtools中增加了以下两个Building Block：
 - BBDecoder  
 一个Object Load的修改版本，它将试图展示所有`--Script Hidden--`的编辑界面
 - FreeBlock  
 此模块除了帮助反向工程进行以外不能做任何事情。您可以根据需要给它增添任意多个bIO与pIO。


## 它如何工作？

当保存Virtools文件时启用了`hide script representation in schematic view`（隐藏脚本）选项时，Virtools会丢弃脚本中行为模块的位置和尺寸数据，只保存脚本逻辑。显而易见，这些被丢弃的数据对于让Virtools正确展示脚本原理图非常重要。启用隐藏脚本选项，将有效地保护脚本被轻易地复制。然而这种保护有些类似于Java的字节码混淆——可以通过一些手段将脚本恢复成人类可读的形态。这也是本工程`Virtools Script Deobfuscation`名字的由来。

这个插件做的事情非常简单：重新生成已被丢弃的尺寸和位置数据就可以了。

## 运行

新建一个Virtools文件，随意添加一个脚本，拖拽BBDecoder（位于`Custom/VirtoolsScriptDeobfuscation`分类）于Schematic界面之上，将脚本起始点与BBDecoder的In 0进行拖拽连接。然后双击编辑仅有的一个Parameter，将其设置为需要进行逆向的文件的地址（需要是可编辑的格式，VMO格式请先使用其它工具转为可编辑文件再执行），然后点击右下角运行脚本即可开始反编译。反编译的结果将即时被写入当前文档。

## 编译

### 环境

* 你需要Virtools的SDK组件来编译这个项目
* 至少是Visual Studio 2017。建议使用Visual Studio 2019或2022.

### 快速编译

快速编译法适用于初学者，且仅仅是想使用此工程。

0. 在Virtools SDK目录的示例Behaviors目录下使用Git克隆本项目，例如克隆后的目录：`Virtools Dev 3.5\Sdk\Samples\Behaviors\VirtoolsScriptDeobfuscation`
1. 复制文件`VirtoolsScriptDeobfuscation.props.template`并重命名为`VirtoolsScriptDeobfuscation.props`
2. 使用Visual Studio打开`VirtoolsScriptDeobfuscation.sln`。
3. 在**Release**模式下编译。

### 多目标编译

快速编译法借用了Virtools SDK自带的示例项目来进行编译，如果您不想借用Virtools自带的示例项目，或需要针对不同的Virtools版本进行多目标编译，则需要遵循以下步骤。

0. 在您偏好的位置使用Git克隆本项目。
1. 复制文件`VirtoolsScriptDeobfuscation.props.template`并重命名为`VirtoolsScriptDeobfuscation.props`
2. 编辑文件`VirtoolsScriptDeobfuscation.props`，将其中的宏指向正确的位置。
3. 使用Visual Studio打开`VirtoolsScriptDeobfuscation.sln`。
4. 在**Release**模式下编译。
5. 如果还有其它目标需要编译，重复2-4步骤直至所有目标都被编译。

一份`VirtoolsScriptDeobfuscation.props`宏示例如下：

```xml
<VIRTOOLS_PATH>E:\Virtools\Virtools Dev 5.0</VIRTOOLS_PATH>
<COMPILE_TEMP_PATH>Temp</COMPILE_TEMP_PATH>
<VIRTOOLS_INCLUDE_PATH>E:\Virtools\Virtools Dev 5.0\Sdk\Includes</VIRTOOLS_INCLUDE_PATH>
<VIRTOOLS_LIB_PATH>E:\Virtools\Virtools Dev 5.0\Sdk\Lib\Win32\Release</VIRTOOLS_LIB_PATH>
```

* VIRTOOLS_PATH：Virtools的安装目录
* COMPILE_TEMP_PATH：编译期间临时文件存放的文件夹
* VIRTOOLS_INCLUDE_PATH：Virtools SDK的头文件目录
* VIRTOOLS_LIB_PATH：Virtools SDK的链接库目录

## 日志与调试

本插件具有日志记录功能，以方便追踪生成的脚本的内部数据。日志功能会消耗IO以及磁盘空间，因此只在**Debug**模式下开启，旨在为开发者调试本插件所用。  
日志记录功能的启用实际上由编译期间的宏`VSD_ENABLE_LOG`控制。如果您需要，可以直接定义此宏以在其它编译模式中强制开启日志记录功能。

与旧版本不同，新版本插件使用Virtools自带的临时目录去记录日志，其地址不固定，但会被打印在调试窗口以及Virtools日志窗口中，以供开发者查找。  
这个临时目录在设计上会被Virtools自动清理。但在Virtools崩溃或被Visual Studio强制终止调试时，此目录则不会被清理，需要开发者手动进行清理。

Debug模式下，插件运行速度将会大大降低，请不要将Debug模式编译出的程序用于生产环境。为了减少Debug模式下的运行时间，我们建议您创建最小重现文件。

## 注意

- 整个反向工程过程均以Virtools Dev 3.5为目标。但经过测试，在所有Virtools版本中均可使用。
- 如果脚本中包含Virtools未知的类型的参数，生成的脚本可能无法使用。
- Level script目前会被无视。
