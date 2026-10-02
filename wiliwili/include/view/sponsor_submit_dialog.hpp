//
// 提交空降助手片段对话框
//

#pragma once

#include <cstdint>
#include <string>

#include <borealis/core/box.hpp>

#include "utils/event_helper.hpp"

namespace brls {
class Label;
class DetailCell;
class Button;
}  // namespace brls

namespace wiliwili {

/**
 * 「提交空降助手片段」的对话框内容。
 *
 * 与 PlayerCoin 一样，本类自身就是内容视图，由调用方包一层 brls::Dialog：
 *
 *   auto content = new SponsorSubmitDialog(bvid, cid, duration);
 *   auto dialog  = new brls::Dialog(content);
 *   dialog->addButton("hints/cancel"_i18n, [] {});
 *   dialog->open();
 *
 * 交互设计：
 *   - 打开时用「当前播放进度」预填起点与终点。最常见的用法是先把进度拖到片段开头再打开；
 *   - 「起点」「终点」两行点一下就把该项设为当前播放进度。视频在对话框后面继续播放，
 *     所以「标记起点 → 等片段放完 → 标记终点 → 提交」可以一次做完；
 *   - 「分类」打开下拉框选择；
 *   - 提交按钮在内容区（不是 Dialog 的按钮），因为 Dialog 的按钮回调是在 dismiss
 *     之后才执行的，那时本视图可能已经被销毁。
 */
class SponsorSubmitDialog : public brls::Box {
public:
    /**
     * @param bvid          视频 ID
     * @param cid           分 P ID
     * @param videoDuration 视频总时长（秒），<= 0 表示未知
     */
    SponsorSubmitDialog(std::string bvid, int64_t cid, double videoDuration);
    ~SponsorSubmitDialog() override;

    /// 供外部（如 Dialog 的取消按钮）判断是否正在提交，避免重复提交
    bool isSubmitting() const { return submitting; }

private:
    brls::DetailCell* cellStart{};
    brls::DetailCell* cellEnd{};
    brls::DetailCell* cellCategory{};
    brls::Button* btnSubmit{};
    brls::Label* labelNow{};
    brls::Label* labelHint{};

    /// 订阅播放进度事件，实时刷新「当前进度」这一行
    MPVEvent::Subscription eventSubscribeID;

    std::string bvid;
    int64_t cid;
    double videoDuration;

    double start = 0;
    double end   = 0;
    std::string category;

    bool submitting = false;

    /// 刷新三行 cell 的右侧文案与提交按钮状态
    void refresh();

    /// 打开分类下拉框
    void chooseCategory();

    /// 提交，成功后关闭对话框
    void doSubmit();

    /// 把「当前播放进度」写入起点或终点
    void markStart();
    void markEnd();

    /// 当前播放进度（秒），取不到时返回 0
    static double currentProgress();
};

}  // namespace wiliwili
