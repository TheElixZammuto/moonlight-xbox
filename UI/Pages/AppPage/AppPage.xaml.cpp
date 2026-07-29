#include "pch.h"
#include "AppPage.xaml.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <vector>
#include "State\MoonlightClient.h"
#include "UI\Controls\SlidingMenu.xaml.h"
#include "UI\Modals\AlertDialog.xaml.h"
#include "UI\Modals\AppActionsDialog.xaml.h"
#include "UI\Modals\ConfirmDialog.xaml.h"
#include "UI\Models\ViewModels\AppPageViewModel.h"
#include "UI\Pages\HostSelectorPage.xaml.h"
#include "UI\Pages\HostSettingsPage.xaml.h"
#include "UI\Pages\MoonlightSettings.xaml.h"
#include "UI\Pages\StreamPage.xaml.h"
#define MLOG_TAG_OVERRIDE "AppPage"
#include "Utils.hpp"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Foundation::Numerics;
using namespace concurrency;

namespace moonlight_xbox_dx {

namespace {

static double ResolveBackgroundOverlayOpacityFromPageResources(Page ^ page) {
	double overlayOpacity = 0.05;
	if (page == nullptr || page->Resources == nullptr) return overlayOpacity;

	try {
		auto value = page->Resources->Lookup(ref new Platform::String(L"BackgroundOverlayOpacity"));
		auto pv = dynamic_cast<Windows::Foundation::IPropertyValue ^>(value);
		if (pv != nullptr) {
			overlayOpacity = pv->GetDouble();
		}
	} catch (...) {
	}

	if (overlayOpacity < 0.0) overlayOpacity = 0.0;
	if (overlayOpacity > 1.0) overlayOpacity = 1.0;
	return overlayOpacity;
}

static double ResolveSharedAnimationDurationMsFromPageResources(Page ^ page) {
	if (page == nullptr || page->Resources == nullptr) return 250.0;

	try {
		auto value = page->Resources->Lookup(ref new Platform::String(L"SharedAnimationDuration"));
		auto durationValue = dynamic_cast<Platform::String ^>(value);
		return Utils::DurationStringToMs(durationValue);
	} catch (...) {
	}

	return 250.0;
}

static void FindElementChildren(DependencyObject ^ container,
                                UIElement ^ &outDesaturator, UIElement ^ &outImage, UIElement ^ &outName,
                                UIElement ^ &outBlur, UIElement ^ &outPlay) {
	outDesaturator = outImage = outName = outBlur = outPlay = nullptr;
	if (container == nullptr) return;
	try {
		outDesaturator = dynamic_cast<UIElement ^>(FindChildByName(container, ref new Platform::String(L"Desaturator")));
		outImage = dynamic_cast<UIElement ^>(FindChildByName(container, ref new Platform::String(L"AppImageRect")));
		outName = dynamic_cast<UIElement ^>(FindChildByName(container, ref new Platform::String(L"AppName")));
		outBlur = dynamic_cast<UIElement ^>(FindChildByName(container, ref new Platform::String(L"AppImageBlurRect")));
		outPlay = dynamic_cast<UIElement ^>(FindChildByName(container, ref new Platform::String(L"Play")));
	} catch (...) {
		outDesaturator = outImage = outName = outBlur = outPlay = nullptr;
	}
}

}

Controls::SlidingMenu ^ AppPage::GetLeftMenu() {
	try {
		if (m_leftMenu == nullptr) m_leftMenu = ref new Controls::SlidingMenu();
		return m_leftMenu;
	} catch (...) {
		return nullptr;
	}
}

AppPage::AppPage() {
	InitializeComponent();
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()
	    ->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);

	m_apps_changed_token.Value = m_back_cookie.Value = m_keydown_cookie.Value = 0;
	m_centerWaitContainer_token.Value = 0;
	m_centerWaitScrollViewer_token.Value = 0;
	m_appsListView_selection_token.Value = m_appsListView_itemclick_token.Value = 0;
	m_appsListView_righttapped_token.Value = 0;
	m_appsListView_loaded_token.Value = m_appsListView_ccc_token.Value = 0;
	m_appsGridView_selection_token.Value = m_appsGridView_itemclick_token.Value = 0;
	m_appsGridView_righttapped_token.Value = 0;
	m_appsGridView_loaded_token.Value = m_appsGridView_ccc_token.Value = 0;
	m_searchbox_gettingfocus_token.Value = 0;
	m_centerDebounceTimer_token.Value = 0;

	{
		auto weakThis = WeakReference(this);
		this->Loaded += ref new RoutedEventHandler([weakThis](Platform::Object ^ s, RoutedEventArgs ^ e) {
			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr) return;
			try {
				that->OnLoaded(s, e);
			} catch (...) {
				MLOGF(Utils::LogLevel::Error, "OnLoaded failed, page lifecycle wiring may be incomplete\n");
			}
		});
	}
	{
		auto weakThis = WeakReference(this);
		this->Unloaded += ref new RoutedEventHandler([weakThis](Platform::Object ^ s, RoutedEventArgs ^ e) {
			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr) return;
			try {
				that->OnUnloaded(s, e);
			} catch (...) {
				MLOGF(Utils::LogLevel::Error, "OnUnloaded failed, cleanup may be incomplete\n");
			}
		});
	}
	try {
		WireAppsGridEvents(this->AppsListView,
		                   m_appsListView_selection_token, m_appsListView_itemclick_token,
		                   m_appsListView_righttapped_token, m_appsListView_loaded_token, m_appsListView_ccc_token);
	} catch (...) {
		MLOGF(Utils::LogLevel::Error, "AppsListView event wiring failed, selection/click/layout events permanently unavailable\n");
	}

	try {
		WireAppsGridEvents(this->AppsGridView,
		                   m_appsGridView_selection_token, m_appsGridView_itemclick_token,
		                   m_appsGridView_righttapped_token, m_appsGridView_loaded_token, m_appsGridView_ccc_token);
	} catch (...) {
		MLOGF(Utils::LogLevel::Error, "AppsGridView event wiring failed, selection/click/layout events permanently unavailable\n");
	}

	try {
		if (this->SearchBox != nullptr) {
			auto weakThis = WeakReference(this);
			m_searchbox_gettingfocus_token = this->SearchBox->GettingFocus +=
			    ref new Windows::Foundation::TypedEventHandler<
			        Windows::UI::Xaml::UIElement ^,
			        Windows::UI::Xaml::Input::GettingFocusEventArgs ^>(
			        [weakThis](Windows::UI::Xaml::UIElement ^, Windows::UI::Xaml::Input::GettingFocusEventArgs ^ args) {
				        auto that = weakThis.Resolve<AppPage>();
				        if (that == nullptr) return;
				        try {
					        if (that->m_searchIsOpen) return;
					        if (args->Direction == FocusNavigationDirection::Up) {
						        that->m_searchIsOpen = true;
						        return;
					        }
					        args->Cancel = true;
				        } catch (...) {
				        }
			        });
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "SearchBox GettingFocus wiring failed, DPad-Up-to-search navigation permanently unavailable\n");
	}

	m_filteredApps = ref new Platform::Collections::Vector<MoonlightApp ^>();
}

