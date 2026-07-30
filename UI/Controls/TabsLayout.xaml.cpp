#include "pch.h"
#include "UI\Controls\TabsLayout.xaml.h"

using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Markup;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Core;
using namespace Windows::Foundation::Collections;
#include <collection.h>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;

TabsLayout::TabsLayout()
{
    InitializeComponent();
    this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &TabsLayout::OnLoaded);
    this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &TabsLayout::OnUnloaded);
    m_tabButtons = ref new Platform::Collections::Vector<Button^>();
    m_clickTokens = ref new Platform::Collections::Vector<long long>();
    m_focusTokens = ref new Platform::Collections::Vector<long long>();
    m_selectionIndicatorReady = false;
    m_slideStoryboard = nullptr;
    m_indicatorY = 0.0;
    try {
        auto uiSettings = ref new Windows::UI::ViewManagement::UISettings();
        m_accentColor = uiSettings->GetColorValue(Windows::UI::ViewManagement::UIColorType::Accent);
    } catch (...) {
        m_accentColor = Windows::UI::ColorHelper::FromArgb(0xFF, 0x00, 0x78, 0xD7);
    }
    SelectionIndicator->Background = ref new SolidColorBrush(m_accentColor);
}

void TabsLayout::AccentColor::set(Windows::UI::Color value)
{
    m_accentColor = value;
    if (SelectionIndicator != nullptr) {
        SelectionIndicator->Background = ref new SolidColorBrush(value);
    }
}

UIElement^ TabsLayout::LeftContent::get()
{
    return safe_cast<UIElement^>(ContentPresenter->Content);
}

void TabsLayout::LeftContent::set(UIElement^ value)
{
    ContentPresenter->Content = value;
}

UIElement^ TabsLayout::TabsContent::get()
{
    return safe_cast<UIElement^>(TabsPresenter->Content);
}

void TabsLayout::TabsContent::set(UIElement^ value)
{
    TabsPresenter->Content = value;
}

Windows::UI::Xaml::DependencyProperty^ TabsLayout::TargetPanelNameProperty = nullptr;

void TabsLayout::SetTargetPanelName(UIElement^ element, Platform::String^ value)
{
    if (TargetPanelNameProperty == nullptr) {
        TargetPanelNameProperty = DependencyProperty::RegisterAttached(
            "TargetPanelName",
            Platform::String::typeid,
            TabsLayout::typeid,
            nullptr);
    }
    element->SetValue(TargetPanelNameProperty, value);
}

Platform::String^ TabsLayout::GetTargetPanelName(UIElement^ element)
{
    if (TargetPanelNameProperty == nullptr) return nullptr;
    auto val = element->GetValue(TargetPanelNameProperty);
    return safe_cast<Platform::String^>(val);
}

Windows::UI::Xaml::DependencyProperty^ TabsLayout::TargetPanelProperty = nullptr;

Windows::UI::Xaml::DependencyProperty^ TabsLayout::IsSelectedProperty = nullptr;

void TabsLayout::SetTargetPanel(UIElement^ element, UIElement^ value)
{
    if (TargetPanelProperty == nullptr) {
        TargetPanelProperty = DependencyProperty::RegisterAttached(
            "TargetPanel",
            Windows::UI::Xaml::UIElement::typeid,
            TabsLayout::typeid,
            nullptr);
    }
    element->SetValue(TargetPanelProperty, value);
}

UIElement^ TabsLayout::GetTargetPanel(UIElement^ element)
{
    if (TargetPanelProperty == nullptr) return nullptr;
    auto val = element->GetValue(TargetPanelProperty);
    return safe_cast<UIElement^>(val);
}

void TabsLayout::SetIsSelected(UIElement^ element, bool value)
{
    if (IsSelectedProperty == nullptr) {
        IsSelectedProperty = DependencyProperty::RegisterAttached(
            "IsSelected",
            bool::typeid,
            TabsLayout::typeid,
            nullptr);
    }
    element->SetValue(IsSelectedProperty, value);

    auto btn = dynamic_cast<Button^>(element);
    if (btn != nullptr) {
        AnimateForeground(btn, value);
    }
}

bool TabsLayout::GetIsSelected(UIElement^ element)
{
    if (IsSelectedProperty == nullptr) return false;
    auto val = element->GetValue(IsSelectedProperty);
    return safe_cast<bool>(val);
}

void TabsLayout::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    HookUpTabs();
}

void TabsLayout::FocusFirstTab()
{
    if (m_tabButtons != nullptr && m_tabButtons->Size > 0) {
        auto first = m_tabButtons->GetAt(0);
        if (first != nullptr) {
            first->Focus(Windows::UI::Xaml::FocusState::Programmatic);
        }
    }
}

