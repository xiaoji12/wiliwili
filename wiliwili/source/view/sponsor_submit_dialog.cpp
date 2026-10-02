//
// 提交空降助手片段对话框
//

#include "view/sponsor_submit_dialog.hpp"

#include <borealis.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/button.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/dropdown.hpp>
#include <borealis/views/label.hpp>

#include "api/sponsor_block.hpp"
#include "utils/number_helper.hpp"
#include "view/mpv_core.hpp"

using namespace brls::literals;

namespace wiliwili {

/// 分类的显示名走 i18n（键名后缀与 SponsorBlock 的分类 ID 一致）
static std::string categoryLabelOf(const std::string& category) {
    return brls::getStr("wiliwili/player/sponsor_submit/cat_" + category);
}

SponsorSubmitDialog::SponsorSubmitDialog(std::string bvid_, int64_t cid_, double videoDuration_)
    : bvid(std::move(bvid_)), cid(cid_), videoDuration(videoDuration_) {
    this->setAxis(brls::Axis::COLUMN);
    this->setWidth(560);
    this->setHeight(brls::View::AUTO);
    this->setPadding(24, 32, 20, 32);

    auto theme = brls::Application::getTheme();

    /// 标题
    auto* title = new brls::Label();
    title->setText("wiliwili/player/sponsor_submit/title"_i18n);
    title->setFontSize(24);
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    title->setMargins(0, 0, 16, 0);
    this->addView(title);

    /// 起点 / 终点 / 分类，三行样式与设置页一致
    auto addCell = [this](const std::string& text, const std::string& id) {
        auto* cell = new brls::DetailCell();
        cell->setText(text);
        cell->setWidth(brls::View::AUTO);
        cell->registerClickAction([this, id](brls::View*) {
            if (id == "start") this->markStart();
            else if (id == "end") this->markEnd();
            else this->chooseCategory();
            return true;
        });
        cell->addGestureRecognizer(new brls::TapGestureRecognizer(cell));
        this->addView(cell);
        return cell;
    };

    this->cellStart    = addCell("wiliwili/player/sponsor_submit/start"_i18n, "start");
    this->cellEnd      = addCell("wiliwili/player/sponsor_submit/end"_i18n, "end");
    this->cellCategory = addCell("wiliwili/player/sponsor_submit/category"_i18n, "category");

    /// 实时进度提示
    this->labelNow = new brls::Label();
    this->labelNow->setFontSize(16);
    this->labelNow->setTextColor(theme.getColor("font/grey"));
    this->labelNow->setMargins(16, 0, 4, 0);
    this->addView(this->labelNow);

    /// 操作说明
    this->labelHint = new brls::Label();
    this->labelHint->setFontSize(14);
    this->labelHint->setTextColor(theme.getColor("font/grey"));
    this->labelHint->setSingleLine(false);
    this->labelHint->setText("wiliwili/player/sponsor_submit/hint"_i18n);
    this->labelHint->setMargins(0, 0, 16, 0);
    this->addView(this->labelHint);

    /// 提交按钮放在内容区，而不是 Dialog 的按钮上：
    /// Dialog 的按钮回调是在 dismiss 之后才执行的，那时本视图可能已经被销毁。
    this->btnSubmit = new brls::Button();
    this->btnSubmit->setStyle(&brls::BUTTONSTYLE_HIGHLIGHT);
    this->btnSubmit->setText("wiliwili/player/sponsor_submit/submit"_i18n);
    this->btnSubmit->setWidth(200);
    this->btnSubmit->setMarginTop(4);
    this->btnSubmit->registerClickAction([this](brls::View*) {
        this->doSubmit();
        return true;
    });
    this->addView(this->btnSubmit);

    /// 默认值：以当前播放进度预填起点与终点
    const double now = currentProgress();
    this->start      = now;
    this->end        = now;
    this->category   = SponsorBlock::submittableCategories().front();

    this->refresh();

    /// 视频在对话框后面继续播放，实时刷新进度提示
    this->eventSubscribeID = MPV_E->subscribe([this](MpvEventEnum event) {
        if (event != MpvEventEnum::UPDATE_PROGRESS) return;
        this->labelNow->setText("wiliwili/player/sponsor_submit/now"_i18n + " " + sec2Time(static_cast<size_t>(currentProgress())));
    });

    brls::Logger::debug("View SponsorSubmitDialog: create");
}

SponsorSubmitDialog::~SponsorSubmitDialog() {
    MPV_E->unsubscribe(this->eventSubscribeID);
    brls::Logger::debug("View SponsorSubmitDialog: delete");
}

double SponsorSubmitDialog::currentProgress() {
    const int64_t progress = MPVCore::instance().video_progress;
    return progress > 0 ? static_cast<double>(progress) : 0;
}

void SponsorSubmitDialog::refresh() {
    this->cellStart->setDetailText(sec2Time(static_cast<size_t>(this->start)));
    this->cellEnd->setDetailText(sec2Time(static_cast<size_t>(this->end)));
    this->cellCategory->setDetailText(categoryLabelOf(this->category));
    this->labelNow->setText("wiliwili/player/sponsor_submit/now"_i18n + " " + sec2Time(static_cast<size_t>(currentProgress())));
}

void SponsorSubmitDialog::markStart() {
    this->start = currentProgress();
    // 起点被推到终点之后时，把终点一起带过去，避免出现「终点早于起点」的死状态
    if (this->end < this->start) this->end = this->start;
    this->refresh();
}

void SponsorSubmitDialog::markEnd() {
    this->end = currentProgress();
    this->refresh();
}

void SponsorSubmitDialog::chooseCategory() {
    const auto& categories = SponsorBlock::submittableCategories();

    std::vector<std::string> labels;
    labels.reserve(categories.size());
    int selected = 0;
    for (size_t i = 0; i < categories.size(); ++i) {
        labels.emplace_back(categoryLabelOf(categories[i]));
        if (categories[i] == this->category) selected = static_cast<int>(i);
    }

    auto* dropdown = new brls::Dropdown(
        "wiliwili/player/sponsor_submit/category"_i18n, labels,
        [this, categories](int value) {
            if (value < 0 || value >= static_cast<int>(categories.size())) return;
            this->category = categories[value];
            this->refresh();
        },
        selected);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void SponsorSubmitDialog::doSubmit() {
    if (this->submitting) return;

    if (this->end <= this->start) {
        brls::Application::notify("wiliwili/player/sponsor_submit/invalid_range"_i18n);
        return;
    }

    this->submitting = true;

    // 先把所有参数拷出来，再关闭对话框：
    // 之后本视图会被销毁，异步回调里不能再访问 this。
    const std::string submitBvid    = this->bvid;
    const int64_t submitCid         = this->cid;
    const double submitStart        = this->start;
    const double submitEnd          = this->end;
    const double submitDuration     = this->videoDuration;
    const std::string submitCategory = this->category;

    this->dismiss();

    SponsorBlock::instance().submit(
        submitBvid, submitCid, submitStart, submitEnd, submitCategory, submitDuration,
        [](bool ok, SponsorSubmitError error) {
            // cpr 的回调在后台线程，切回主线程再弹提示
            brls::sync([ok, error]() {
                if (ok) {
                    brls::Application::notify("wiliwili/player/sponsor_submit/ok"_i18n);
                    return;
                }
                switch (error) {
                    case SponsorSubmitError::Invalid:
                        brls::Application::notify("wiliwili/player/sponsor_submit/invalid_range"_i18n);
                        break;
                    case SponsorSubmitError::Network:
                        brls::Application::notify("wiliwili/player/sponsor_submit/network_failed"_i18n);
                        break;
                    default:
                        brls::Application::notify("wiliwili/player/sponsor_submit/failed"_i18n);
                        break;
                }
            });
        });
}

}  // namespace wiliwili