void AppPage::WireAppsGridEvents(
    ListView ^ grid,
    Windows::Foundation::EventRegistrationToken &selectionToken,
    Windows::Foundation::EventRegistrationToken &itemClickToken,
    Windows::Foundation::EventRegistrationToken &rightTappedToken,
    Windows::Foundation::EventRegistrationToken &loadedToken,
    Windows::Foundation::EventRegistrationToken &cccToken) {
	if (grid == nullptr) return;
	auto weakThis = WeakReference(this);
	selectionToken = grid->SelectionChanged +=
	    ref new SelectionChangedEventHandler([weakThis](Platform::Object ^ s, SelectionChangedEventArgs ^ e) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) try {
				    that->AppsGrid_SelectionChanged(s, e);
			    } catch (...) {
			    }
	    });
	itemClickToken = grid->ItemClick +=
	    ref new ItemClickEventHandler([weakThis](Platform::Object ^ s, ItemClickEventArgs ^ e) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) try {
				    that->AppsGrid_ItemClick(s, e);
			    } catch (...) {
			    }
	    });
	rightTappedToken = grid->RightTapped +=
	    ref new RightTappedEventHandler([weakThis](Platform::Object ^ s, RightTappedRoutedEventArgs ^ e) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) try {
				    that->AppsGrid_RightTapped(s, e);
			    } catch (...) {
			    }
	    });
	loadedToken = grid->Loaded +=
	    ref new RoutedEventHandler([weakThis](Platform::Object ^ s, RoutedEventArgs ^ e) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) try {
				    that->AppsGrid_Loaded(s, e);
			    } catch (...) {
			    }
	    });
	cccToken = grid->ContainerContentChanging +=
	    ref new TypedEventHandler<ListViewBase ^, ContainerContentChangingEventArgs ^>(
	        [weakThis](ListViewBase ^ s, ContainerContentChangingEventArgs ^ args) {
		        auto that = weakThis.Resolve<AppPage>();
		        if (that) try {
				        that->AppsGrid_ContainerContentChanging(s, args);
			        } catch (...) {
			        }
	        });
}

void AppPage::UnwireAppsGridEvents(
    const char *gridName,
    ListView ^ grid,
    Windows::Foundation::EventRegistrationToken selectionToken,
    Windows::Foundation::EventRegistrationToken itemClickToken,
    Windows::Foundation::EventRegistrationToken rightTappedToken,
    Windows::Foundation::EventRegistrationToken loadedToken,
    Windows::Foundation::EventRegistrationToken cccToken) {
	if (grid == nullptr) return;
	try {
		grid->SelectionChanged -= selectionToken;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: %s SelectionChanged -= failed, handler leaked\n", gridName);
	}
	try {
		grid->ItemClick -= itemClickToken;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: %s ItemClick -= failed, handler leaked\n", gridName);
	}
	try {
		grid->RightTapped -= rightTappedToken;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: %s RightTapped -= failed, handler leaked\n", gridName);
	}
	try {
		grid->Loaded -= loadedToken;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: %s Loaded -= failed, handler leaked\n", gridName);
	}
	try {
		grid->ContainerContentChanging -= cccToken;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: %s ContainerContentChanging -= failed, handler leaked\n", gridName);
	}
}

void AppPage::OnLoaded(Platform::Object ^, RoutedEventArgs ^) {
	Platform::WeakReference weakThis(this);
	m_back_cookie = Windows::UI::Core::SystemNavigationManager::GetForCurrentView()->BackRequested +=
	    ref new EventHandler<BackRequestedEventArgs ^>([weakThis](Platform::Object ^ s, BackRequestedEventArgs ^ args) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) that->OnBackRequested(s, args);
	    });

	try {
		auto window = CoreApplication::MainView->CoreWindow;
		if (window != nullptr) {
			m_keydown_cookie = window->KeyDown +=
			    ref new TypedEventHandler<CoreWindow ^, KeyEventArgs ^>([weakThis](CoreWindow ^ s, KeyEventArgs ^ args) {
				    auto that = weakThis.Resolve<AppPage>();
				    if (that) that->OnGamepadKeyDown(s, args);
			    });
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Error, "OnLoaded: KeyDown subscribe failed, gamepad input permanently unavailable this page instance\n");
	}

	try {
		if (this->ViewModel != nullptr && this->PageBackgroundImage != nullptr) {
			this->ViewModel->SetPageBackgroundBorder(this->PageBackgroundImage);
			this->ViewModel->SetBackgroundTransitionSettings(
			    ResolveSharedAnimationDurationMsFromPageResources(this),
			    ResolveBackgroundOverlayOpacityFromPageResources(this));
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnLoaded: ViewModel background setup failed, background transitions may not apply\n");
	}

	StartBgPanAnimation();
	StartBannerSlideInAnimation();
}

