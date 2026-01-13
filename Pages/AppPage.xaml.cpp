#include "pch.h"
#include "AppPage.Xaml.h"
#include "Common\ModalDialog.xaml.h"
#include "HostSettingsPage.xaml.h"
#include "HostSelectorPage.xaml.h"
#include "State\MoonlightClient.h"
#include "StreamPage.xaml.h"
#include "Utils.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Composition;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=234238

AppPage::AppPage()
{
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseVisible);
	
	this->Loaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &AppPage::OnLoaded);
	this->Unloaded += ref new Windows::UI::Xaml::RoutedEventHandler(this, &AppPage::OnUnloaded);
}

void AppPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {

	MoonlightHost^ mhost = dynamic_cast<MoonlightHost^>(e->Parameter);

	if (mhost == nullptr) {
		try {
			Utils::Log("[AppPage] OnNavigatedTo: mhost is null - attempting redirect to HostSelectorPage\n");
			if (this->Frame != nullptr) {
				bool navRes = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid));
				if (!navRes) {
					Utils::Log("[AppPage] OnNavigatedTo: Navigate returned false - scheduling deferred navigation\n");
					Platform::WeakReference weakThis(this);
					this->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([weakThis]() {
						try {
							auto that = weakThis.Resolve<AppPage>();
							if (that != nullptr && that->Frame != nullptr) {
								bool r = that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid));
								Utils::Logf("[AppPage] Deferred Navigate returned %d\n", r);
							} else {
								Utils::Log("[AppPage] Deferred Navigate: Page or Frame no longer available\n");
							}
						} catch (const std::exception &ex) {
							Utils::Logf("[AppPage] Deferred navigation threw an exception. Exception: %s\n", ex.what());
						} catch (...) {
							Utils::Log("[AppPage] Deferred navigation threw an exception. Unknown Exception.\n");
						}
					}));
				}
			} else {
				Utils::Log("[AppPage] OnNavigatedTo: this->Frame is null\n");
			}
		} catch (const std::exception &ex) {
			Utils::Logf("[AppPage] OnNavigatedTo: Failed to navigate to HostSelectorPage. Exception: %s\n", ex.what());
		} catch (...) {
			Utils::Log("[AppPage] OnNavigatedTo: Failed to navigate to HostSelectorPage. Unknown Exception.\n");
		}
		return;
	}

	host = mhost;
	host->UpdateHostInfo(true);
	host->UpdateApps();

	// Start background polling for app running state and connectivity
	continueAppFetch.store(true);
	wasConnected.store(host->Connected);
	auto that = this;
	create_task([that]() {
		while (that->continueAppFetch.load()) {
			try {
				if (that->host != nullptr) {
					that->host->UpdateAppRunningStates();
					bool connected = false;
					try { connected = (that->host->Connect() == 0); } catch (...) { connected = false; }
					if (that->wasConnected.load() && !connected) {
						that->wasConnected.store(false);
						Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that]() {
							try {
								auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
								dialog->Title = Utils::StringFromStdString("Disconnected");
								dialog->Content = Utils::StringFromStdString("Connection to host was lost.");
								dialog->PrimaryButtonText = Utils::StringFromStdString("OK");
								concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog)).then([that](Windows::UI::Xaml::Controls::ContentDialogResult result) {
									that->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that]() {
										try {
											that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid));
										} catch (...) {}
									}));
								});
							} catch (...) {}
						}));
					}
					else if (!that->wasConnected.load() && connected) {
						that->wasConnected.store(true);
					}
				}
			} catch (...) {}
			Sleep(3000);
		}
	});

	if (host->AutostartID >= 0 && GetApplicationState()->shouldAutoConnect) {
		GetApplicationState()->shouldAutoConnect = false;
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this]() {
				this->Connect(host->AutostartID);
			}));
	}
	GetApplicationState()->shouldAutoConnect = false;
}

void AppPage::AppsGrid_ItemClick(Platform::Object ^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs ^ e) {
	MoonlightApp ^ app = (MoonlightApp ^) e->ClickedItem;
	this->currentApp = app;

	if (this->host != nullptr) {
		for (unsigned int i = 0; i < this->host->Apps->Size; ++i) {
			auto candidate = this->host->Apps->GetAt(i);
			if (candidate != nullptr && candidate->CurrentlyRunning && candidate->Id != app->Id) {
				this->closeAndStartButton_Click(nullptr, nullptr);
				return;
			}
		}
	}

	this->Connect(app->Id);
}

