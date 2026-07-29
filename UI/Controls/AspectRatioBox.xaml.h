#pragma once

#include "UI\Controls\AspectRatioBox.g.h"

namespace moonlight_xbox_dx {

    [Windows::UI::Xaml::Data::Bindable]
    public ref class AspectRatioBox sealed
    {
    public:
        AspectRatioBox();

        static property Windows::UI::Xaml::DependencyProperty^ RatioProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_ratioProperty; }
        }

        property double Ratio {
            double get();
            void set(double v);
        }

    protected:
        virtual Windows::Foundation::Size MeasureOverride(Windows::Foundation::Size availableSize) override;
        virtual Windows::Foundation::Size ArrangeOverride(Windows::Foundation::Size finalSize) override;

    private:
        static Windows::UI::Xaml::DependencyProperty^ m_ratioProperty;
    };
}
