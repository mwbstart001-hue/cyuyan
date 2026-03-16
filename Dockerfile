FROM alpine:latest

# 安装 gcc 工具链
RUN apk add --no-cache gcc musl-dev

# 工作目录
WORKDIR /app

# 拷贝源代码和测试数据
COPY yuyi.c /app/
COPY tests /app/tests/

# 编译源代码
RUN gcc yuyi.c -o yuyi

# 默认运行所有的测试用例
CMD ["sh", "-c", "for i in 1 2 3 4 5; do echo \"=== Case tests/test$i.txt ===\"; ./yuyi tests/test$i.txt; echo \"\"; done"]