void AppPage::Connect(int appId) {
	StreamConfiguration ^ config = ref new StreamConfiguration();
	config->hostname = host->LastHostname;
	config->appID = appId;
	config->width = host->Resolution->Width;
	config->height = host->Resolution->Height;
	config->bitrate = host->Bitrate;
	config->FPS = host->FPS;
	config->audioConfig = host->AudioConfig;
	config->videoCodec = host->VideoCodec;
	config->playAudioOnPC = host->PlayAudioOnPC;
	config->enableHDR = host->EnableHDR;
	config->enableSOPS = host->EnableSOPS;
	config->framePacing = host->FramePacing;
	config->enableStats = host->EnableStats;
	config->enableGraphs = host->EnableGraphs;
	if (config->enableHDR) {
		host->VideoCodec = "HEVC (H.265)";
	}
 bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(StreamPage::typeid), config);
	if (!result) {
		printf("C");
	}
}

void AppPage::AppsGrid_RightTapped(Platform::Object ^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs ^ e) {
    Utils::Log("AppPage::AppsGrid_RightTapped invoked\n");
    FrameworkElement ^ senderElement = (FrameworkElement ^) e->OriginalSource;
    FrameworkElement ^ anchor = senderElement;

	if (senderElement != nullptr && senderElement->GetType()->FullName->Equals(GridViewItem::typeid->FullName)) {
		auto gi = (GridViewItem ^) senderElement;
		currentApp = (MoonlightApp ^)(gi->Content);
		anchor = gi;
    } else {
        if (senderElement != nullptr) currentApp = (MoonlightApp ^)(senderElement->DataContext);

        if (currentApp == nullptr && this->HostsGrid != nullptr && this->HostsGrid->SelectedIndex >= 0) {
            currentApp = (MoonlightApp ^) this->HostsGrid->SelectedItem;
			auto container = (GridViewItem ^) this->HostsGrid->ContainerFromIndex(this->HostsGrid->SelectedIndex);
            if (container != nullptr)
                anchor = container;
            else
                anchor = this->HostsGrid;
        }
    }

    bool anyRunning = false;
    MoonlightApp^ runningApp = nullptr;
    if (this->host != nullptr) {
        for (unsigned int i = 0; i < this->host->Apps->Size; ++i) {
            auto candidate = this->host->Apps->GetAt(i);
            if (candidate != nullptr && candidate->CurrentlyRunning) {
                anyRunning = true;
                runningApp = candidate;
                break;
            }
        }
    }

    this->resumeAppButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    this->closeAppButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    this->closeAndStartButton->Visibility = Windows::UI::Xaml::Visibility::Collapsed;

    if (!anyRunning) {
        this->resumeAppButton->Text = "Open App";
        this->resumeAppButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
    } else {
        if (currentApp != nullptr && currentApp->CurrentlyRunning) {
            this->resumeAppButton->Text = "Resume App";
            this->resumeAppButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
            this->closeAppButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
        } else {
            if (currentApp != nullptr) {
                this->closeAndStartButton->Visibility = Windows::UI::Xaml::Visibility::Visible;
            }
        }
    }

    if (anchor != nullptr) {
        this->ActionsFlyout->ShowAt(anchor);
    } else {
        this->ActionsFlyout->ShowAt(this->HostsGrid);
    }
}

void AppPage::resumeAppButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	this->Connect(this->currentApp->Id);
}