void TabsLayout::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    UnhookTabs();
}

static void WalkVisualChildren(UIElement^ root, IVector<UIElement^>^ out)
{
    auto fe = dynamic_cast<FrameworkElement^>(root);
    if (fe == nullptr) return;
    int count = VisualTreeHelper::GetChildrenCount(fe);
    for (int i = 0; i < count; i++) {
        auto child = safe_cast<UIElement^>(VisualTreeHelper::GetChild(fe, i));
        if (child == nullptr) continue;
        out->Append(child);
        WalkVisualChildren(child, out);
    }
}

void TabsLayout::HookUpTabs()
{
    UnhookTabs();
    if (TabsPresenter == nullptr) return;
    auto content = TabsPresenter->Content;
    if (content == nullptr) return;

    auto found = ref new Platform::Collections::Vector<UIElement^>();
    WalkVisualChildren(safe_cast<UIElement^>(content), found);
    for (unsigned int i = 0; i < found->Size; i++) {
        auto btn = dynamic_cast<Button^>(found->GetAt(i));
        if (btn != nullptr) {

            btn->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Stretch;

            btn->HorizontalContentAlignment = Windows::UI::Xaml::HorizontalAlignment::Left;

            btn->Margin = Windows::UI::Xaml::Thickness(0);

            if (m_tabButtons == nullptr) m_tabButtons = ref new Platform::Collections::Vector<Button^>();
            if (m_clickTokens == nullptr) m_clickTokens = ref new Platform::Collections::Vector<long long>();
            if (m_focusTokens == nullptr) m_focusTokens = ref new Platform::Collections::Vector<long long>();

            auto clickToken = btn->Click += ref new RoutedEventHandler(this, &TabsLayout::OnTabButtonClick);
            auto focusToken = btn->GotFocus += ref new RoutedEventHandler(this, &TabsLayout::OnTabButtonGotFocus);
            m_clickTokens->Append(clickToken.Value);
            m_focusTokens->Append(focusToken.Value);
            m_tabButtons->Append(btn);
        }
    }

    if (m_tabButtons != nullptr && m_tabButtons->Size > 0) {
        auto first = m_tabButtons->GetAt(0);
        if (first != nullptr) {

            auto direct = GetTargetPanel(first);
            if (direct != nullptr) {
                auto fe = dynamic_cast<FrameworkElement^>(direct);
                if (fe != nullptr && fe->Name != nullptr && fe->Name->Length() > 0) {
                    SelectTabByName(fe->Name);
                }
                else {

                    direct->Visibility = Windows::UI::Xaml::Visibility::Visible;
                    SetIsSelected(first, true);
                    m_selectedButton = first;
                }
            }
            else {

                auto name = GetTargetPanelName(first);
                if (name != nullptr) {
                    SelectTabByName(name);
                }
                else {

                    SetIsSelected(first, true);
                    m_selectedButton = first;
                }
            }
        }
    }
}

void TabsLayout::UnhookTabs()
{

    if (m_tabButtons != nullptr && m_clickTokens != nullptr && m_focusTokens != nullptr) {
        unsigned int count = m_tabButtons->Size;
        if (m_clickTokens->Size < count) count = m_clickTokens->Size;
        if (m_focusTokens->Size < count) count = m_focusTokens->Size;
        for (unsigned int i = 0; i < count; i++) {
            auto btn = m_tabButtons->GetAt(i);
            auto clickTokenVal = m_clickTokens->GetAt(i);
            auto focusTokenVal = m_focusTokens->GetAt(i);
            if (btn != nullptr) {
                Windows::Foundation::EventRegistrationToken clickToken{ clickTokenVal };
                Windows::Foundation::EventRegistrationToken focusToken{ focusTokenVal };
                btn->Click -= clickToken;
                btn->GotFocus -= focusToken;
            }
        }
    }
    if (m_tabButtons != nullptr) m_tabButtons->Clear();
    if (m_clickTokens != nullptr) m_clickTokens->Clear();
    if (m_focusTokens != nullptr) m_focusTokens->Clear();
    m_selectedButton = nullptr;
    m_selectionIndicatorReady = false;
    m_indicatorY = 0.0;
    if (m_slideStoryboard != nullptr) {
        m_slideStoryboard->Stop();
        m_slideStoryboard = nullptr;
    }
    if (m_fadeStoryboard != nullptr) {
        m_fadeStoryboard->Stop();
        m_fadeStoryboard = nullptr;
    }
    if (ContentPresenter != nullptr) ContentPresenter->Opacity = 1.0;
}

