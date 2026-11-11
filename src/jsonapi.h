// jsonapi.h —— 命令行 JSON 接口：给 Agent / 脚本用的无界面编辑与导出通道
//
//   CounterForge.exe --json spec.json [--out 目录] [--result result.json]
//   CounterForge.exe --json -            （从标准输入读 spec）
//   CounterForge.exe --schema            （打印机器可读的字段/枚举表）
//
// 详见 README「JSON 接口」一节。
#pragma once

#include <QStringList>

namespace cf {

// 处理 --json / --schema 分支；返回进程退出码（0 成功，2 参数或数据错误）
int runJsonApi(const QStringList &args);

} // namespace cf
