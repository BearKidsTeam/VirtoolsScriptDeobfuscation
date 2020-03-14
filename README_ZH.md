# Virtools 脚本反混淆

[English document](README.md)

## 这是什么？

这是最近对Virtools进行的反向工程一部分成果。它可以将在Virtools中显示为`--Script Hidden--`的脚本变回可查看和编辑的形式。它以一个CK2插件的形式被提供。

此CK2插件在Virtools中增加了以下两个Building Block：
 - BBDecoder  
 一个Object Load的修改版本，它将试图展示所有`--Script Hidden--`的编辑界面
 - FreeBlock  
 此模块除了帮助反向工程进行以外不能做任何事情。您可以根据需要给它增添任意多个bIO与pIOT


## 它如何工作？

当保存Virtools文件时启用了`hide script representation in schematic view`（隐藏脚本）选项时，Virtools会丢弃脚本中行为模块的位置和尺寸数据，只保存脚本逻辑。显而易见，这些被丢弃的数据对于让Virtools正确展示脚本原理图非常重要。启用隐藏脚本选项，将有效地保护脚本被轻易地复制。然而这种保护有些类似于Java的字节码混淆——可以通过一些手段将脚本恢复成人类可读的形态。这也是本工程`Virtools Script Deobfuscation`名字的由来。

这个插件做的事情非常简单：重新生成已被丢弃的尺寸和位置数据就可以了。

## 编译

### 编译方法

0. 你需要Virtools Dev 3.5的SDK组件来编译这个项目
1. 将本项目的所有文件放到`Virtools Dev 3.5\Sdk\Samples\Behaviors\Custom`
2. 将Custom.vcxproj添加到Behaviors.sln
3. 使用至少是VS2017的Visual Studio在Debug模式下编译

本工程借用了Virtools SDK自带的示例项目来进行编译，如果需要在别处编译，需要先通读Virtools SDK手册，理解一个Building Block应该如何被编译。然后按手册重新配置项目的附加包含目录和附加库目录等。

### 参数调整

打开`precomp.h`文件，找到如下语句`#define base_path "C:\\Users\\jjy\\Desktop\\test"`

需要修改此宏定义，将其指向到一个认为合适的目录。此目录将存放此插件在执行时的日志文件。此外，需要再选定的目录下新建两个文件夹：`generator`和`parser`，否则脚本执行时会出现IO错误。

如果不修改此宏定义，在Virtools中引用并运行含有此插件的脚本时也会出现IO错误。

## 运行

新建一个Virtools文件，随意添加一个脚本，拖拽此BB于VSL之上，将脚本起始点与此BB的In 0进行拖拽连接。然后双击编辑仅有的一个Parameter，将其设置为需要进行逆向的文件的地址（需要是可编辑的格式，VMO格式请先使用其它工具转为可编辑文件再执行），然后点击右下角运行脚本即可开始反编译。反编译的结果将即时被写入当前文档。

注意：无论是在调试此BB还是在使用此BB时，之前在代码参数调整中设置的日志文件存放目录都不可被删除，因此最好选择一个合适的日志存储位置再进行编译。

## 注意

- 整个反向工程过程均以Virtools Dev 3.5为对象。本工程对其他Virtools版本可能不适用。
- 如果脚本中包含Virtools未知的类型的参数，生成的脚本可能无法使用。
- Level script目前会被无视。
