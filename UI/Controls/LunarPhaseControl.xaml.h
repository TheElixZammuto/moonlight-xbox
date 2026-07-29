#pragma once

#include "UI\Controls\LunarPhaseControl.g.h"

namespace moonlight_xbox_dx {

    [Windows::Foundation::Metadata::WebHostHidden]
    public ref class LunarPhaseControl sealed
    {
    public:
        LunarPhaseControl();

        static property Windows::UI::Xaml::DependencyProperty^ ShowOrbitProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_showOrbitProperty; }
        }
        property bool ShowOrbit {
            bool get();
            void set(bool v);
        }

        static property Windows::UI::Xaml::DependencyProperty^ IsDashedProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_isDashedProperty; }
        }
        property bool IsDashed {
            bool get();
            void set(bool v);
        }

        static property Windows::UI::Xaml::DependencyProperty^ ShowLockProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_showLockProperty; }
        }
        property Windows::UI::Xaml::Visibility ShowLock {
            Windows::UI::Xaml::Visibility get();
            void set(Windows::UI::Xaml::Visibility v);
        }

        static property Windows::UI::Xaml::DependencyProperty^ ShowDisconnectedProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_showDisconnectedProperty; }
        }
        property Windows::UI::Xaml::Visibility ShowDisconnected {
            Windows::UI::Xaml::Visibility get();
            void set(Windows::UI::Xaml::Visibility v);
        }

        static property Windows::UI::Xaml::DependencyProperty^ ShadowCenterXProperty {
            Windows::UI::Xaml::DependencyProperty^ get() { return m_shadowCenterXProperty; }
        }
        property double ShadowCenterX {
            double get();
            void set(double v);
        }

        void UpdatePhase(double fillAmount, int side, bool animated);
        void SetSelected(bool selected, bool animated);

    private:
        static Windows::UI::Xaml::DependencyProperty^ m_showOrbitProperty;
        static Windows::UI::Xaml::DependencyProperty^ m_showLockProperty;
        static Windows::UI::Xaml::DependencyProperty^ m_showDisconnectedProperty;
        static Windows::UI::Xaml::DependencyProperty^ m_shadowCenterXProperty;
        static Windows::UI::Xaml::DependencyProperty^ m_isDashedProperty;

        static void OnShowOrbitChanged(
            Windows::UI::Xaml::DependencyObject^ d,
            Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
        static void OnShowLockChanged(
            Windows::UI::Xaml::DependencyObject^ d,
            Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
        static void OnShowDisconnectedChanged(
            Windows::UI::Xaml::DependencyObject^ d,
            Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
        static void OnShadowCenterXChanged(
            Windows::UI::Xaml::DependencyObject^ d,
            Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);
        static void OnIsDashedChanged(
            Windows::UI::Xaml::DependencyObject^ d,
            Windows::UI::Xaml::DependencyPropertyChangedEventArgs^ e);

        void SetCrescentPath(double shadowCenterX);

        Windows::UI::Xaml::Media::PathFigure^ m_crescentFigure;
        Windows::UI::Xaml::Media::ArcSegment^ m_outerArc;
        Windows::UI::Xaml::Media::ArcSegment^ m_innerArc;
        Windows::UI::Xaml::Media::PathFigure^ m_dashedCrescentFigure;
        Windows::UI::Xaml::Media::ArcSegment^ m_dashedOuterArc;
        Windows::UI::Xaml::Media::ArcSegment^ m_dashedInnerArc;
        Windows::UI::Xaml::Media::Animation::Storyboard^ m_phaseStoryboard;
        Windows::UI::Xaml::Media::Animation::Storyboard^ m_selectionStoryboard;
        Windows::UI::Xaml::DispatcherTimer^ m_orbitHideTimer;
        long long m_orbitShownTick;
    };
}
