#ifndef IVIEWMODEL_H
#define IVIEWMODEL_H

#include <QObject>

/**
 *  IViewModel —— 全工程 ViewModel 基类约定
 *
 *  设计原则（MVVM）：
 *  1. ViewModel 不依赖任何 QWidget / UI 代码（不能 #include 任何 ui_xxx.h）。
 *  2. 状态以 Q_PROPERTY 暴露，**所有可观察状态必须配 NOTIFY signal**，
 *     View 仅通过 connect(vm, &XxxViewModel::xxxChanged, ...) 更新自己。
 *  3. 用户动作以 public slot 形式暴露（即 Command），View 调用它们而不是直接
 *     调用 Model（PlayerCore / VideoSummaryManager 等）。
 *  4. ViewModel 内部持有 Model（聚合或组合），并负责把 Model 的信号翻译为
 *     ViewModel 层语义（如 currentPos -> positionChanged(ms)）。
 *  5. ViewModel 与 View 一对多，便于复用：同一个 PlayerViewModel 可以同时
 *     绑定到 MainWindow 的进度条/时间标签/按钮多个控件上，互不感知。
 */
class IViewModel : public QObject {
    Q_OBJECT
public:
    explicit IViewModel(QObject* parent = nullptr) : QObject(parent) {}
    ~IViewModel() override = default;
};

#endif // IVIEWMODEL_H
