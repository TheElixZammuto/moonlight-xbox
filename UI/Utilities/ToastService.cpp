#include "pch.h"
#include "ToastService.h"
#include "XamlHelpers.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Animation;

static Border^ s_toastBorder = nullptr;
static TextBlock^ s_toastText = nullptr;
static Storyboard^ s_toastStoryboard = nullptr;
static AcrylicBrush^ s_toastBrush = nullptr;

namespace moonlight_xbox_dx {

void InitializeToastService(Grid^ rootGrid) {
    s_toastText = ref new TextBlock();
    s_toastText->Foreground = ref new SolidColorBrush(Windows::UI::Colors::White);
    s_toastText->FontSize = 12;

    s_toastBorder = ref new Border();
    s_toastBorder->HorizontalAlignment = HorizontalAlignment::Right;
    s_toastBorder->VerticalAlignment = VerticalAlignment::Bottom;
    s_toastBorder->Margin = Thickness{ 0, 0, 0, 48 };
    s_toastBorder->Padding = Thickness{ 16, 10, 48, 10 };
    s_toastBorder->CornerRadius = CornerRadius{ 8, 0, 0, 8 };
    s_toastBorder->Visibility = Visibility::Collapsed;
    s_toastBorder->IsHitTestVisible = false;
    s_toastBorder->Child = s_toastText;

    s_toastBrush = ref new AcrylicBrush();
    s_toastBrush->TintOpacity = 0.75;
    s_toastBrush->TintLuminosityOpacity = 0.5;
    s_toastBorder->Background = s_toastBrush;

    auto translate = ref new TranslateTransform();
    s_toastBorder->RenderTransform = translate;

    s_toastStoryboard = ref new Storyboard();

    auto xAnim = ref new DoubleAnimationUsingKeyFrames();
    Storyboard::SetTarget(xAnim, translate);
    Storyboard::SetTargetProperty(xAnim, "X");

    auto kfX0 = ref new DiscreteDoubleKeyFrame();
    kfX0->KeyTime = KeyTime{ TimeSpan{ 0LL } };
    kfX0->Value = 400.0;

    auto kfX1 = ref new EasingDoubleKeyFrame();
    kfX1->KeyTime = KeyTime{ TimeSpan{ 3000000LL } };
    kfX1->Value = 0.0;
    auto easeIn = ref new CubicEase();
    easeIn->EasingMode = EasingMode::EaseOut;
    kfX1->EasingFunction = easeIn;

    auto kfX2 = ref new DiscreteDoubleKeyFrame();
    kfX2->KeyTime = KeyTime{ TimeSpan{ 23000000LL } };
    kfX2->Value = 0.0;

    auto kfX3 = ref new EasingDoubleKeyFrame();
    kfX3->KeyTime = KeyTime{ TimeSpan{ 28000000LL } };
    kfX3->Value = 400.0;
    auto easeOut = ref new CubicEase();
    easeOut->EasingMode = EasingMode::EaseIn;
    kfX3->EasingFunction = easeOut;

    xAnim->KeyFrames->Append(kfX0);
    xAnim->KeyFrames->Append(kfX1);
    xAnim->KeyFrames->Append(kfX2);
    xAnim->KeyFrames->Append(kfX3);

    auto opAnim = ref new DoubleAnimationUsingKeyFrames();
    Storyboard::SetTarget(opAnim, s_toastBorder);
    Storyboard::SetTargetProperty(opAnim, "Opacity");

    auto kfOp0 = ref new DiscreteDoubleKeyFrame();
    kfOp0->KeyTime = KeyTime{ TimeSpan{ 0LL } };
    kfOp0->Value = 0.0;

    auto kfOp1 = ref new EasingDoubleKeyFrame();
    kfOp1->KeyTime = KeyTime{ TimeSpan{ 2500000LL } };
    kfOp1->Value = 1.0;
    auto opEaseIn = ref new CubicEase();
    opEaseIn->EasingMode = EasingMode::EaseOut;
    kfOp1->EasingFunction = opEaseIn;

    auto kfOp2 = ref new DiscreteDoubleKeyFrame();
    kfOp2->KeyTime = KeyTime{ TimeSpan{ 23000000LL } };
    kfOp2->Value = 1.0;

    auto kfOp3 = ref new EasingDoubleKeyFrame();
    kfOp3->KeyTime = KeyTime{ TimeSpan{ 28000000LL } };
    kfOp3->Value = 0.0;
    auto opEaseOut = ref new CubicEase();
    opEaseOut->EasingMode = EasingMode::EaseIn;
    kfOp3->EasingFunction = opEaseOut;

    opAnim->KeyFrames->Append(kfOp0);
    opAnim->KeyFrames->Append(kfOp1);
    opAnim->KeyFrames->Append(kfOp2);
    opAnim->KeyFrames->Append(kfOp3);

    s_toastStoryboard->Children->Append(xAnim);
    s_toastStoryboard->Children->Append(opAnim);

    s_toastStoryboard->Completed += ref new EventHandler<Object^>([](Object^, Object^) {
        s_toastBorder->Visibility = Visibility::Collapsed;
    });

    rootGrid->Children->Append(s_toastBorder);
}

void ShowToast(Platform::String^ message) {
    if (s_toastBorder == nullptr || s_toastText == nullptr || s_toastStoryboard == nullptr)
        return;
    if (s_toastBrush != nullptr)
        s_toastBrush->TintColor = GetAppliedAccentColor();
    s_toastStoryboard->Stop();
    s_toastText->Text = message;
    s_toastBorder->Visibility = Visibility::Visible;
    s_toastStoryboard->Begin();
}

}