void AppPage::OnUnloaded(Platform::Object ^, RoutedEventArgs ^) {
	if (m_selectedApp != nullptr) {
		try {
			m_selectedApp->IsSelected = false;
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "OnUnloaded: IsSelected=false failed for app id=%d\n", m_selectedApp->Id);
		}
	}
	m_selectedApp = nullptr;
	m_initialFocusApplied = false;
	m_appsGridRevealed = false;
	try {
		Windows::UI::Core::SystemNavigationManager::GetForCurrentView()->BackRequested -= m_back_cookie;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: BackRequested -= failed, handler leaked\n");
	}
	try {
		if (this->SearchBox != nullptr) this->SearchBox->GettingFocus -= m_searchbox_gettingfocus_token;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: SearchBox GettingFocus -= failed, handler leaked\n");
	}
	continueAppFetch.store(false);
	try {
		PollingIndicator->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnUnloaded: PollingIndicator visibility reset failed\n");
	}

	auto window = CoreApplication::MainView->CoreWindow;
	if (window != nullptr) {
		try {
			window->KeyDown -= m_keydown_cookie;
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "OnUnloaded: KeyDown -= failed, handler leaked\n");
		}
	}

	if (this->host != nullptr && this->host->Apps != nullptr) {
		auto obs = dynamic_cast<Windows::Foundation::Collections::IObservableVector<MoonlightApp ^> ^>(this->host->Apps);
		if (obs != nullptr) {
			try {
				obs->VectorChanged -= m_apps_changed_token;
			} catch (...) {
				MLOGF(Utils::LogLevel::Warning, "OnUnloaded: VectorChanged -= failed, handler leaked\n");
			}
		}
	}

	if (m_centerDebounceTimer != nullptr) {
		try {
			m_centerDebounceTimer->Stop();
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "OnUnloaded: m_centerDebounceTimer->Stop() failed, timer may keep firing after teardown");
		}
		try {
			m_centerDebounceTimer->Tick -= m_centerDebounceTimer_token;
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "OnUnloaded: m_centerDebounceTimer Tick -= failed, handler leaked");
		}
		m_centerDebounceTimer = nullptr;
	}

	if (m_bgPanStoryboard != nullptr) {
		m_bgPanStoryboard->Stop();
		m_bgPanStoryboard = nullptr;
	}
	UnwireAppsGridEvents("AppsListView", this->AppsListView,
	                     m_appsListView_selection_token, m_appsListView_itemclick_token,
	                     m_appsListView_righttapped_token, m_appsListView_loaded_token, m_appsListView_ccc_token);
	UnwireAppsGridEvents("AppsGridView", this->AppsGridView,
	                     m_appsGridView_selection_token, m_appsGridView_itemclick_token,
	                     m_appsGridView_righttapped_token, m_appsGridView_loaded_token, m_appsGridView_ccc_token);
	CancelContainerRealizedWait(m_centerWaitContainerTarget, m_centerWaitContainer_token);
	m_centerWaitContainerTarget = nullptr;
	CancelLayoutUpdatedWait(m_centerWaitScrollViewerTarget, m_centerWaitScrollViewer_token);
	m_centerWaitScrollViewerTarget = nullptr;
}

void AppPage::OnNavigatedTo(NavigationEventArgs ^ e) {
	MoonlightHost ^ mhost = dynamic_cast<MoonlightHost ^>(e->Parameter);
	if (mhost == nullptr) return;
	host = mhost;

	bool willAutoStartApp = (host->AutostartID >= 0 && GetApplicationState()->shouldAutoConnect);

	continueAppFetch.store(true);
	wasConnected.store(true);
	if (!willAutoStartApp) FadeInPollingIndicator();

	try {
		if (this->ClosingOverlayText != nullptr)
			this->ClosingOverlayText->Text = ref new Platform::String(L"Loading apps...");
		if (this->ClosingOverlay != nullptr) {
			this->ClosingOverlay->Opacity = 1.0;
			this->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;
		}
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "OnNavigatedTo: failed to configure closing overlay text/visibility");
	}

	{
		Platform::WeakReference weakThis(this);
		create_task([weakThis]() {
			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr || !that->continueAppFetch.load()) return;
			that->host->UpdateApps();
		});
	}

	if (!willAutoStartApp) {
		Platform::WeakReference weakThis(this);
		create_task([weakThis]() {
			while (true) {
				auto that = weakThis.Resolve<AppPage>();
				if (that == nullptr) break;
				if (!that->continueAppFetch.load()) break;
				CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
				    CoreDispatcherPriority::Normal,
				    ref new DispatchedHandler([weakThis]() {
					    auto ui = weakThis.Resolve<AppPage>();
					    if (ui) try {
							    ui->FadeInPollingIndicator();
						    } catch (...) {
						    }
				    }));
				try {
					that->PollAppRunningAndConnectivity();
				} catch (const std::exception &e) {
					MLOGF(Utils::LogLevel::Error, "Failed to poll app and host running state. Exception: %s\n", e.what());
				} catch (...) {
					MLOG(Utils::LogLevel::Error, "Failed to poll app and host running state. Unknown Exception.\n");
				}
				Sleep(3000);
				CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
				    CoreDispatcherPriority::Normal,
				    ref new DispatchedHandler([weakThis]() {
					    auto ui = weakThis.Resolve<AppPage>();
					    if (ui) try {
							    ui->FadeOutPollingIndicator();
						    } catch (...) {
						    }
				    }));
				Sleep(7000);
			}
		});
	}

	try {
		auto obs = dynamic_cast<Windows::Foundation::Collections::IObservableVector<MoonlightApp ^> ^>(host->Apps);
		if (obs != nullptr) {
			Platform::WeakReference weakThis(this);
			m_apps_changed_token = obs->VectorChanged +=
			    ref new Windows::Foundation::Collections::VectorChangedEventHandler<MoonlightApp ^>(
			        [weakThis](Windows::Foundation::Collections::IObservableVector<MoonlightApp ^> ^ sender,
			                   Windows::Foundation::Collections::IVectorChangedEventArgs ^ args) {
				        auto that = weakThis.Resolve<AppPage>();
				        if (that) try {
						        that->OnHostAppsChanged(sender, args);
					        } catch (...) {
					        }
			        });
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Error, "OnNavigatedTo: VectorChanged subscribe failed, app-list updates permanently unavailable this session\n");
	}

	try {
		bool savedGrid = (host->Personalization->AppView == AppHostView::Grid);
		if (savedGrid != m_isGridLayout && LayoutToggleButton != nullptr) {
			LayoutToggleButton->IsChecked = savedGrid;
			LayoutToggleButton_Click(LayoutToggleButton, nullptr);
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "OnNavigatedTo: saved layout restore failed for host, defaulting to current layout\n");
	}

	ArmEntranceAnimations();

	{
		Windows::UI::Color accentColor = host->Personalization->UseSystemAccent
		                                     ? (ref new Windows::UI::ViewManagement::UISettings())
		                                           ->GetColorValue(Windows::UI::ViewManagement::UIColorType::Accent)
		                                     : host->Personalization->AccentColor;
		ApplyAccentColor(accentColor);

		auto cur = this->ActualTheme;
		this->RequestedTheme = (cur != ElementTheme::Dark) ? ElementTheme::Dark : ElementTheme::Light;
		this->RequestedTheme = ElementTheme::Default;
	}

	if (willAutoStartApp) {
		GetApplicationState()->shouldAutoConnect = false;
		CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
		    CoreDispatcherPriority::High, ref new DispatchedHandler([this]() {
			    this->Connect(host->AutostartID);
		    }));
	}
	GetApplicationState()->shouldAutoConnect = false;
}

