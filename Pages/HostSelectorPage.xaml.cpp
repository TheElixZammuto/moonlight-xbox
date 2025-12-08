//
// HostSelectorPage.xaml.cpp
// Implementation of the HostSelectorPage class
//

#include "pch.h"
#include "HostSelectorPage.xaml.h"
#include "AppPage.xaml.h"
#include <State\MoonlightClient.h>
#include "HostSettingsPage.xaml.h"
#include "Utils.hpp"
#include "MoonlightSettings.xaml.h"
#include "State\MDNSHandler.h"
#include "MoonlightWelcome.xaml.h"
#include "Common\ModalDialog.xaml.h"
#include <string>
#include <cstdlib>
#include <winsock2.h>
#include <Ws2tcpip.h>

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

HostSelectorPage::HostSelectorPage()
{
	state = GetApplicationState();
	InitializeComponent();
}

void HostSelectorPage::NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	dialogHostnameTextBox = ref new TextBox();
	dialogHostnameTextBox->AcceptsReturn = false;
	dialogHostnameTextBox->KeyDown += ref new Windows::UI::Xaml::Input::KeyEventHandler(this, &HostSelectorPage::OnKeyDown);
	ContentDialog^ dialog = ref new ContentDialog();
	dialog->Content = dialogHostnameTextBox;
	dialog->Title = L"Add new Host";
	dialog->IsSecondaryButtonEnabled = true;
	dialog->PrimaryButtonText = "Ok";
	dialog->SecondaryButtonText = "Cancel";
	concurrency::create_task(dialog->ShowAsync());
	dialog->PrimaryButtonClick += ref new Windows::Foundation::TypedEventHandler<Windows::UI::Xaml::Controls::ContentDialog^, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^>(this, &HostSelectorPage::OnNewHostDialogPrimaryClick);
}

void HostSelectorPage::OnNewHostDialogPrimaryClick(Windows::UI::Xaml::Controls::ContentDialog^ sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs^ args)
{
	sender->IsPrimaryButtonEnabled = false;
	Platform::String^ hostname = dialogHostnameTextBox->Text;
	auto def = args->GetDeferral();
	Concurrency::create_task([def, hostname, this, args, sender]() {
		bool status = state->AddHost(hostname);
		if (!status) {
			Platform::WeakReference weakThis(this);
			concurrency::create_task(Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([sender, weakThis, hostname, def, args]() {
				args->Cancel = true;
				sender->Content = L"Failed to Connect to " + hostname;
				def->Complete();
				}))) .then([](concurrency::task<void> t) {
				try {
					t.get();
				}
				catch (const std::exception &e) {
					Utils::Logf("HostSelectorPage NewHost create_task exception: %s", e.what());
				}
				catch (...) {
					Utils::Log("HostSelectorPage NewHost create_task unknown exception");
				}
				});
			return;
		}
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([def]() {
			def->Complete();
			}));
		});
}

void HostSelectorPage::GridView_ItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e)
{
	MoonlightHost^ host = (MoonlightHost^)e->ClickedItem;

	if (host->Connected && host->Paired) {
		this->Connect(host);
		return;
	}

	if (host->Connected && !host->Paired) {
		this->StartPairing(host);
		return;
	}

	FrameworkElement^ anchor = nullptr;
	if (e && e->OriginalSource) {
		anchor = dynamic_cast<FrameworkElement^>(e->OriginalSource);
	}

	if (!anchor) {
		auto gv = dynamic_cast<GridView^>(sender);
		if (gv) {
			auto container = dynamic_cast<GridViewItem^>(gv->ContainerFromItem(host));
			if (container) anchor = container;
		}
	}

	this->ShowHostActions(anchor, host);
}

void HostSelectorPage::ShowHostActions(Windows::UI::Xaml::FrameworkElement^ anchor, MoonlightHost^ host)
{
    currentHost = host;

    if (currentHost == nullptr) {
        return;
    }

    if (currentHost->Connected || currentHost->WolPolling) {
        this->wakeHostButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        this->testConnectionButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
    }
    else {
        this->wakeHostButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
        this->testConnectionButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    }

    if (anchor) {
        this->ActionsFlyout->ShowAt(anchor);
    }
    else {
        this->ActionsFlyout->ShowAt(this->HostsGrid);
    }
}

