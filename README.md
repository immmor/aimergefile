# aimerge

一个用于合并文件和显示目录结构的C程序

## 功能

- 显示当前目录的树状结构
- 将多个文件内容合并到一个输出文件中
- 支持自定义输出文件名

## 使用方法

```bash
./aimerge [-d output_file] input_file1 input_file2 ...
```

### 参数说明

- `-d output_file`: 指定输出文件名(默认为aimerged.txt)
- `input_file1 input_file2 ...`: 要合并的输入文件列表

## 示例

```bash
# 显示当前目录结构并合并file1.txt和file2.txt
./aimerge file1.txt file2.txt

# 指定输出文件名为merged.txt
./aimerge -d merged.txt file1.txt file2.txt
```

## 输出格式

输出文件包含:
1. 当前目录的树状结构
2. 每个输入文件的内容，前面带有文件名和分隔线

## 编译

```bash
gcc aimerge.c -o aimerge
```