void AppPage::OnBackRequested(Platform::Object ^, BackRequestedEventArgs ^ args) {
	auto lm = this->GetLeftMenu();
	if (lm != nullptr && lm->IsOpen) {
		lm->Close();
		args->Handled = true;
		return;
	}

	if (this->Frame->CanGoBack) {
		this->Frame->GoBack();
		args->Handled = true;
	}
}

void AppPage::backButton_Click(Platform::Object ^, RoutedEventArgs ^) {
	this->Frame->GoBack();
}

void AppPage::PollAppRunningAndConnectivity() {
	if (this->host == nullptr) return;
	this->host->UpdateAppRunningStates();
	if (this->wasConnected.load() && !this->host->Connected) {
		this->wasConnected.store(false);
		auto weakThis = WeakReference(this);
		CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
		    CoreDispatcherPriority::Normal,
		    ref new DispatchedHandler([weakThis]() {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) that->ShowDisconnectedDialogAndNavigateBack();
		    }));
	} else if (!this->wasConnected.load() && this->host->Connected) {
		this->wasConnected.store(true);
	}
}

void AppPage::ShowDisconnectedDialogAndNavigateBack() {
	try {
		auto dialog = ref new ::moonlight_xbox_dx::AlertDialog();
		dialog->Configure(L"Disconnected", L"Connection to host was lost.");
		try {
			dialog->XamlRoot = this->XamlRoot;
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "ShowDisconnectedDialogAndNavigateBack: failed to set dialog XamlRoot, dialog may not display correctly");
		}
		Platform::WeakReference weakThis(this);
		create_task(dialog->ShowAsync()).then([weakThis](ContentDialogResult result) {
			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr) return;
			that->Dispatcher->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([that]() {
				                           try {
					                           auto slideBack = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
					                           slideBack->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromLeft;
					                           that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSelectorPage::typeid), nullptr, slideBack);
				                           } catch (const std::exception &e) {
					                           MLOGF(Utils::LogLevel::Error, "Failed to navigate to HostSelectorPage after disconnect. Exception: %s\n", e.what());
				                           } catch (...) {
					                           MLOG(Utils::LogLevel::Error, "Failed to navigate to HostSelectorPage after disconnect. Unknown Exception.\n");
				                           }
			                           }));
		});
	} catch (const std::exception &e) {
		MLOGF(Utils::LogLevel::Error, "Failed to show disconnect dialog. Exception: %s\n", e.what());
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "Failed to show disconnect dialog. Unknown Exception.\n");
	}
}

bool AppPage::ApplyAppFilter(Platform::String ^ filter) {
	auto host = this->Host;
	auto vec = this->m_filteredApps;
	if (vec == nullptr) return false;

	if (host == nullptr || host->Apps == nullptr) {
		vec->Clear();
		return false;
	}

	auto newResults = ref new Platform::Collections::Vector<MoonlightApp ^>();
	bool empty = (filter == nullptr || filter->Length() == 0);
	std::wstring fw;
	if (!empty) {
		std::string fstr = Utils::PlatformStringToStdString(filter);
		fw = Utils::NarrowToWideString(fstr);
		std::transform(fw.begin(), fw.end(), fw.begin(), ::towlower);
	}

	std::vector<MoonlightApp ^> emptyOrdered;
	for (unsigned int i = 0; i < host->Apps->Size; ++i) {
		auto app = host->Apps->GetAt(i);
		if (app == nullptr) continue;
		if (empty) {
			emptyOrdered.push_back(app);
		} else {
			std::string nstr = Utils::PlatformStringToStdString(app->Name != nullptr ? app->Name : ref new Platform::String(L""));
			std::wstring namew = Utils::NarrowToWideString(nstr);
			std::transform(namew.begin(), namew.end(), namew.begin(), ::towlower);
			if (namew.find(fw) != std::wstring::npos) newResults->Append(app);
		}
	}
	if (empty) {

		auto &favoriteIds = host->favoriteAppIds;
		std::stable_sort(emptyOrdered.begin(), emptyOrdered.end(), [&favoriteIds](MoonlightApp ^ a, MoonlightApp ^ b) {
			if (a->IsFavorite != b->IsFavorite) return a->IsFavorite;
			if (!a->IsFavorite) return false;
			auto itA = std::find(favoriteIds.begin(), favoriteIds.end(), a->Id);
			auto itB = std::find(favoriteIds.begin(), favoriteIds.end(), b->Id);
			return itA < itB;
		});
		for (auto app : emptyOrdered) newResults->Append(app);
	}

	bool identical = false;
	try {
		if (vec->Size == newResults->Size) {
			identical = true;
			for (unsigned int i = 0; i < vec->Size; ++i) {
				auto a = vec->GetAt(i), b = newResults->GetAt(i);
				if ((a != nullptr ? a->Id : -1) != (b != nullptr ? b->Id : -1)) {
					identical = false;
					break;
				}
			}
		}
	} catch (...) {
		identical = false;
	}
	if (identical) {
		try {
			for (unsigned int i = 0; i < vec->Size; ++i) {
				auto a = vec->GetAt(i), b = newResults->GetAt(i);
				if (a != b) {
					b->IsSelected = a->IsSelected;
					b->BlurredImage = a->BlurredImage;
					b->GlowImage = a->GlowImage;
					if (a->Image != nullptr) b->Image = a->Image;
					if (m_selectedApp != nullptr && m_selectedApp->Id == a->Id)
						m_selectedApp = b;

					vec->SetAt(i, b);
				}
			}
		} catch (...) {
		}
	} else {
		vec->Clear();
		for (unsigned int i = 0; i < newResults->Size; ++i) vec->Append(newResults->GetAt(i));

		try {
			bool emptyResults = (vec->Size == 0);
			auto weakThis = WeakReference(this);
			this->Dispatcher->RunAsync(CoreDispatcherPriority::Normal,
			                           ref new DispatchedHandler([weakThis, emptyResults]() {
				                           try {
					                           auto that = weakThis.Resolve<AppPage>();
					                           if (that == nullptr) return;
					                           if (that->NoAppsMessage != nullptr)
						                           that->NoAppsMessage->Visibility = emptyResults ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;

					                           if (emptyResults) try {
							                           that->RevealActiveAppsGrid();
						                           } catch (...) {
						                           }
					                           if (emptyResults && that->SelectedAppText != nullptr && that->SelectedAppBox != nullptr) {
						                           auto sb = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard ^>(
						                               that->Resources->Lookup(ref new Platform::String(L"HideSelectedAppStoryboard")));
						                           if (sb != nullptr) sb->Begin();
					                           }
				                           } catch (...) {
				                           }
			                           }));
		} catch (...) {
		}
	}

	auto activeGrid = this->ActiveAppsGrid();
	if (vec->Size > 0 && activeGrid != nullptr && activeGrid->SelectedIndex < 0)
		activeGrid->SelectedIndex = 0;
	return identical;
}

