#include "pch.h"
#include "XamlHelpers.h"
#define MLOG_TAG_OVERRIDE "XamlHelpers"
#include "Utils.hpp"
#include <algorithm>
#include <cmath>
#include <memory>

using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

namespace moonlight_xbox_dx {

FrameworkElement^ FindChildByName(DependencyObject^ parent, Platform::String^ name) {
    if (parent == nullptr) return nullptr;
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(parent, i);
        auto fe = dynamic_cast<FrameworkElement^>(child);
        if (fe != nullptr && fe->Name == name) return fe;
        auto rec = FindChildByName(child, name);
        if (rec != nullptr) return rec;
    }
    return nullptr;
}

ScrollViewer^ FindScrollViewer(DependencyObject^ parent) {
    if (parent == nullptr) return nullptr;
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(parent, i);
        auto sv = dynamic_cast<ScrollViewer^>(child);
        if (sv != nullptr) return sv;
        auto rec = FindScrollViewer(child);
        if (rec != nullptr) return rec;
    }
    return nullptr;
}

static Windows::UI::Color s_appliedAccentColor = Windows::UI::Color{ 255, 0, 120, 215 };

Windows::UI::Color GetAppliedAccentColor() {
    return s_appliedAccentColor;
}

void ApplyAccentColor(Windows::UI::Color color) {
    s_appliedAccentColor = color;

    wchar_t buf[16];
    swprintf_s(buf, L"#%02X%02X%02X%02X", color.A, color.R, color.G, color.B);
    Windows::UI::Xaml::Interop::TypeName colorType;
    colorType.Name = "Windows.UI.Color";
    colorType.Kind = Windows::UI::Xaml::Interop::TypeKind::Metadata;
    auto boxed = Windows::UI::Xaml::Markup::XamlBindingHelper::ConvertValue(
        colorType, ref new Platform::String(buf));

    static const wchar_t* themeKeys[] = { L"Default", L"Dark", L"Light" };
    auto appThemes = Windows::UI::Xaml::Application::Current->Resources->ThemeDictionaries;
    for (auto themeKey : themeKeys) {
        auto key = ref new Platform::String(themeKey);
        Windows::UI::Xaml::ResourceDictionary^ dict;
        if (appThemes->HasKey(key)) {
            dict = dynamic_cast<Windows::UI::Xaml::ResourceDictionary^>(appThemes->Lookup(key));
        } else {
            dict = ref new Windows::UI::Xaml::ResourceDictionary();
            appThemes->Insert(key, dict);
        }
        if (dict != nullptr) {
            dict->Insert("SystemAccentColor", boxed);

            auto brush = ref new Windows::UI::Xaml::Media::SolidColorBrush(color);
            dict->Insert("SystemControlHighlightAccentBrush", brush);
            dict->Insert("SystemControlBackgroundAccentBrush", brush);
        }
    }
}

SelectedContainerStatus ResolveSelectedContainer(
    ListViewBase^ grid, ScrollViewer^& scrollViewer, ListViewItem^& container)
{
    container = nullptr;
    if (grid == nullptr || grid->SelectedIndex < 0 || grid->SelectedItem == nullptr)
        return SelectedContainerStatus::NoSelection;

    if (scrollViewer == nullptr) scrollViewer = FindScrollViewer(grid);
    if (scrollViewer == nullptr) return SelectedContainerStatus::ScrollViewerMissing;

    auto item = grid->SelectedItem;
    try {
        container = dynamic_cast<ListViewItem^>(grid->ContainerFromItem(item));
    } catch (...) {}
    if (container == nullptr) {
        try {
            grid->ScrollIntoView(item);
            grid->UpdateLayout();
            container = dynamic_cast<ListViewItem^>(grid->ContainerFromItem(item));
        } catch (...) {}
    }
    if (container == nullptr) {
        return SelectedContainerStatus::ContainerMissing;
    }

    return SelectedContainerStatus::Ready;
}

CenterContainerOutcome CenterContainerInScrollViewer(
    ScrollViewer^ scrollViewer, UIElement^ container, bool vertical, bool immediate)
{
    if (scrollViewer == nullptr || container == nullptr) return CenterContainerOutcome::NotReady;

    auto fe = dynamic_cast<FrameworkElement^>(container);
    double containerWidth  = fe != nullptr ? fe->ActualWidth  : 0.0;
    double containerHeight = fe != nullptr ? fe->ActualHeight : 0.0;
    if (containerWidth <= 0.0 || containerHeight <= 0.0) return CenterContainerOutcome::NotReady;

    double viewport = vertical ? scrollViewer->ViewportHeight : scrollViewer->ViewportWidth;
    if (!std::isfinite(viewport) || viewport <= 0.0) return CenterContainerOutcome::NotReady;

    auto contentElement = dynamic_cast<UIElement^>(scrollViewer->Content);
    if (contentElement == nullptr) return CenterContainerOutcome::NotReady;

    Point origin; origin.X = 0.0f; origin.Y = 0.0f;
    Point inContent;
    try {
        inContent = container->TransformToVisual(contentElement)->TransformPoint(origin);
    } catch (...) {
        return CenterContainerOutcome::NotReady;
    }

    double itemCenter = vertical
        ? (inContent.Y + (containerHeight * 0.5))
        : (inContent.X + (containerWidth  * 0.5));

    double scrollable = vertical ? scrollViewer->ScrollableHeight : scrollViewer->ScrollableWidth;
    double current     = vertical ? scrollViewer->VerticalOffset  : scrollViewer->HorizontalOffset;
    if (!std::isfinite(scrollable) || !std::isfinite(current)) return CenterContainerOutcome::NotReady;

    double target = itemCenter - (viewport * 0.5);
    target = std::max(0.0, std::min(target, std::max(0.0, scrollable)));

    if (std::fabs(target - current) < 0.5) {
        return CenterContainerOutcome::Applied;
    }

    try {
        if (vertical) scrollViewer->ChangeView(nullptr, target, nullptr, immediate);
        else          scrollViewer->ChangeView(target, nullptr, nullptr, immediate);
    } catch (...) {}

    return CenterContainerOutcome::Applied;
}

