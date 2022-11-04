# lexical scanner 说明文档

## 注释的处理

开始状态`COMMENT`表示当前处于注释中，变量`comment_level_`表示嵌套注释的层级，初始值为1。

1. `INITIAL`状态中，识别到`/*`进入`COMMENT`状态，并将`comment_level_`加1。

2. `COMMENT`状态中，识别到`/*`将`comment_level_`加1；识别到`*/`将comment_level_减1，当`comment_level_`为1时，返回`INITIAL`状态；识别到其他token不做处理。

## 字符串的处理

开始状态`STR`表示当前处于字符串中，开始状态`IGNORE`表示当前处于字符串被忽略的部分中，变量`strlen_dif_`表示输入字符串与转义后字符串的长度差，便于确定每一个token在输入文件中的实际位置。

在字符串处理中，将每一个识别的token通过`more()`作为下一个token的前缀，字符串识别完成后作为一个整体token一起返回。

1. `INITIAL`状态中，识别到`"`进入`STR`状态，将`strlen_dif_`置1（转义后的字符串不包括双引号）。

2. `STR`状态中，识别到`"`根据`strlen_dif_`调整`tok_pos_`和`char_pos_`的位置，返回`INITIAL`状态，并返回整个字符串token；识别到转义字符通过`setMatched()`重置该字符，修改`strlen_dif_`；识别到跟有格式符的`\`时进入`IGNORE`状态；识别到其他字符仅作`more()`处理。

3. `IGNORE`状态中，识别到`\`返回到`STR`状态；识别到格式符通过`setMatched()`去掉当前字符，再将其余部分传递下去；识别到其他字符报错。

## 错误处理

`INITIAL`状态中，识别到不能匹配任何表达式的字符时，报错 **illegal token**。

检测到未闭合的注释/字符串/被忽略的字符串时，报错 **unclosed comment/string/ignore string**。

检测到不合法字符串时，报错 **illegal string**。  
判断方法：增加变量`str_error_`，表示当前字符串是否合法。在识别到字符串时，将其置`false`，当遇到不合法字符（如超出范围的ascii码或`\f..f\`中除格式符外的其他字符）时，将其置`true`。字符串结束时，如`str_error_`为`true`，则报错，否则返回该字符串token。

## 文件结束处理

在`COMMENT/STRING/IGNORE`状态中，增加`<<EOF>>`的处理：报错**unclosed comment/string/ignore string**，并返回`INITIAL`状态。结束识别。

## 其他特性

检测到未闭合的注释/字符串/忽略的字符串时，错误信息可以定位到第一个`/*`、`"`、`\`，方便寻找该错误出现在何处。

遇到错误的token（无论该token是普通的id、数字等还是字符串）时，报错信息可以精确定位。该token不会整体返回，但不影响后续token的识别和定位。