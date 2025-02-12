//
// AddHost.xaml.cpp
// Implementation of the AddHost class
//

#include "pch.h"
#include "AddHost.xaml.h"
#include <State\MoonlightClient.h>
#include "Utils.hpp"

using namespace moonlight_xbox_dx;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::ViewManagement::Core;

// The Content Dialog item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

AddHost::AddHost()
{
	state = GetApplicationState();
	InitializeComponent();
}

void moonlight_xbox_dx::AddHost::OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args)
{
	sender->IsPrimaryButtonEnabled = false;

	Platform::String^ ipAddress = dialogIPAddressTextBox->Text;
	Platform::String^ port = dialogPortTextBox->Text;

	auto def = args->GetDeferral();
	Concurrency::create_task([def, ipAddress, port, this, args, sender]() {
		bool status = state->AddHost(ipAddress, port);
		if (!status) {
			Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([sender, this, ipAddress, def, args]() {
				args->Cancel = true;
				sender->Content = L"Failed to Connect to " + ipAddress;
				def->Complete();
				}));
			return;
		}
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([def]() {
			def->Complete();
			}));
		});
}

void moonlight_xbox_dx::AddHost::OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}