EdgeCenteringPaddingResult ApplyEdgeCenteringPadding(ListViewBase^ grid, double desiredPadding)
{
    if (grid == nullptr || !std::isfinite(desiredPadding)) return EdgeCenteringPaddingResult::Unchanged;

    auto padding = grid->Padding;
    if (std::fabs(padding.Left - desiredPadding) < 0.5 && std::fabs(padding.Right - desiredPadding) < 0.5)
        return EdgeCenteringPaddingResult::Unchanged;

    padding.Left = desiredPadding;
    padding.Right = desiredPadding;
    grid->Padding = padding;
    return EdgeCenteringPaddingResult::Applied;
}

Windows::Foundation::EventRegistrationToken ArmContainerRealizedWait(ListViewBase^ grid, std::function<void()> onFire)
{
    Windows::Foundation::EventRegistrationToken empty{ 0 };
    if (grid == nullptr) return empty;
    auto selectedItem = grid->SelectedItem;
    if (selectedItem == nullptr) return empty;

    auto tokenHolder = std::make_shared<Windows::Foundation::EventRegistrationToken>();
    tokenHolder->Value = 0;
    *tokenHolder = grid->ContainerContentChanging +=
        ref new TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>(
            [grid, selectedItem, tokenHolder, onFire](ListViewBase^, ContainerContentChangingEventArgs^ args) {
                if (args->Item != selectedItem) return;
                try { grid->ContainerContentChanging -= *tokenHolder; }
                catch (...) { MLOG(Utils::LogLevel::Error, "ArmContainerRealizedWait failed to unsubscribe"); }
                tokenHolder->Value = 0;
                onFire();
            });
    return *tokenHolder;
}

void CancelContainerRealizedWait(ListViewBase^ grid, Windows::Foundation::EventRegistrationToken& token)
{
    if (grid == nullptr || token.Value == 0) return;
    try { grid->ContainerContentChanging -= token; }
    catch (...) { MLOG(Utils::LogLevel::Error, "CancelContainerRealizedWait failed to unsubscribe"); }
    token.Value = 0;
}

Windows::Foundation::EventRegistrationToken ArmLayoutUpdatedWait(ListViewBase^ grid, std::function<void()> onFire)
{
    Windows::Foundation::EventRegistrationToken empty{ 0 };
    if (grid == nullptr) return empty;

    auto tokenHolder = std::make_shared<Windows::Foundation::EventRegistrationToken>();
    tokenHolder->Value = 0;
    *tokenHolder = grid->LayoutUpdated +=
        ref new EventHandler<Platform::Object^>(
            [grid, tokenHolder, onFire](Platform::Object^, Platform::Object^) {
                try { grid->LayoutUpdated -= *tokenHolder; }
                catch (...) { MLOG(Utils::LogLevel::Error, "ArmLayoutUpdatedWait failed to unsubscribe"); }
                tokenHolder->Value = 0;
                onFire();
            });
    return *tokenHolder;
}

void CancelLayoutUpdatedWait(ListViewBase^ grid, Windows::Foundation::EventRegistrationToken& token)
{
    if (grid == nullptr || token.Value == 0) return;
    try { grid->LayoutUpdated -= token; }
    catch (...) { MLOG(Utils::LogLevel::Error, "CancelLayoutUpdatedWait failed to unsubscribe"); }
    token.Value = 0;
}

void AnimateCrossfadeText(
    FrameworkElement^ visibilityAnchor,
    Windows::UI::Xaml::Media::Animation::Storyboard^ showSb,
    Windows::UI::Xaml::Media::Animation::Storyboard^ hideSb,
    bool wantAnimate,
    std::function<void()> applyNewText,
    std::function<bool()> isStillCurrent)
{
    bool alreadyVisible = visibilityAnchor != nullptr && visibilityAnchor->Opacity > 0.01;
    if (wantAnimate && alreadyVisible && hideSb != nullptr && showSb != nullptr) {
        auto token = std::make_shared<Windows::Foundation::EventRegistrationToken>();
        *token = hideSb->Completed += ref new EventHandler<Platform::Object^>(
            [hideSb, showSb, token, applyNewText, isStillCurrent](Platform::Object^, Platform::Object^) mutable {
                try { hideSb->Completed -= *token; }
                catch (...) { MLOG(Utils::LogLevel::Error, "AnimateCrossfadeText failed to unsubscribe hideSb.Completed"); }
                if (!isStillCurrent()) return;
                try {
                    applyNewText();
                    showSb->Begin();
                } catch (...) { MLOG(Utils::LogLevel::Error, "AnimateCrossfadeText post-hide apply failed"); }
            });
        hideSb->Begin();
    } else {
        try { applyNewText(); }
        catch (...) { MLOG(Utils::LogLevel::Error, "AnimateCrossfadeText direct apply failed"); }
        if (showSb != nullptr) showSb->Begin();
    }
}

}