void HostSelectorPage::StartPairing(MoonlightHost^ host) {
	MoonlightClient* client = new MoonlightClient();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, host->LastHostname->Data(), 2047);
	int status = client->Connect(ipAddressStr);
	if (status != 0)return;
	char* pin = client->GeneratePIN();
	ContentDialog^ dialog = ref new ContentDialog();
	wchar_t msg[4096];
	swprintf(msg, 4096, L"We need to pair the host before continuing. Type %S on your host to continue", pin);
	dialog->Content = ref new Platform::String(msg);
	dialog->PrimaryButtonText = "Ok";
	concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog));
	Concurrency::create_task([dialog, host, client, pin]() {
			int a = client->Pair();
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([a, dialog, host]()
			{
				if (a == 0) {
						::moonlight_xbox_dx::ModalDialog::HideDialog(dialog);
				}
				else {
					//dialog->Content = Utils::StringFromStdString(std::string(gs_error));
				}
					host->UpdateHostInfo(true);
			}
			));
		}) .then([](concurrency::task<void> t) {
			try {
				t.get();
			}
			catch (const std::exception &e) {
				Utils::Logf("HostSelectorPage StartPairing task exception: %s", e.what());
			}
			catch (...) {
				Utils::Log("HostSelectorPage StartPairing task unknown exception");
			}
		});
}

void HostSelectorPage::removeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	State->RemoveHost(currentHost);
}

void HostSelectorPage::HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e)
{
	//Thanks to https://stackoverflow.com/questions/62878368/uwp-gridview-listview-get-the-righttapped-item-supporting-both-mouse-and-xbox-co
	FrameworkElement^ senderElement = (FrameworkElement^)e->OriginalSource;

	// Determine the host from the tapped element
	if (senderElement->GetType()->FullName->Equals(GridViewItem::typeid->FullName)) {
		auto gi = (GridViewItem^)senderElement;
		currentHost = (MoonlightHost^)(gi->Content);
	}
	else {
		currentHost = (MoonlightHost^)(senderElement->DataContext);
	}

	// Delegate to consolidated helper (use senderElement as anchor)
	this->ShowHostActions(senderElement, currentHost);
}

void HostSelectorPage::hostSettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), currentHost);
}

void HostSelectorPage::SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid));
}

void HostSelectorPage::OnStateLoaded() {
	if (GetApplicationState()->FirstTime) {
		this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightWelcome::typeid));
		return;
	}

	Concurrency::create_task([this]() {
		for (auto a : GetApplicationState()->SavedHosts) {
			a->UpdateHostInfo(false);
		}
	}).then([this]() {
		if (GetApplicationState()->autostartInstance.size() > 0) {
			auto pii = Utils::StringFromStdString(GetApplicationState()->autostartInstance);
			for (unsigned int i = 0; i < GetApplicationState()->SavedHosts->Size; i++) {
				auto host = GetApplicationState()->SavedHosts->GetAt(i);
				if (host->InstanceId->Equals(pii)) {
					auto that = this;
					Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
						Windows::UI::Core::CoreDispatcherPriority::High,
						ref new Windows::UI::Core::DispatchedHandler([that, host]() {
							that->Connect(host);
						})
					);
					break;
				}
			}
		}
	}).then([this](concurrency::task<void> t) {
		try {
			t.get();
		}
		catch (const std::exception &e) {
			Utils::Logf("HostSelectorPage OnStateLoaded task exception: %s", e.what());
		}
		catch (...) {
			Utils::Log("HostSelectorPage OnStateLoaded task unknown exception");
		}
	});
}

void HostSelectorPage::Connect(MoonlightHost^ host) {
	if (!host->Connected)return;
	if (!host->Paired) {
		StartPairing(host);
		return;
	}
	state->shouldAutoConnect = true;
	continueFetch.store(false);
		bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(AppPage::typeid), host);
}

void HostSelectorPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseVisible);
	continueFetch.store(true);
	Concurrency::create_task([this] {
		while (continueFetch.load()) {
			query_mdns();
			for (auto a : GetApplicationState()->SavedHosts) {
				a->UpdateHostInfo(true);
			}
			Sleep(5000);
		}
	}) .then([](concurrency::task<void> t) {
		try {
			t.get();
		}
		catch (const std::exception &e) {
			Utils::Logf("HostSelectorPage mdns loop task exception: %s", e.what());
		}
		catch (...) {
			Utils::Log("HostSelectorPage mdns loop task unknown exception");
		}
	});
}

void HostSelectorPage::OnKeyDown(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e)
{
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}

void moonlight_xbox_dx::HostSelectorPage::wakeHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (currentHost == nullptr) {
		return;
	}

	try {
		bool success = State->WakeHost(currentHost);
		if (success) {
			ContentDialog^ confirm = ref new ContentDialog();
			confirm->Title = "Wake Host";
			confirm->Content = "Wake-on-LAN packet sent successfully to " + currentHost->ComputerName;
			confirm->PrimaryButtonText = "OK";
			concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(confirm));
		}
		else {
			ContentDialog^ fail = ref new ContentDialog();
			fail->Title = "Wake Host Failed";
			fail->Content = "Failed to send Wake-on-LAN packet.\n\nPlease check if Wake-on-LAN is enabled on the host.";
			fail->PrimaryButtonText = "OK";
			concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(fail));
		}

		// After sending WoL successfully, aggressively poll this host for a short period
		// to detect when it comes online (hosts can take time to boot/respond).
		// Poll every 1s for up to 60s, stop early if Connected becomes true.
		if (success) {
			auto host = currentHost;
			host->WolPolling = true;
			concurrency::create_task(concurrency::create_async([host]() {
				int consecutiveSuccess = 0;
				for (int i = 0; i < 60; ++i) {
					try {
						host->UpdateHostInfo(false);
						if (host->Connected) {
							consecutiveSuccess++;
							if (consecutiveSuccess >= 3) {
								host->WolPolling = false;
								break;
							}
						} else {
							consecutiveSuccess = 0;
						}
					} catch (...) {
						consecutiveSuccess = 0;
					}
					Sleep(1000);
				}
			})).then([host](concurrency::task<void> t) {
				try { t.get(); } catch (...) { }
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([host]() {
					host->WolPolling = false;
				}));
			});
		}
	}
	catch (std::exception ex) {
		ContentDialog^ dialog = ref new ContentDialog();
		dialog->Title = "Wake Host Error";
		dialog->Content = "An error occurred while trying to wake the host:\n\n" + Utils::StringFromChars((char*)ex.what());
	dialog->PrimaryButtonText = "OK";
	concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog));
	}
}

void HostSelectorPage::hostDetailsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
    if (currentHost == nullptr) return;

    auto panel = ref new Windows::UI::Xaml::Controls::StackPanel();
    panel->Spacing = 8;

    auto addLine = [panel](Platform::String^ label, Platform::String^ value) {
        auto val = ref new Windows::UI::Xaml::Controls::TextBlock();
		val->Text = label + " " + value;
        panel->Children->Append(val);
    };

    addLine(Utils::StringFromStdString("Hostname:"), currentHost->LastHostname == nullptr ? Utils::StringFromStdString("(null)") : currentHost->LastHostname);
    addLine(Utils::StringFromStdString("Instance ID:"), currentHost->InstanceId == nullptr ? Utils::StringFromStdString("(null)") : currentHost->InstanceId);
    addLine(Utils::StringFromStdString("Computer Name:"), currentHost->ComputerName == nullptr ? Utils::StringFromStdString("(null)") : currentHost->ComputerName);
    addLine(Utils::StringFromStdString("Server Address:"), currentHost->ServerAddress == nullptr ? Utils::StringFromStdString("(null)") : currentHost->ServerAddress);
    addLine(Utils::StringFromStdString("MAC Address:"), currentHost->MacAddress == nullptr ? Utils::StringFromStdString("(null)") : currentHost->MacAddress);

    auto scroll = ref new Windows::UI::Xaml::Controls::ScrollViewer();
    scroll->Content = panel;
    scroll->VerticalScrollBarVisibility = Windows::UI::Xaml::Controls::ScrollBarVisibility::Auto;
    scroll->MaxHeight = 400;

    auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
    dialog->Title = Utils::StringFromStdString("Host Details");
    dialog->Content = scroll;
    dialog->PrimaryButtonText = Utils::StringFromStdString("OK");

    try {
        dialog->XamlRoot = this->XamlRoot;
    } catch(...) {}

    concurrency::create_task(dialog->ShowAsync());
}

