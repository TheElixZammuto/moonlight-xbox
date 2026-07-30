#include "pch.h"
#include "UI\Controls\AspectRatioBox.xaml.h"
#include "Utils.hpp"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;

Windows::UI::Xaml::DependencyProperty^ AspectRatioBox::m_ratioProperty =
    Windows::UI::Xaml::DependencyProperty::Register(
        "Ratio",
        double::typeid,
        AspectRatioBox::typeid,
        ref new Windows::UI::Xaml::PropertyMetadata(1.0)
    );

AspectRatioBox::AspectRatioBox() {
    InitializeComponent();
}

double AspectRatioBox::Ratio::get() {
    return (double)this->GetValue(AspectRatioBox::m_ratioProperty);
}

void AspectRatioBox::Ratio::set(double v) {
    this->SetValue(AspectRatioBox::m_ratioProperty, v);
}

Windows::Foundation::Size AspectRatioBox::MeasureOverride(Windows::Foundation::Size availableSize) {

    double ratio = this->Ratio;
	double width = availableSize.Width;
	double height = availableSize.Height;

    if (isnan(width) || isnan(height) || (width == std::numeric_limits<double>::infinity() && height == std::numeric_limits<double>::infinity())) {
        return Windows::Foundation::Size(100 * ratio, 100);
    }

    if (!(height == std::numeric_limits<double>::infinity())) {
        width = height * ratio;
    } else if (!(width == std::numeric_limits<double>::infinity())) {

        height = width / ratio;
    }

    auto content = this->Content;
    if (content != nullptr) {
        auto fe = dynamic_cast<UIElement^>(content);
        if (fe != nullptr) {
            fe->Measure(Size(width, height));
        }
    }

    return Size(width, height);
}

Windows::Foundation::Size AspectRatioBox::ArrangeOverride(Windows::Foundation::Size finalSize) {

    double ratio = this->Ratio;

    double targetWidth = finalSize.Width;
    double targetHeight = finalSize.Height;

    double idealWidth = targetHeight * ratio;
    double idealHeight = targetHeight;
    if (idealWidth > targetWidth) {
        idealWidth = targetWidth;
        idealHeight = targetWidth / ratio;
    }

    double offsetX = (targetWidth - idealWidth) / 2.0;
    double offsetY = (targetHeight - idealHeight) / 2.0;

    auto content = this->Content;
    if (content != nullptr) {
        auto fe = dynamic_cast<UIElement^>(content);
        if (fe != nullptr) {
            fe->Arrange(Rect(offsetX, offsetY, idealWidth, idealHeight));
        }
    }

    return finalSize;
}
