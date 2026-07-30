#include "pch.h"
#include "SlidingMenu.xaml.h"
#include "Utils.hpp"

using namespace moonlight_xbox_dx::Controls;
using namespace Windows::UI::Xaml::Media::Animation;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Core;
using namespace concurrency;

SlidingMenu::SlidingMenu()
{
    InitializeComponent();
    auto app = dynamic_cast<App^>(Application::Current);

    if (app != nullptr && app->GlobalMenuItems != nullptr) {
        GlobalItems = app->GlobalMenuItems;
    }
    else {
        GlobalItems = ref new Platform::Collections::Vector<moonlight_xbox_dx::MenuItem^>();
    }
    PageItems = ref new Platform::Collections::Vector<moonlight_xbox_dx::MenuItem^>();

    try { GlobalItemsList->ItemsSource = GlobalItems; } catch(...) {}
    try { PageItemsList->ItemsSource = PageItems; } catch(...) {}
}

void SlidingMenu::OnApplyTemplate()
{
    __super::OnApplyTemplate();
}

bool SlidingMenu::IsOpen::get() {
    return m_isOpen;
}

void SlidingMenu::AddPageItem(moonlight_xbox_dx::MenuItem^ item) {
    if (item == nullptr) return;
    PageItems->Append(item);
}

void SlidingMenu::ClearPageItems() {
    PageItems->Clear();
}

void SlidingMenu::Open()
{
    if (m_isOpen) return;
    m_isOpen = true;
    try {

        if (this->Dispatcher != nullptr && this->Dispatcher->HasThreadAccess) {
            auto asyncOp = this->ShowAsync();

            try { VisualStateManager::GoToState(this, "Opened", true); } catch(...) {}
            concurrency::create_task(asyncOp).then([this](Windows::UI::Xaml::Controls::ContentDialogResult result) {
                m_isOpen = false;
                try { VisualStateManager::GoToState(this, "Closed", false); } catch(...) {}
            });
        } else {

            Platform::WeakReference weakThis(this);
            auto disp = this->Dispatcher;
            if (disp == nullptr) {
                try { auto coreView = Windows::ApplicationModel::Core::CoreApplication::MainView; if (coreView != nullptr && coreView->CoreWindow != nullptr) disp = coreView->CoreWindow->Dispatcher; } catch(...) { disp = nullptr; }
            }
            if (disp != nullptr) {
                disp->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([weakThis]() {
                    auto that = weakThis.Resolve<SlidingMenu>();
                    if (that == nullptr) return;
                    try {
                        auto asyncOp = that->ShowAsync();
                        try { VisualStateManager::GoToState(that, "Opened", true); } catch(...) {}
                        concurrency::create_task(asyncOp).then([that](Windows::UI::Xaml::Controls::ContentDialogResult result) {
                            that->m_isOpen = false;
                            try { VisualStateManager::GoToState(that, "Closed", false); } catch(...) {}
                        });
                    } catch(...) {
                        that->m_isOpen = false;
                    }
                }));
            } else {

                try {
                    auto asyncOp = this->ShowAsync();
                    try { VisualStateManager::GoToState(this, "Opened", true); } catch(...) {}
                    concurrency::create_task(asyncOp).then([this](Windows::UI::Xaml::Controls::ContentDialogResult result) {
                        m_isOpen = false;
                        try { VisualStateManager::GoToState(this, "Closed", false); } catch(...) {}
                    });
                } catch(...) {
                    m_isOpen = false;
                }
            }
        }
    } catch(...) {
        m_isOpen = false;
    }
}

void SlidingMenu::Close()
{
	if (!m_isOpen) return;

    try {

        try { VisualStateManager::GoToState(this, "Closed", true); } catch(...) {}
        this->Hide();
    } catch(...) {}
    m_isOpen = false;
}

void SlidingMenu::OnMenuItemClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    auto btn = safe_cast<Windows::UI::Xaml::Controls::Button^>(sender);
    auto item = safe_cast<moonlight_xbox_dx::MenuItem^>(btn->DataContext);
    if (item != nullptr) {

        try {
            if (item->ClickAction != nullptr) {
                try { item->ClickAction(item, nullptr); } catch(...) {}
            }
        } catch(...) {}

        try { MenuItemInvoked(this, item); } catch(...) {}
    }

    Close();
}

void SlidingMenu::Overlay_Tapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e)
{
    Close();
}