void HostSelectorPage::testConnectionButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e) {
    if (currentHost == nullptr) return;

	std::string hostname = Utils::PlatformStringToStdString(currentHost->LastHostname);
	std::string hostOnly;
	int port = 0;
	try {
		auto split = GetApplicationState()->Split_IP_Address(hostname, ':');
		hostOnly = split.first;
		port = split.second;
	}
	catch (...) {
		// If parsing fails for any reason, fall back to original behaviour
		auto pos = hostname.find(':');
		hostOnly = (pos == std::string::npos) ? hostname : hostname.substr(0, pos);
		port = 0;
	}

	concurrency::create_task([hostOnly, port]() {
        WSADATA wsaData;
        std::string resultMsg = "Unknown";
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            resultMsg = "WSAStartup failed";
        } else {
            struct addrinfo hints;
            struct addrinfo *res = nullptr;
            ZeroMemory(&hints, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;

			// If port is known (non-zero) use it, otherwise default to 47989
			std::string portStorage = (port > 0) ? std::to_string(port) : std::string("47989");
			int gai = getaddrinfo(hostOnly.c_str(), portStorage.c_str(), &hints, &res);
            if (gai != 0) {
                resultMsg = std::string("DNS lookup failed: ") + std::to_string(gai);
            } else {
                bool ok = false;
                double bestRttMs = -1.0;
                for (struct addrinfo *p = res; p != nullptr; p = p->ai_next) {
                    SOCKET s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
                    if (s == INVALID_SOCKET) continue;
                    u_long mode = 1;
                    ioctlsocket(s, FIONBIO, &mode);

                    using namespace std::chrono;
                    auto start = high_resolution_clock::now();
                    int rc = connect(s, p->ai_addr, (int)p->ai_addrlen);
                    if (rc == 0) {
                        auto end = high_resolution_clock::now();
                        double ms = duration_cast<microseconds>(end - start).count() / 1000.0;
                        if (bestRttMs < 0 || ms < bestRttMs) bestRttMs = ms;
                        ok = true;
                        closesocket(s);
                        break;
                    }
                    fd_set writeSet;
                    FD_ZERO(&writeSet);
                    FD_SET(s, &writeSet);
                    timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
                    int sel = select(0, NULL, &writeSet, NULL, &tv);
                    if (sel > 0 && FD_ISSET(s, &writeSet)) {
                        auto end = high_resolution_clock::now();
                        double ms = duration_cast<microseconds>(end - start).count() / 1000.0;
                        if (bestRttMs < 0 || ms < bestRttMs) bestRttMs = ms;
                        ok = true;
                        closesocket(s);
                        break;
                    }
                    closesocket(s);
                }
                freeaddrinfo(res);
                if (ok) {
                    if (bestRttMs >= 0) {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "Connection OK (RTT: %.1f ms)", bestRttMs);
                        resultMsg = buf;
                    } else {
                        resultMsg = "Connection OK";
                    }
                } else {
                    resultMsg = "Connection failed";
                }
            }
            WSACleanup();
        }

        Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([resultMsg, hostOnly]() {
            auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
            dialog->Title = Utils::StringFromStdString("Test Connection");
            dialog->Content = Utils::StringFromStdString(hostOnly + ": " + resultMsg);
            dialog->PrimaryButtonText = Utils::StringFromStdString("OK");
            concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog));
        }));
    });
}
