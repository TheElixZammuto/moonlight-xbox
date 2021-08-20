//
// MenuPage.xaml.cpp
// Implementation of the MenuPage class
//

#include "pch.h"
#include "MenuPage.xaml.h"
#include "StreamPage.xaml.h"
#include <ppltasks.h>

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

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

MenuPage::MenuPage()
{
	InitializeComponent();
}

void moonlight_xbox_dx::MenuPage::ConnectButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Platform::String ^ipAddress = this->ipAddressText->Text;
	this->progressRing->IsActive = true;
	MoonlightClient *client = MoonlightClient::GetInstance();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, ipAddress->Data(), 2047);
	int status = client->Connect(ipAddressStr);
	if (status != 0) {
		this->connectStatus->Text = L"Cannot Connect";
		return;
	}
	if (!client->IsPaired()) {
		char* pin = client->GeneratePIN();
		std::string s_str = std::string(pin);
		std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
		const wchar_t* w_char = wid_str.c_str();
		Platform::String^ p_string = ref new Platform::String(w_char);
		this->connectStatus->Text =String::Concat(L"Use PIN for pairing and then press Connect Again: ", p_string);
		ProgressRing ^ring = progressRing;
		TextBlock ^text = connectStatus;
		auto t = Concurrency::create_async([]()
		{
				MoonlightClient* client = MoonlightClient::GetInstance();
				int a = client->Pair();
		});
		this->progressRing->IsActive = false;
	}
	else {
		this->connectStatus->Text = L"Connected";
		this->progressRing->IsActive = false;
		UpdateApps();
	}
	//
}

void moonlight_xbox_dx::MenuPage::UpdateApps() {
	std::vector<MoonlightApplication> apps = MoonlightClient::GetInstance()->GetApplications();
	this->appsPanel->Visibility = Windows::UI::Xaml::Visibility::Visible;
	this->appsComboBox->Items->Clear();
	for (auto a : apps) {
		ComboBoxItem^ button = ref new ComboBoxItem();
		std::string s_str = std::string(a.name);
		std::wstring wid_str = std::wstring(s_str.begin(), s_str.end());
		const wchar_t* w_char = wid_str.c_str();
		Platform::String^ p_string = ref new Platform::String(w_char);
		button->Content = p_string;
		button->DataContext = a.id;
		this->appsComboBox->Items->Append(button);
	}
}

void moonlight_xbox_dx::MenuPage::OnAppClicked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
	/*ComboBoxItem^ item = (ComboBoxItem^) this->appsComboBox->SelectedItem;
	MoonlightClient::GetInstance()->SetAppID((int) item->DataContext);*/
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(StreamPage::typeid));
	if (result) {
		this->ConnectStatusText->Text = L"OK";
	}
	else {
		this->ConnectStatusText->Text = L"Unable to Navigate onto DirectX Page";
	}
}