void TabsLayout::AnimateForeground(Button^ btn, bool selected)
{
    if (btn == nullptr) return;

    Windows::UI::Color targetColor = selected
        ? Windows::UI::ColorHelper::FromArgb(0xFF, 0xFF, 0xFF, 0xFF)
        : Windows::UI::ColorHelper::FromArgb(0xAA, 0xFF, 0xFF, 0xFF);

    Windows::UI::Color currentColor;
    auto existingBrush = dynamic_cast<SolidColorBrush^>(btn->Foreground);
    if (existingBrush != nullptr) {
        currentColor = existingBrush->Color;
    } else {
        currentColor = selected
            ? Windows::UI::ColorHelper::FromArgb(0xAA, 0xFF, 0xFF, 0xFF)
            : Windows::UI::ColorHelper::FromArgb(0xFF, 0xFF, 0xFF, 0xFF);
    }

    auto brush = ref new SolidColorBrush(currentColor);
    btn->Foreground = brush;

    auto storyboard = ref new Storyboard();
    auto colorAnim = ref new ColorAnimation();
    colorAnim->To = targetColor;

    colorAnim->Duration = Windows::UI::Xaml::Duration(
        Windows::Foundation::TimeSpan{ selected ? 3000000LL : 1500000LL });
    colorAnim->EnableDependentAnimation = true;

    if (selected) {
        auto ease = ref new CubicEase();
        ease->EasingMode = EasingMode::EaseOut;
        colorAnim->EasingFunction = ease;
    }

    Storyboard::SetTarget(colorAnim, brush);
    Storyboard::SetTargetProperty(colorAnim, "Color");
    storyboard->Children->Append(colorAnim);
    storyboard->Begin();
}

void TabsLayout::AnimateSelectionToButton(Button^ btn, bool animate)
{
    if (btn == nullptr || SelectionIndicator == nullptr || TabsGrid == nullptr) return;

    double btnHeight = btn->ActualHeight;
    if (btnHeight <= 0) return;

    auto transform = btn->TransformToVisual(TabsGrid);
    auto point = transform->TransformPoint(Windows::Foundation::Point(0, 0));
    double targetY = point.Y;

    SelectionIndicator->Height = btnHeight;

    if (!animate) {
        if (m_slideStoryboard != nullptr) {
            m_slideStoryboard->Stop();
            m_slideStoryboard = nullptr;
        }
        SelectionTransform->Y = targetY;
        m_indicatorY = targetY;
        SelectionIndicator->Opacity = 1.0;
        return;
    }

    if (m_slideStoryboard != nullptr) {
        m_slideStoryboard->Stop();
        m_slideStoryboard = nullptr;
    }

    SelectionTransform->Y = m_indicatorY;
    m_indicatorY = targetY;

    auto storyboard = ref new Storyboard();
    auto animation = ref new DoubleAnimation();
    animation->To = targetY;
    animation->Duration = Windows::UI::Xaml::Duration(Windows::Foundation::TimeSpan{ 3200000LL });

    auto ease = ref new BackEase();
    ease->Amplitude = 0.1;
    ease->EasingMode = EasingMode::EaseInOut;
    animation->EasingFunction = ease;

    Storyboard::SetTarget(animation, SelectionTransform);
    Storyboard::SetTargetProperty(animation, "Y");
    storyboard->Children->Append(animation);

    m_slideStoryboard = storyboard;
    storyboard->Begin();
}

static UIElement^ FindElementByNameRecursive(UIElement^ root, Platform::String^ name)
{
    auto fe = dynamic_cast<FrameworkElement^>(root);
    if (fe == nullptr) return nullptr;
    if (fe->Name == name) return fe;
    int count = VisualTreeHelper::GetChildrenCount(fe);
    for (int i = 0; i < count; i++) {
        auto child = safe_cast<UIElement^>(VisualTreeHelper::GetChild(fe, i));
        auto res = FindElementByNameRecursive(child, name);
        if (res != nullptr) return res;
    }
    return nullptr;
}

void TabsLayout::OnTabButtonClick(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto btn = dynamic_cast<Button^>(safe_cast<UIElement^>(sender));
    if (btn == nullptr) return;
    auto direct = GetTargetPanel(btn);
    if (direct != nullptr) {

        auto fe = dynamic_cast<FrameworkElement^>(direct);
        if (fe != nullptr && fe->Name != nullptr && fe->Name->Length() > 0) {
            SelectTabByName(fe->Name);
            return;
        }

        direct->Visibility = Windows::UI::Xaml::Visibility::Visible;
        return;
    }
    auto name = GetTargetPanelName(btn);
    if (name != nullptr) {
        SelectTabByName(name);
    }
}

void TabsLayout::OnTabButtonGotFocus(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

    OnTabButtonClick(sender, e);
}