void AppPage::OnHostAppsChanged(
    Windows::Foundation::Collections::IObservableVector<MoonlightApp ^> ^,
    Windows::Foundation::Collections::IVectorChangedEventArgs ^) {
	try {
		auto weakThis = WeakReference(this);
		this->Dispatcher->RunAsync(CoreDispatcherPriority::Normal,
		                           ref new DispatchedHandler([weakThis]() {
			                           try {
				                           auto that = weakThis.Resolve<AppPage>();
				                           if (that) that->ApplyAppFilter(that->SearchBox != nullptr ? that->SearchBox->Text : nullptr);
			                           } catch (...) {
			                           }
		                           }));
	} catch (...) {
	}
}

void AppPage::SearchBox_TextChanged(Platform::Object ^ sender, TextChangedEventArgs ^) {
	try {
		auto tb = dynamic_cast<TextBox ^>(sender);
		if (tb == nullptr) return;
		ArmEntranceAnimations();
		ApplyAppFilter(tb->Text);
	} catch (...) {
	}
}

Windows::UI::Xaml::Controls::ListView ^ AppPage::ActiveAppsGrid() {
	return m_isGridLayout ? this->AppsGridView : this->AppsListView;
}

void AppPage::LayoutToggleButton_Click(Platform::Object ^ sender, RoutedEventArgs ^) {
	try {
		auto toggle = dynamic_cast<Windows::UI::Xaml::Controls::Primitives::ToggleButton ^>(sender);
		bool wantGrid = toggle != nullptr && toggle->IsChecked != nullptr && toggle->IsChecked->Value;
		if (m_isGridLayout == wantGrid) return;

		CancelContainerRealizedWait(m_centerWaitContainerTarget, m_centerWaitContainer_token);
		m_centerWaitContainerTarget = nullptr;
		CancelLayoutUpdatedWait(m_centerWaitScrollViewerTarget, m_centerWaitScrollViewer_token);
		m_centerWaitScrollViewerTarget = nullptr;

		auto oldGrid = this->ActiveAppsGrid();
		auto selectedItem = (oldGrid != nullptr) ? oldGrid->SelectedItem : nullptr;

		m_isGridLayout = wantGrid;

		if (host != nullptr) {
			host->Personalization->AppView = wantGrid ? AppHostView::Grid : AppHostView::List;
			GetApplicationState()->UpdateFile();
		}

		VisualStateManager::GoToState(this, wantGrid ? "GridLayout" : "ListLayout", false);

		if (this->AppsListView != nullptr)
			this->AppsListView->Visibility = wantGrid ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
		if (this->AppsGridView != nullptr)
			this->AppsGridView->Visibility = wantGrid ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;

		m_scrollViewer = nullptr;

		if (this->Host != nullptr && this->Host->Apps != nullptr) {
			for (int i = 0; i < (int)this->Host->Apps->Size; ++i) {
				auto app = this->Host->Apps->GetAt(i);
				app->BlurredImage = nullptr;
				app->GlowImage = nullptr;
				this->Host->Apps->SetAt(i, app);
			}
		}

		auto newGrid = this->ActiveAppsGrid();
		if (newGrid == nullptr) return;

		m_suppressSelectionVisuals = true;
		if (selectedItem != nullptr) {
			try {
				newGrid->SelectedItem = selectedItem;
			} catch (...) {
			}
			try {
				newGrid->ScrollIntoView(selectedItem);
			} catch (...) {
			}
		}
		m_suppressSelectionVisuals = false;

		try {
			newGrid->UpdateLayout();
		} catch (...) {
		}
		try {
			this->UpdateItemHeights();
		} catch (...) {
		}

		try {
			if (newGrid->SelectedItem != nullptr) {
				auto c2 = dynamic_cast<ListViewItem ^>(newGrid->ContainerFromItem(newGrid->SelectedItem));
				if (c2 != nullptr) {
					UIElement ^ des2 = nullptr, ^img2 = nullptr, ^nm2 = nullptr;
					UIElement ^ bl2 = nullptr, ^pl2 = nullptr;
					FindElementChildren(c2, des2, img2, nm2, bl2, pl2);
					auto resetCP = [](UIElement ^ el) {
						if (el == nullptr) return;
						auto fe = dynamic_cast<FrameworkElement ^>(el);
						if (fe == nullptr || fe->ActualWidth <= 0 || fe->ActualHeight <= 0) return;
						auto vis = ElementCompositionPreview::GetElementVisual(el);
						if (vis == nullptr) return;
						Windows::Foundation::Numerics::float3 cp;
						cp.x = (float)fe->ActualWidth * 0.5f;
						cp.y = (float)fe->ActualHeight * 0.5f;
						cp.z = 0.0f;
						vis->CenterPoint = cp;
					};
					resetCP(img2);
					resetCP(des2);
					resetCP(pl2);
				}
			}
		} catch (...) {
		}

		try {
			auto app = dynamic_cast<MoonlightApp ^>(newGrid->SelectedItem);
			if (app != nullptr)
				this->ApplySelectionVisuals(app, false, true);
			else
				this->CenterSelectedItem(true);
		} catch (...) {
		}

		try {
			if (newGrid->SelectedItem != nullptr) {
				auto sel = dynamic_cast<ListViewItem ^>(newGrid->ContainerFromItem(newGrid->SelectedItem));
				if (sel != nullptr)
					sel->Focus(Windows::UI::Xaml::FocusState::Programmatic);
				else
					newGrid->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			} else {
				newGrid->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			}
		} catch (...) {
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Error, "LayoutToggleButton_Click failed, layout toggle may be left in an inconsistent visual state\n");
	}
}

