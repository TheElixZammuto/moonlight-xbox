#pragma once
#include "pch.h"
#include <functional>

namespace moonlight_xbox_dx {

Windows::UI::Xaml::Controls::ScrollViewer^
    FindScrollViewer(Windows::UI::Xaml::DependencyObject^ parent);

Windows::UI::Xaml::FrameworkElement^
    FindChildByName(Windows::UI::Xaml::DependencyObject^ parent, Platform::String^ name);

void ApplyAccentColor(Windows::UI::Color color);
Windows::UI::Color GetAppliedAccentColor();

enum class SelectedContainerStatus {
    NoSelection,
    ScrollViewerMissing,
    ContainerMissing,
    Ready,
};

SelectedContainerStatus ResolveSelectedContainer(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    Windows::UI::Xaml::Controls::ScrollViewer^& scrollViewer,
    Windows::UI::Xaml::Controls::ListViewItem^& container);

enum class CenterContainerOutcome {
    NotReady,
    Applied,
};

CenterContainerOutcome CenterContainerInScrollViewer(
    Windows::UI::Xaml::Controls::ScrollViewer^ scrollViewer,
    Windows::UI::Xaml::UIElement^ container,
    bool vertical,
    bool immediate);

enum class EdgeCenteringPaddingResult {
    Unchanged,
    Applied,
};

EdgeCenteringPaddingResult ApplyEdgeCenteringPadding(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    double desiredPadding);

Windows::Foundation::EventRegistrationToken ArmContainerRealizedWait(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    std::function<void()> onFire);

void CancelContainerRealizedWait(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    Windows::Foundation::EventRegistrationToken& token);

Windows::Foundation::EventRegistrationToken ArmLayoutUpdatedWait(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    std::function<void()> onFire);

void CancelLayoutUpdatedWait(
    Windows::UI::Xaml::Controls::ListViewBase^ grid,
    Windows::Foundation::EventRegistrationToken& token);

void AnimateCrossfadeText(
    Windows::UI::Xaml::FrameworkElement^ visibilityAnchor,
    Windows::UI::Xaml::Media::Animation::Storyboard^ showSb,
    Windows::UI::Xaml::Media::Animation::Storyboard^ hideSb,
    bool wantAnimate,
    std::function<void()> applyNewText,
    std::function<bool()> isStillCurrent);

}