void TabsLayout::OnContentScrollViewerKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
    if (e->Key == Windows::System::VirtualKey::Left && m_selectedButton != nullptr) {
        m_selectedButton->Focus(Windows::UI::Xaml::FocusState::Keyboard);
        e->Handled = true;
    }
}

void TabsLayout::SelectTabByName(Platform::String^ panelName)
{
    if (panelName == nullptr) return;

    auto left = this->LeftContent;
    if (left == nullptr) return;

    auto target = FindElementByNameRecursive(left, panelName);
    if (target == nullptr) return;

    auto parent = dynamic_cast<Panel^>(safe_cast<FrameworkElement^>(left));

    Button^ matchedButton = nullptr;
    for (unsigned int b = 0; b < m_tabButtons->Size; b++) {
        auto btn = m_tabButtons->GetAt(b);
        auto direct = GetTargetPanel(btn);
        auto name = GetTargetPanelName(btn);
        bool match = false;
        if (direct != nullptr) match = (direct == target);
        else if (name != nullptr) match = (name == panelName);
        if (match) { matchedButton = btn; break; }
    }

    if (matchedButton != nullptr && matchedButton == m_selectedButton && m_selectionIndicatorReady) return;

    if (matchedButton != nullptr) {
        if (m_selectedButton != nullptr) SetIsSelected(m_selectedButton, false);
        SetIsSelected(matchedButton, true);
        m_selectedButton = matchedButton;

        if (m_selectionIndicatorReady) {
            AnimateSelectionToButton(matchedButton, true);
        } else {
            auto self = this;
            auto btn_ref = matchedButton;
            this->Dispatcher->RunAsync(
                CoreDispatcherPriority::Normal,
                ref new DispatchedHandler([self, btn_ref]() {
                    self->AnimateSelectionToButton(btn_ref, false);
                    self->m_selectionIndicatorReady = true;
                }));
        }
    }

    if (parent == nullptr) {
        auto fe = dynamic_cast<FrameworkElement^>(target);
        if (fe != nullptr) fe->Visibility = Windows::UI::Xaml::Visibility::Visible;
        return;
    }

    if (!m_selectionIndicatorReady) {
        for (unsigned int i = 0; i < parent->Children->Size; i++) {
            auto child = parent->Children->GetAt(i);
            child->Visibility = (child == target)
                ? Windows::UI::Xaml::Visibility::Visible
                : Windows::UI::Xaml::Visibility::Collapsed;
        }
        return;
    }

    if (m_fadeStoryboard != nullptr) {
        m_fadeStoryboard->Stop();
        m_fadeStoryboard = nullptr;
        ContentPresenter->Opacity = 1.0;
    }

    auto fadeOut = ref new DoubleAnimation();
    fadeOut->To = 0.0;
    fadeOut->Duration = Windows::UI::Xaml::Duration(Windows::Foundation::TimeSpan{ 1200000LL });
    auto easeOut = ref new CubicEase();
    easeOut->EasingMode = EasingMode::EaseIn;
    fadeOut->EasingFunction = easeOut;

    auto sb = ref new Storyboard();
    Storyboard::SetTarget(fadeOut, ContentPresenter);
    Storyboard::SetTargetProperty(fadeOut, "Opacity");
    sb->Children->Append(fadeOut);
    m_fadeStoryboard = sb;

    auto self = this;
    auto target_ref = target;
    auto parent_ref = parent;

    sb->Completed += ref new Windows::Foundation::EventHandler<Platform::Object^>(
        [self, target_ref, parent_ref](Platform::Object^, Platform::Object^) {
            for (unsigned int i = 0; i < parent_ref->Children->Size; i++) {
                auto child = parent_ref->Children->GetAt(i);
                child->Visibility = (child == target_ref)
                    ? Windows::UI::Xaml::Visibility::Visible
                    : Windows::UI::Xaml::Visibility::Collapsed;
            }

            auto fadeIn = ref new DoubleAnimation();
            fadeIn->From = 0.0;
            fadeIn->To = 1.0;
            fadeIn->Duration = Windows::UI::Xaml::Duration(Windows::Foundation::TimeSpan{ 2000000LL });
            auto easeIn = ref new CubicEase();
            easeIn->EasingMode = EasingMode::EaseOut;
            fadeIn->EasingFunction = easeIn;

            auto sb2 = ref new Storyboard();
            Storyboard::SetTarget(fadeIn, self->ContentPresenter);
            Storyboard::SetTargetProperty(fadeIn, "Opacity");
            sb2->Children->Append(fadeIn);
            self->m_fadeStoryboard = sb2;
            sb2->Begin();
        });

    sb->Begin();
}