void AppPage::AppsGrid_Loaded(Platform::Object ^ sender, RoutedEventArgs ^) {
	try {
		auto grid = dynamic_cast<ListViewBase ^>(sender);
		if (grid != nullptr && grid == this->ActiveAppsGrid() && m_scrollViewer == nullptr)
			m_scrollViewer = FindScrollViewer(grid);
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "AppsGrid_Loaded: FindScrollViewer failed, m_scrollViewer left unset");
	}
}

void AppPage::OnGamepadKeyDown(CoreWindow ^, KeyEventArgs ^ args) {
	try {
		using namespace Windows::System;
		auto key = args->VirtualKey;

		if (key == VirtualKey::GamepadY || key == VirtualKey::Y) {
			try {
				this->HandleYButtonPress();
			} catch (...) {
			}
			args->Handled = true;
		}

		if (key == VirtualKey::B) {
			try {
				auto lm = this->GetLeftMenu();
				if (lm != nullptr && lm->IsOpen) {
					lm->Close();
					args->Handled = true;
				} else if (this->Frame->CanGoBack) {
					this->Frame->GoBack();
					args->Handled = true;
				}
			} catch (...) {
			}
		}

		if (key == VirtualKey::GamepadX || key == VirtualKey::X) {
			try {
				this->HandleXButtonPress();
			} catch (...) {
			}
			args->Handled = true;
		}

		bool searchHasFocus = this->SearchBox != nullptr &&
		                      this->SearchBox->FocusState != Windows::UI::Xaml::FocusState::Unfocused;

		if (key == VirtualKey::GamepadDPadUp && !searchHasFocus && !m_isGridLayout) {
			m_searchIsOpen = true;
			try {
				if (this->SearchBox != nullptr &&
				    this->SearchBox->FocusState == Windows::UI::Xaml::FocusState::Unfocused)
					this->SearchBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			} catch (...) {
			}
		}

		if (key == VirtualKey::GamepadDPadDown && searchHasFocus) {
			m_searchIsOpen = false;
			try {
				this->FocusSelectedContainerOrGrid();
			} catch (...) {
			}
			args->Handled = true;
		}

	} catch (...) {
	}
}

void AppPage::HandleYButtonPress() {
	bool searchHasFocus = this->SearchBox != nullptr &&
	                      this->SearchBox->FocusState != Windows::UI::Xaml::FocusState::Unfocused;
	if (!searchHasFocus) {
		this->m_searchIsOpen = true;
		this->SearchBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
		return;
	}
	this->m_searchIsOpen = false;
	this->FocusSelectedContainerOrGrid();
}

void AppPage::HandleXButtonPress() {
	bool newState = !this->m_isGridLayout;
	if (this->LayoutToggleButton) this->LayoutToggleButton->IsChecked = newState;
	this->LayoutToggleButton_Click(this->LayoutToggleButton, nullptr);
}

void AppPage::FocusSelectedContainerOrGrid() {
	auto lv = this->ActiveAppsGrid();
	if (lv == nullptr) return;
	if (lv->SelectedItem != nullptr) {
		auto c = dynamic_cast<ListViewItem ^>(lv->ContainerFromItem(lv->SelectedItem));
		if (c != nullptr) {
			c->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			return;
		}
	}
	lv->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void AppPage::AppsGrid_ItemClick(Platform::Object ^ sender, ItemClickEventArgs ^ e) {
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
	config->enableStats = host->EnableStats;
	config->enableGraphs = host->EnableGraphs;
	if (config->enableHDR) host->VideoCodec = "HEVC (H.265)";
	config->backgroundImage = (this->currentApp != nullptr) ? this->currentApp->BlurredImage : nullptr;
	config->appName = (this->currentApp != nullptr) ? this->currentApp->Name : nullptr;
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(StreamPage::typeid), config, ref new Windows::UI::Xaml::Media::Animation::DrillInNavigationTransitionInfo());
}