void AppPage::closeAndStartButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	if (this->currentApp == nullptr) {
		return;
	}

	if (sender != nullptr) {
		this->ExecuteCloseAndStart();
		return;
	}

	auto dialog = ref new Windows::UI::Xaml::Controls::ContentDialog();
	dialog->Title = Utils::StringFromStdString("Confirm");
	dialog->Content = Utils::StringFromStdString("Close currently running app and connect?");
	dialog->PrimaryButtonText = Utils::StringFromStdString("Yes");
	dialog->CloseButtonText = Utils::StringFromStdString("Cancel");

	Platform::WeakReference weakThis(this);

	concurrency::create_task(::moonlight_xbox_dx::ModalDialog::ShowOnceAsync(dialog)).then([weakThis](Windows::UI::Xaml::Controls::ContentDialogResult result) {
		try {
			if (result != Windows::UI::Xaml::Controls::ContentDialogResult::Primary) {
				return;
			}

			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr) return;
			that->ExecuteCloseAndStart();
		} catch (const std::exception &e) {
			Utils::Logf("closeAndStartButton_Click dialog task exception: %s\n", e.what());
		} catch (...) {
			Utils::Log("closeAndStartButton_Click dialog task unknown exception\n");
		}
	});
}

void AppPage::ExecuteCloseAndStart() {
	auto that = this;
	auto progressToken = ::moonlight_xbox_dx::ModalDialog::ShowProgressDialogToken(nullptr, Utils::StringFromStdString("Closing app..."));
	moonlight_xbox_dx::Utils::Logf("AppPage::ExecuteCloseAndStart: progressToken=%llu\n", (unsigned long long)progressToken);

	concurrency::create_task(concurrency::create_async([that, progressToken]() {
		try {
			MoonlightClient client;
			auto ipAddr = Utils::PlatformStringToStdString(that->host->LastHostname);
			int status = client.Connect(ipAddr.c_str());
			if (status == 0) {
				client.StopApp();
				Sleep(1000);
			}
		} catch (...) {
		}

		that->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([that, progressToken]() {
				try {
					if (that->currentApp != nullptr) {
						that->Connect(that->currentApp->Id);
					}
					::moonlight_xbox_dx::ModalDialog::HideDialogByToken(progressToken);
				} catch (const std::exception &e) {
					Utils::Logf("ExecuteCloseAndStart UI exception: %s\n", e.what());
				} catch (...) {
					Utils::Log("ExecuteCloseAndStart UI unknown exception\n");
				}
			}));
	})).then([](concurrency::task<void> t) {
		try {
			t.get();
		} catch (const std::exception &e) {
			Utils::Logf("ExecuteCloseAndStart task exception: %s\n", e.what());
		} catch (...) {
			Utils::Log("ExecuteCloseAndStart unknown task exception\n");
		}
	});
}

void AppPage::closeAppButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	auto that = this;

	auto progressToken = ::moonlight_xbox_dx::ModalDialog::ShowProgressDialogToken(Utils::StringFromStdString("Closing"), Utils::StringFromStdString("Closing app..."));
	moonlight_xbox_dx::Utils::Logf("AppPage::closeAppButton_Click: progressToken=%llu\n", (unsigned long long)progressToken);

	concurrency::create_task(concurrency::create_async([that, progressToken]() {
		try {
			MoonlightClient client;
			auto ipAddr = Utils::PlatformStringToStdString(that->host->LastHostname);
			int status = client.Connect(ipAddr.c_str());
			if (status == 0) {
				client.StopApp();
				Sleep(1000);
			}
		} catch (...) {
		}

		that->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([that, progressToken]() {
			try {
				that->host->UpdateHostInfo(true);
				that->host->UpdateAppRunningStates();
			} catch (...) {
			}
			::moonlight_xbox_dx::ModalDialog::HideDialogByToken(progressToken);
		}));
	}));
}

void AppPage::backButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	this->Frame->GoBack();
}

void AppPage::settingsButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	bool result = this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), Host);
}

void AppPage::OnBackRequested(Platform::Object^ e, Windows::UI::Core::BackRequestedEventArgs^ args)
{
	// UWP on Xbox One triggers a back request whenever the B
	// button is pressed which can result in the app being
	// suspended if unhandled
	if (this->Frame->CanGoBack) {
		this->Frame->GoBack();
		args->Handled = true;
	}
                                                        }

void AppPage::OnLoaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	m_back_cookie = navigation->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &AppPage::OnBackRequested);
}

void AppPage::OnUnloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
	navigation->BackRequested -= m_back_cookie;
	// stop background polling loop
	continueAppFetch.store(false);
}

