//
// Created by fang on 2022/8/15.
//

// register this view in main.cpp
//#include "view/video_progress_slider.hpp"
//    brls::Application::registerXMLView("VideoProgressSlider", VideoProgressSlider::create);

#pragma once

#include <vector>
#include <borealis/core/box.hpp>

#include "api/sponsor_block.hpp"

namespace brls {
class Rectangle;
}
class SVGImage;

class VideoProgressSlider : public brls::Box {
public:
    VideoProgressSlider();

    ~VideoProgressSlider() override;

    static brls::View* create();

    void onLayout() override;

    brls::View* getDefaultFocus() override;

    void onChildFocusLost(brls::View* directChild, brls::View* focusedView) override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void setProgress(float progress);

    [[nodiscard]] float getProgress() const { return progress; }

    // Progress is manually dragged
    brls::Event<float>* getProgressEvent() { return &progressEvent; }

    // Manual dragging is over
    brls::Event<float>* getProgressSetEvent() { return &progressSetEvent; }

    // Manual dragging is canceled
    brls::Event<>* getProgressCancelEvent() { return &progressCancelEvent; }

    // Add a chapter point
    void addClipPoint(float point);

    // Clear all the points
    void clearClipPoint();

    void setClipPoint(const std::vector<float>& data);

    const std::vector<float>& getClipPoint();

    void setProgressUpdater(const std::function<float(float)>& updater) { progressUpdater = updater; }

    void setManuallyMode();

    /**
     * 设置空降助手分段，用于在进度条轨道上绘制彩色区间标记。
     * @param data     分段列表（起止时间为秒）
     * @param duration 视频总时长（秒），用于把时间换算成 0~1 的比例
     */
    void setSponsorSegments(const std::vector<wiliwili::SponsorSegment>& data, double duration);

    /// 清空分段颜色标记
    void clearSponsorSegments();

private:
    brls::InputManager* input;
    brls::Rectangle* line;
    brls::Rectangle* lineEmpty;
    SVGImage* pointerIcon;
    brls::Box* pointer;

    brls::Event<float> progressEvent;
    brls::Event<float> progressSetEvent;
    brls::Event<> progressCancelEvent;

    std::vector<float> clipPointList;

    /// 空降助手分段（用于在轨道上绘制颜色标记）
    std::vector<wiliwili::SponsorSegment> sponsorSegments;

    /// 视频总时长（秒）。0 表示未知，此时不绘制分段标记
    double sponsorDuration = 0;

    float progress             = 1;
    bool pointerSelected       = false;
    // while pointer is selected ignore progress setting
    bool ignoreProgressSetting = false;
    // while pointer is selected, the last progress value set by setProgress()
    // is stored here to be restored when canceling the selection
    float lastProgress         = 1;

    // pointer 被选中时, 上一次按钮按下时的进度值
    float lastStartProgress  = 0;
    std::function <float(float)> progressUpdater{};

    void buttonsProcessing();
    void updateUI();
    bool cancelPointerChange();
};