void AppPage::AppsGrid_RightTapped(Platform::Object ^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs ^ e) {
	FrameworkElement ^ senderElement = dynamic_cast<FrameworkElement ^>(e != nullptr ? e->OriginalSource : nullptr);
	if (senderElement != nullptr) {
		if (senderElement->GetType()->FullName->Equals(ListViewItem::typeid->FullName)) {
			currentApp = (MoonlightApp ^)((ListViewItem ^) senderElement)->Content;
		} else {
			currentApp = dynamic_cast<MoonlightApp ^>(senderElement->DataContext);
			if (currentApp == nullptr) {
				auto activeGrid = this->ActiveAppsGrid();
				if (activeGrid != nullptr && activeGrid->SelectedIndex >= 0)
					currentApp = (MoonlightApp ^) activeGrid->SelectedItem;
			}
		}
	}

	bool anyRunning = false;
	if (this->host != nullptr) {
		for (unsigned int i = 0; i < this->host->Apps->Size; ++i) {
			auto c = this->host->Apps->GetAt(i);
			if (c != nullptr && c->CurrentlyRunning) {
				anyRunning = true;
				break;
			}
		}
	}

	if (this->currentApp == nullptr) {
		if (e != nullptr) e->Handled = false;
		return;
	}

	try {
		Platform::WeakReference weakThis(this);
		auto dialog = ref new AppActionsDialog();
		dialog->Configure(
		    (this->currentApp->Name != nullptr) ? this->currentApp->Name : ref new Platform::String(L""),
		    this->currentApp->CurrentlyRunning,
		    !this->currentApp->CurrentlyRunning && anyRunning,
		    !this->currentApp->CurrentlyRunning && !anyRunning,
		    this->currentApp->IsFavorite,
		    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) try {
					    that->Connect(that->currentApp->Id);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog Resume handler failed to Connect");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis, dialog](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) try {
					    that->closeAppButton_Click(dialog, nullptr);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog Close handler failed to close running app");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis, dialog](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) try {
					    that->closeAndStartButton_Click(dialog, nullptr);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog CloseAndStart handler failed to close and start app");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr && that->currentApp != nullptr) try {
					    that->Connect(that->currentApp->Id);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog Start handler failed to Connect");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) try {
					    auto t = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
					    t->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
					    that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid), nullptr, t);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog MoonlightSettings handler failed to navigate");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr) try {
					    auto t = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
					    t->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
					    that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), that->Host, t);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog HostSettings handler failed to navigate");
				    }
		    }),
		    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr && that->currentApp != nullptr && that->host != nullptr) try {
					    that->host->ToggleFavorite(that->currentApp->Id);
					    GetApplicationState()->UpdateFile();
					    that->ApplyAppFilter(that->SearchBox != nullptr ? that->SearchBox->Text : nullptr);
				    } catch (...) {
					    MLOG(Utils::LogLevel::Warning, "AppActionsDialog ToggleFavorite handler failed");
				    }
		    }));

		create_task(dialog->ShowAsync());
		if (e != nullptr) e->Handled = true;
	} catch (...) {
		if (e != nullptr) e->Handled = false;
	}
}

Platform::String ^ AppPage::FindRunningAppName() {
	if (this->host == nullptr) return nullptr;
	for (unsigned int i = 0; i < this->host->Apps->Size; ++i) {
		auto candidate = this->host->Apps->GetAt(i);
		if (candidate != nullptr && candidate->CurrentlyRunning &&
		    (this->currentApp == nullptr || candidate->Id != this->currentApp->Id)) {
			return candidate->Name;
		}
	}
	return nullptr;
}

void AppPage::closeAndStartButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	if (this->currentApp == nullptr) return;
	if (sender != nullptr) {
		this->ExecuteCloseAndStart();
		return;
	}

	Platform::String ^ runningName = this->FindRunningAppName();

	auto startName = (this->currentApp->Name != nullptr && this->currentApp->Name->Length() > 0)
	                     ? std::wstring(this->currentApp->Name->Data())
	                     : std::wstring(L"this app");
	auto closePart = (runningName != nullptr && runningName->Length() > 0)
	                     ? std::wstring(L"Close '") + runningName->Data() + L"'"
	                     : std::wstring(L"Close the currently running app");
	Platform::String ^ message = ref new Platform::String((closePart + L" and start '" + startName + L"'?").c_str());

	Platform::WeakReference weakThis(this);
	auto dialog = ref new ConfirmDialog();
	dialog->Configure(
	    ref new Platform::String(L"Close & Start"),
	    message,
	    ref new RoutedEventHandler([weakThis](Platform::Object ^, RoutedEventArgs ^) {
		    auto that = weakThis.Resolve<AppPage>();
		    if (that) that->ExecuteCloseAndStart();
	    }));
	create_task(dialog->ShowAsync());
}

void AppPage::CloseRunningAppOnHostAsync(
    const char *callerName,
    CoreDispatcherPriority uiPriority,
    std::function<void(AppPage ^)> onClosedUIThread) {
	Platform::WeakReference weakThis(this);
	create_task(create_async([weakThis, callerName, uiPriority, onClosedUIThread]() {
		try {
			auto thatLocal = weakThis.Resolve<AppPage>();
			if (thatLocal == nullptr) return;
			MoonlightClient client;
			auto ipAddr = Utils::PlatformStringToStdString(thatLocal->host->LastHostname);
			if (client.Connect(ipAddr.c_str()) == 0) {
				client.StopApp();
				Sleep(1000);
			}
		} catch (...) {
			MLOGF(Utils::LogLevel::Error, "%s: StopApp on host failed, running app may not actually be closed\n", callerName);
		}
		auto thatLocal2 = weakThis.Resolve<AppPage>();
		if (thatLocal2 == nullptr) return;
		thatLocal2->Dispatcher->RunAsync(uiPriority,
		                                 ref new DispatchedHandler([weakThis, callerName, onClosedUIThread]() {
			                                 auto thatUI = weakThis.Resolve<AppPage>();
			                                 try {
				                                 if (thatUI != nullptr) onClosedUIThread(thatUI);
			                                 } catch (...) {
				                                 MLOGF(Utils::LogLevel::Warning, "%s: post-close UI update failed, ClosingOverlay may stay visible\n", callerName);
			                                 }
		                                 }));
	})).then([](concurrency::task<void> t) {
		try {
			t.get();
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "CloseRunningAppOnHostAsync background task faulted");
		}
	});
}

void AppPage::ExecuteCloseAndStart() {
	auto name = this->FindRunningAppName();
	this->ClosingOverlayText->Text = (name != nullptr && name->Length() > 0)
	                                     ? ref new Platform::String((std::wstring(L"Closing ") + name->Data() + L"...").c_str())
	                                     : ref new Platform::String(L"Closing...");
	this->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;

	CloseRunningAppOnHostAsync("ExecuteCloseAndStart", CoreDispatcherPriority::High, [](AppPage ^ thatUI) {
		thatUI->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		if (thatUI->currentApp != nullptr)
			thatUI->Connect(thatUI->currentApp->Id);
	});
}

void AppPage::closeAppButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	auto name = (this->currentApp != nullptr && this->currentApp->Name != nullptr) ? this->currentApp->Name : nullptr;
	this->ClosingOverlayText->Text = (name != nullptr && name->Length() > 0)
	                                     ? ref new Platform::String((std::wstring(L"Closing ") + name->Data() + L"...").c_str())
	                                     : ref new Platform::String(L"Closing...");
	this->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Visible;

	CloseRunningAppOnHostAsync("closeAppButton_Click", CoreDispatcherPriority::Normal, [](AppPage ^ thatUI) {
		thatUI->host->UpdateHostInfo(true);
		thatUI->host->UpdateAppRunningStates();
		thatUI->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	});
}

void AppPage::FadeInPollingIndicator() {
	try {
		using namespace Windows::UI::Xaml::Media::Animation;
		PollingIndicator->Opacity = 0.0;
		PollingIndicator->Visibility = Windows::UI::Xaml::Visibility::Visible;
		auto anim = ref new DoubleAnimation();
		anim->To = ref new Platform::Box<double>(0.3);
		TimeSpan ts;
		ts.Duration = 1500000LL;
		anim->Duration = DurationHelper::FromTimeSpan(ts);
		auto sb = ref new Storyboard();
		sb->Children->Append(anim);
		Storyboard::SetTarget(anim, PollingIndicator);
		Storyboard::SetTargetProperty(anim, ref new Platform::String(L"(UIElement.Opacity)"));
		sb->Begin();
	} catch (...) {
	}
}

void AppPage::FadeOutPollingIndicator() {
	try {
		using namespace Windows::UI::Xaml::Media::Animation;
		auto anim = ref new DoubleAnimation();
		anim->To = ref new Platform::Box<double>(0.0);
		TimeSpan ts;
		ts.Duration = 1500000LL;
		anim->Duration = DurationHelper::FromTimeSpan(ts);
		auto sb = ref new Storyboard();
		sb->Children->Append(anim);
		Storyboard::SetTarget(anim, PollingIndicator);
		Storyboard::SetTargetProperty(anim, ref new Platform::String(L"(UIElement.Opacity)"));
		Platform::WeakReference weakThis(this);
		sb->Completed += ref new EventHandler<Platform::Object ^>([weakThis](Platform::Object ^, Platform::Object ^) {
			auto that = weakThis.Resolve<AppPage>();
			if (that) try {
					that->PollingIndicator->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				} catch (...) {
				}
		});
		sb->Begin();
	} catch (...) {
	}
}

void AppPage::RevealActiveAppsGrid() {
	if (m_appsGridRevealed) return;
	m_appsGridRevealed = true;
	try {
		using namespace Windows::UI::Xaml::Media::Animation;
		if (this->ClosingOverlay == nullptr) return;
		auto anim = ref new DoubleAnimation();
		anim->To = ref new Platform::Box<double>(0.0);
		TimeSpan ts;
		ts.Duration = 1500000LL;
		anim->Duration = DurationHelper::FromTimeSpan(ts);
		auto sb = ref new Storyboard();
		sb->Children->Append(anim);
		Storyboard::SetTarget(anim, this->ClosingOverlay);
		Storyboard::SetTargetProperty(anim, ref new Platform::String(L"(UIElement.Opacity)"));
		Platform::WeakReference weakThis(this);
		sb->Completed += ref new EventHandler<Platform::Object ^>([weakThis](Platform::Object ^, Platform::Object ^) {
			auto that = weakThis.Resolve<AppPage>();
			if (that == nullptr) return;
			try {
				that->ClosingOverlay->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
				that->ClosingOverlay->Opacity = 1.0;
			} catch (...) {
				MLOG(Utils::LogLevel::Warning, "RevealActiveAppsGrid: failed to reset ClosingOverlay after reveal, m_appsGridRevealed already set so this will not retry");
			}
		});
		sb->Begin();
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "RevealActiveAppsGrid: reveal animation failed to start, m_appsGridRevealed already set so this will not retry");
	}
}

void AppPage::StartBgPanAnimation() {
	using namespace Windows::UI::Xaml::Media;
	using namespace Windows::UI::Xaml::Media::Animation;

	try {
		if (PageBackgroundImage == nullptr) return;

		auto transform = ref new TranslateTransform();
		PageBackgroundImage->RenderTransform = transform;

		auto sb = ref new Storyboard();
		sb->RepeatBehavior = RepeatBehaviorHelper::Forever;
		sb->AutoReverse = true;

		Windows::UI::Xaml::Duration dur(TimeSpan{kBgPanDurationSec * 10000000LL});

		auto ease = ref new SineEase();
		ease->EasingMode = EasingMode::EaseInOut;

		auto animY = ref new DoubleAnimation();
		animY->To = ref new Platform::Box<double>(120.0);
		animY->Duration = dur;
		animY->EasingFunction = ease;
		Storyboard::SetTarget(animY, transform);
		Storyboard::SetTargetProperty(animY, ref new Platform::String(L"Y"));
		sb->Children->Append(animY);

		sb->Begin();
		m_bgPanStoryboard = sb;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "StartBgPanAnimation failed, background pan animation will not run\n");
	}
}

void AppPage::StartBannerSlideInAnimation() {
	using namespace Windows::UI::Xaml::Media;
	using namespace Windows::UI::Xaml::Media::Animation;

	try {
		if (ComputerNameBannerContainer == nullptr) return;

		auto transform = ref new TranslateTransform();
		ComputerNameBannerContainer->RenderTransform = transform;

		auto ease = ref new CubicEase();
		ease->EasingMode = EasingMode::EaseInOut;

		auto anim = ref new DoubleAnimation();
		anim->From = ref new Platform::Box<double>(-200.0);
		anim->To = ref new Platform::Box<double>(0.0);
		TimeSpan ts;
		ts.Duration = 15000000LL;
		anim->Duration = DurationHelper::FromTimeSpan(ts);
		anim->EasingFunction = ease;

		auto sb = ref new Storyboard();
		sb->Children->Append(anim);
		Storyboard::SetTarget(anim, transform);
		Storyboard::SetTargetProperty(anim, ref new Platform::String(L"X"));
		sb->Begin();
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "StartBannerSlideInAnimation failed, banner slide-in animation will not run\n");
	}
}

}
