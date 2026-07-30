#include "pch.h"
#include "HostSelectorPage.xaml.h"
#include "UI\Backgrounds\DynamicBackgroundHost.xaml.h"
#include "UI\Controls\LunarPhaseControl.xaml.h"
#include "UI\Pages\AppPage\AppPage.xaml.h"
#include <State\MoonlightClient.h>
#include "UI\Pages\HostSettingsPage.xaml.h"
#define MLOG_TAG_OVERRIDE "HostSelectorPage"
#include "Utils.hpp"
#include "UI\Utilities\XamlHelpers.h"
#include "UI\Utilities\ToastService.h"
#include "UI\Pages\MoonlightSettings.xaml.h"
#include "State\MDNSHandler.h"
#include "UI\Pages\MoonlightWelcome.xaml.h"
#include "UI\Modals\AlertDialog.xaml.h"
#include "UI\Modals\PairDialog.xaml.h"
#include "UI\Modals\ConfirmDialog.xaml.h"
#include "UI\Modals\HostActionsDialog.xaml.h"
#include "UI\Modals\TestConnectionResultDialog.xaml.h"
#include <string>
#include <algorithm>
#include <cmath>
#include <ppl.h>
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
	m_hostsScrollViewer = nullptr;
}

void HostSelectorPage::OnStateLoaded() {
	if (GetApplicationState()->FirstTime) {
		this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightWelcome::typeid), nullptr, ref new Windows::UI::Xaml::Media::Animation::EntranceNavigationTransitionInfo());
		return;
	}

	{
		auto weakThis = WeakReference(this);
		try {
			this->Dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Low,
				ref new Windows::UI::Core::DispatchedHandler([weakThis]() {
					auto that = weakThis.Resolve<HostSelectorPage>();
					if (that != nullptr) that->UpdateAllMoonPhases(true, 4);
				}));
		} catch (...) {}
	}

	{
		auto weakThis = WeakReference(this);
		try {
			this->Dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Low,
				ref new Windows::UI::Core::DispatchedHandler([weakThis]() {
					auto that = weakThis.Resolve<HostSelectorPage>();
					if (that != nullptr) that->FocusFirstHostItem(4);
				}));
		} catch (...) {}
	}

	Concurrency::create_task([this]() {
		std::vector<MoonlightHost^> hostsSnapshot;
		for (auto a : GetApplicationState()->SavedHosts) {
			hostsSnapshot.push_back(a);
		}
		Concurrency::parallel_for_each(hostsSnapshot.begin(), hostsSnapshot.end(), [](MoonlightHost^ a) {
			try {
				a->UpdateHostInfo(true);
			} catch (...) {
				MLOG(Utils::LogLevel::Warning, "OnStateLoaded UpdateHostInfo failed for a host");
			}
		});
	}).then([this]() {
		bool willAutoConnect = false;
		if (GetApplicationState()->autostartInstance.size() > 0) {
			auto pii = Utils::StringFromStdString(GetApplicationState()->autostartInstance);
			for (unsigned int i = 0; i < GetApplicationState()->SavedHosts->Size; i++) {
				auto host = GetApplicationState()->SavedHosts->GetAt(i);
				if (host->InstanceId->Equals(pii)) {
					willAutoConnect = true;
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
		if (!willAutoConnect) {
			auto that = this;
			Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Normal,
				ref new Windows::UI::Core::DispatchedHandler([that]() {
					that->StartPollTimerIfNeeded();
				})
			);
		}
	}).then([this](concurrency::task<void> t) {
		try {
			t.get();
		}
		catch (const std::exception &e) {
			MLOGF(Utils::LogLevel::Error, "OnStateLoaded task exception: %s", e.what());
		}
		catch (...) {
			MLOG(Utils::LogLevel::Error, "OnStateLoaded task unknown exception");
		}
	});
}

void HostSelectorPage::OnKeyDown(Platform::Object ^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs ^ e) {
	if (e->Key == Windows::System::VirtualKey::Enter) {
		CoreInputView::GetForCurrentView()->TryHide();
	}
}

void HostSelectorPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) {
	Windows::UI::ViewManagement::ApplicationView::GetForCurrentView()->SetDesiredBoundsMode(Windows::UI::ViewManagement::ApplicationViewBoundsMode::UseCoreWindow);
	continueFetch.store(true);
	m_isNavigatedAway.store(false);

	try {
		if (BackgroundHost != nullptr) {
			BackgroundHost->SetHosts(State->SavedHosts);
			BackgroundHost->Refresh();
			BackgroundHost->StartAnimations();
		}
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "OnNavigatedTo failed to refresh background/animations");
	}

	if (GetApplicationState()->IsStateLoaded) {
		StartPollTimerIfNeeded();
	}
}

void HostSelectorPage::StartPollTimerIfNeeded()
{
	if (m_isNavigatedAway.load()) return;
	if (m_pollTimer != nullptr) return;

	using namespace Windows::System::Threading;
	using namespace Windows::Foundation;

	try {
		TimeSpan period;
		period.Duration = 5000 * 10000LL;
		Platform::WeakReference weakThis(this);
		auto callback = ref new TimerElapsedHandler([weakThis](ThreadPoolTimer^ timer) {
			auto that = weakThis.Resolve<HostSelectorPage>();
			if (that == nullptr) {
				try {
					if (timer != nullptr) timer->Cancel();
				} catch (...) {
					MLOG(Utils::LogLevel::Warning, "failed to cancel poll timer for destroyed page");
				}
				return;
			}
			that->PollTick(timer);
		});

		m_pollTimer = ThreadPoolTimer::CreatePeriodicTimer(callback, period);
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "failed to start poll timer");
	}
}

void HostSelectorPage::OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs ^ e) {
	try {
		if (BackgroundHost != nullptr) BackgroundHost->StopAnimations();
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "OnNavigatedFrom failed to stop background animations");
	}

	try {
		if (m_hostsGrid_ccc_token.Value != 0 && HostsGrid != nullptr) {
			HostsGrid->LayoutUpdated -= m_hostsGrid_ccc_token;
			m_hostsGrid_ccc_token.Value = 0;
		}
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "failed to unsubscribe HostsGrid.LayoutUpdated on navigate-away");
	}

	CancelContainerRealizedWait(HostsGrid, m_hostsContainerWait_token);
	CancelLayoutUpdatedWait(HostsGrid, m_hostsScrollViewerWait_token);

	m_isNavigatedAway.store(true);
	continueFetch.store(false);
	try {
		if (m_pollTimer != nullptr) {
			m_pollTimer->Cancel();
			m_pollTimer = nullptr;
		}
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "failed to cancel poll timer on navigate-away");
	}

	for (int i = 0; i < 50 && m_pollActiveCount.load() > 0; ++i) {
		Sleep(20);
	}

	try {
		__super::OnNavigatedFrom(e);
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "OnNavigatedFrom base class call failed");
	}
}

static const double kSelectedHostContainerWidth =    230;

bool HostSelectorPage::EnsureCenteringPadding()
{
	try {
		if (m_adjustingCenterPadding) return false;

		auto grid = this->HostsGrid;
		ListViewItem^ container = nullptr;
		auto status = ResolveSelectedContainer(grid, m_hostsScrollViewer, container);
		if (status != SelectedContainerStatus::Ready) return false;

		double viewport = m_hostsScrollViewer->ViewportWidth;
		double width = kSelectedHostContainerWidth;
		if (!std::isfinite(viewport) || viewport <= 0.0) return false;

		double desired = std::max(0.0, (viewport - width) * 0.5);
		if (!std::isfinite(desired)) return false;
		if (m_lastCenterPadding >= 0.0 && std::fabs(m_lastCenterPadding - desired) < 0.5) return false;

		m_adjustingCenterPadding = true;
		auto result = ApplyEdgeCenteringPadding(grid, desired);
		m_lastCenterPadding = desired;
		m_adjustingCenterPadding = false;
		bool applied = result == EdgeCenteringPaddingResult::Applied;
		if (applied) m_paddingDirty = true;
		return applied;
	} catch (...) {
		m_adjustingCenterPadding = false;
		return false;
	}
}

void HostSelectorPage::CenterSelectedHost(bool immediate)
{
	try {
		auto grid = this->HostsGrid;
		ListViewItem^ container = nullptr;
		auto status = ResolveSelectedContainer(grid, m_hostsScrollViewer, container);
		if (status == SelectedContainerStatus::NoSelection) return;
		if (status == SelectedContainerStatus::ScrollViewerMissing) {
			WaitForHostScrollViewerThenCenter(immediate);
			return;
		}
		if (status == SelectedContainerStatus::ContainerMissing) {
			WaitForHostContainerThenCenter(immediate);
			return;
		}

		EnsureCenteringPadding();
		if (m_paddingDirty) {
			m_paddingDirty = false;
			WaitForHostScrollViewerThenCenter(immediate);
			return;
		}

		try { grid->UpdateLayout(); } catch (...) {}
		if (container->ActualWidth <= 0.0 || container->ActualHeight <= 0.0 ||
			std::fabs(container->ActualWidth - kSelectedHostContainerWidth) > 1.0) {
			WaitForHostScrollViewerThenCenter(immediate);
			return;
		}

		double viewport = m_hostsScrollViewer->ViewportWidth;
		if (!std::isfinite(viewport) || viewport <= 0.0) {
			try { grid->UpdateLayout(); } catch (...) {}
			viewport = m_hostsScrollViewer->ViewportWidth;
		}
		if (!std::isfinite(viewport) || viewport <= 0.0) return;

		CenterContainerInScrollViewer(m_hostsScrollViewer, container, false, immediate);
	} catch (...) {}
}

void HostSelectorPage::WaitForHostContainerThenCenter(bool immediate)
{
	auto grid = this->HostsGrid;
	if (grid == nullptr || m_hostsContainerWait_token.Value != 0) return;
	Platform::WeakReference weakThis(this);
	m_hostsContainerWait_token = ArmContainerRealizedWait(grid, [weakThis, immediate]() {
		auto that = weakThis.Resolve<HostSelectorPage>();
		if (that == nullptr) return;
		that->m_hostsContainerWait_token.Value = 0;
		that->CenterSelectedHost(immediate);
	});
}

void HostSelectorPage::WaitForHostScrollViewerThenCenter(bool immediate)
{
	auto grid = this->HostsGrid;
	if (grid == nullptr || m_hostsScrollViewerWait_token.Value != 0) return;
	Platform::WeakReference weakThis(this);
	m_hostsScrollViewerWait_token = ArmLayoutUpdatedWait(grid, [weakThis, immediate]() {
		auto that = weakThis.Resolve<HostSelectorPage>();
		if (that == nullptr) return;
		that->m_hostsScrollViewerWait_token.Value = 0;
		that->CenterSelectedHost(immediate);
	});
}

void HostSelectorPage::PreRealizeAllHostContainers()
{
	if (m_hostsPreRealized) return;
	auto grid = this->HostsGrid;
	if (grid == nullptr || grid->Items == nullptr || grid->Items->Size == 0) return;

	m_hostsPreRealized = true;
	unsigned int count = grid->Items->Size;
	int selectedIndex = grid->SelectedIndex;
	try {
		for (unsigned int i = 0; i < count; ++i) {
			grid->ScrollIntoView(grid->Items->GetAt(i));
			grid->UpdateLayout();
		}
		if (selectedIndex >= 0) {
			grid->ScrollIntoView(grid->Items->GetAt(selectedIndex));
			grid->UpdateLayout();
		}
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "PreRealizeAllHostContainers failed");
	}
}

void HostSelectorPage::HostsGrid_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^)
{
	try {
		auto grid = dynamic_cast<ListViewBase^>(sender);
		if (grid == nullptr) return;
		if (m_hostsScrollViewer == nullptr) m_hostsScrollViewer = FindScrollViewer(grid);

		try {
			if (grid->Items != nullptr && grid->Items->Size > 0 && grid->SelectedIndex < 0) {
				grid->SelectedIndex = 0;
			}
			if (grid->Items != nullptr && grid->Items->Size > 0) {
				Platform::WeakReference weakThis(this);
				this->Dispatcher->RunAsync(
					Windows::UI::Core::CoreDispatcherPriority::Low,
					ref new Windows::UI::Core::DispatchedHandler([weakThis]() {
						auto that = weakThis.Resolve<HostSelectorPage>();
						if (that != nullptr) that->FocusFirstHostItem(4);
					}));
			}
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "HostsGrid_Loaded initial-selection/focus dispatch failed");
		}

		PreRealizeAllHostContainers();
		SubscribeMoonPhaseRefreshOnNextLayout(true);
		EnsureCenteringPadding();
		CenterSelectedHost(true);
		UpdateAllMoonPhases(false, 4);
	} catch (...) {
		MLOG(Utils::LogLevel::Error, "HostsGrid_Loaded setup failed");
	}
}

void HostSelectorPage::HostsGrid_SizeChanged(Platform::Object^, Windows::UI::Xaml::SizeChangedEventArgs^)
{
	if (m_adjustingCenterPadding) return;
	EnsureCenteringPadding();
	CenterSelectedHost(true);
}

void HostSelectorPage::FocusFirstHostItem(int attempts)
{
	try {
		auto grid = HostsGrid;
		if (grid == nullptr || grid->Items == nullptr || grid->Items->Size == 0) return;
		if (grid->SelectedIndex < 0) grid->SelectedIndex = 0;
		int idx = grid->SelectedIndex >= 0 ? grid->SelectedIndex : 0;
		auto container = dynamic_cast<Windows::UI::Xaml::Controls::Control^>(grid->ContainerFromIndex(idx));
		if (container != nullptr) {
			container->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			return;
		}
		if (attempts <= 0 || Dispatcher == nullptr) return;
		Platform::WeakReference weakThis(this);
		try {
			Dispatcher->RunAsync(
				Windows::UI::Core::CoreDispatcherPriority::Normal,
				ref new Windows::UI::Core::DispatchedHandler([weakThis, attempts]() {
					auto that = weakThis.Resolve<HostSelectorPage>();
					if (that != nullptr) that->FocusFirstHostItem(attempts - 1);
				}));
		} catch (...) {}
	} catch (...) {}
}

void HostSelectorPage::HostsGrid_SelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^)
{
	try {
		auto grid = dynamic_cast<ListViewBase^>(sender);
		if (grid == nullptr || grid->SelectedIndex < 0) return;
		CenterSelectedHost(false);
		UpdateAllMoonPhases(true);
	} catch (...) {}

	try {
		auto grid = dynamic_cast<ListViewBase^>(sender);
		auto selectedHost = (grid != nullptr && grid->SelectedItem != nullptr)
			? dynamic_cast<MoonlightHost^>(grid->SelectedItem)
			: nullptr;

		auto bgKey = (selectedHost != nullptr) ? selectedHost->Personalization->Background : nullptr;
		auto ls = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
		if (bgKey != nullptr && !bgKey->IsEmpty()) {
			ls->Insert("background", bgKey);
		} else {
			ls->Insert("background", DynamicBackgroundHost::DefaultKey);
		}

		if (BackgroundHost != nullptr) {
			auto singleHost = ref new Platform::Collections::Vector<MoonlightHost^>();
			if (selectedHost != nullptr) singleHost->Append(selectedHost);
			BackgroundHost->SetHosts(singleHost->Size > 0 ? (IVector<MoonlightHost^>^)singleHost : State->SavedHosts);
			BackgroundHost->Refresh();
			BackgroundHost->StartAnimations();
		}

		Windows::UI::Color accentColor = (selectedHost != nullptr && !selectedHost->Personalization->UseSystemAccent)
			? selectedHost->Personalization->AccentColor
			: (ref new Windows::UI::ViewManagement::UISettings())
				->GetColorValue(Windows::UI::ViewManagement::UIColorType::Accent);
		ApplyAccentColor(accentColor);
	} catch (...) {}

	try {
		auto grid = dynamic_cast<Windows::UI::Xaml::Controls::ListViewBase^>(sender);
		auto selectedHost = (grid != nullptr && grid->SelectedItem != nullptr)
			? dynamic_cast<MoonlightHost^>(grid->SelectedItem)
			: nullptr;

		if (this->SelectedHostBanner != nullptr && this->SelectedHostText != nullptr) {
			Platform::String^ newName = (selectedHost != nullptr && selectedHost->ComputerName != nullptr)
				? selectedHost->ComputerName
				: ref new Platform::String(L"");

			Windows::UI::Xaml::Media::Animation::Storyboard^ showSb = nullptr;
			Windows::UI::Xaml::Media::Animation::Storyboard^ hideSb = nullptr;
			auto res = this->Resources;
			if (res != nullptr) {
				try { showSb = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard^>(res->Lookup(ref new Platform::String(L"ShowSelectedHostStoryboard"))); } catch (...) {}
				try { hideSb = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard^>(res->Lookup(ref new Platform::String(L"HideSelectedHostStoryboard"))); } catch (...) {}
			}

			bool alreadyVisible = this->SelectedHostBanner->Opacity > 0.01;
			if (selectedHost == nullptr) {
				if (alreadyVisible && hideSb != nullptr) hideSb->Begin();
				this->SelectedHostText->Text = ref new Platform::String(L"");
			} else {
				const unsigned int animVer = ++m_hostTextAnimVersion;
				Platform::WeakReference weakThis(this);
				auto capturedName = newName;
				AnimateCrossfadeText(
					this->SelectedHostBanner, showSb, hideSb, true,
					[weakThis, capturedName]() {
						auto that = weakThis.Resolve<HostSelectorPage>();
						if (that != nullptr && that->SelectedHostText != nullptr)
							that->SelectedHostText->Text = capturedName;
					},
					[weakThis, animVer]() {
						auto that = weakThis.Resolve<HostSelectorPage>();
						return that != nullptr && that->m_hostTextAnimVersion == animVer;
					});
			}
		}
	} catch (...) {}
}

void HostSelectorPage::SubscribeMoonPhaseRefreshOnNextLayout(bool alsoFocusFirst)
{
	if (HostsGrid == nullptr || m_hostsGrid_ccc_token.Value != 0) return;
	Platform::WeakReference weakThis(this);
	m_hostsGrid_ccc_token = HostsGrid->LayoutUpdated +=
		ref new EventHandler<Platform::Object^>([weakThis, alsoFocusFirst](Platform::Object^, Platform::Object^) {
			auto that = weakThis.Resolve<HostSelectorPage>();
			if (that == nullptr) return;
			if (that->HostsGrid == nullptr ||
				that->HostsGrid->Items == nullptr ||
				that->HostsGrid->Items->Size == 0) return;

			try { that->HostsGrid->LayoutUpdated -= that->m_hostsGrid_ccc_token; }
			catch (...) { MLOG(Utils::LogLevel::Warning, "failed to unsubscribe HostsGrid.LayoutUpdated"); }
			that->m_hostsGrid_ccc_token.Value = 0;
			that->UpdateAllMoonPhases(false, 0);
			if (alsoFocusFirst) that->FocusFirstHostItem(4);
		});
}

void HostSelectorPage::UpdateAllMoonPhases(bool animated, int attempts)
{
	try {
		auto grid = HostsGrid;
		if (grid == nullptr) return;
		int selectedIdx = grid->SelectedIndex;
		if (selectedIdx < 0) selectedIdx = 0;
		unsigned int count = grid->Items->Size;
		bool anyMissing = false;
		for (unsigned int i = 0; i < count; ++i) {
			try {
				auto container = dynamic_cast<Windows::UI::Xaml::DependencyObject^>(
					grid->ContainerFromIndex(i));
				if (container == nullptr) { anyMissing = true; continue; }
				auto ctrl = FindLunarControl(container);
				if (ctrl == nullptr) { anyMissing = true; continue; }
				int dist = (int)i - selectedIdx;
				double fillAmount = dist < 0 ? -dist * 0.4 : dist * 0.4;
				if (fillAmount > 1.0) fillAmount = 1.0;
				int side = dist < 0 ? 1 : dist > 0 ? -1 : 0;
				ctrl->UpdatePhase(fillAmount, side, animated);
				ctrl->SetSelected(i == (unsigned int)selectedIdx, animated);
			} catch (...) {
				MLOGF(Utils::LogLevel::Warning, "UpdateAllMoonPhases failed for item %u", i);
			}
		}
		if (anyMissing && attempts > 0) {
			Platform::WeakReference weakThis(this);
			bool capturedAnimated = animated;
			int next = attempts - 1;
			try {
				this->Dispatcher->RunAsync(
					Windows::UI::Core::CoreDispatcherPriority::Normal,
					ref new Windows::UI::Core::DispatchedHandler([weakThis, capturedAnimated, next]() {
						auto that = weakThis.Resolve<HostSelectorPage>();
						if (that != nullptr) that->UpdateAllMoonPhases(capturedAnimated, next);
					}));
			} catch (...) {}
		}
	} catch (...) {}
}

LunarPhaseControl^ HostSelectorPage::FindLunarControl(Windows::UI::Xaml::DependencyObject^ root)
{
	if (root == nullptr) return nullptr;
	if (auto ctrl = dynamic_cast<LunarPhaseControl^>(root)) return ctrl;
	try {
		int count = VisualTreeHelper::GetChildrenCount(root);
		for (int i = 0; i < count; ++i) {
			auto found = FindLunarControl(VisualTreeHelper::GetChild(root, i));
			if (found != nullptr) return found;
		}
	} catch (...) {}
	return nullptr;
}

void HostSelectorPage::NewHostButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto dialog = ref new AddHostDialog();
	Platform::WeakReference weakThis(this);

	dialog->Configure(
		ref new Windows::UI::Xaml::RoutedEventHandler([weakThis, dialog](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
			auto that = weakThis.Resolve<HostSelectorPage>();
			if (that == nullptr) return;
			Platform::String^ hostname = dialog->GetHostname();
			if (hostname == nullptr || hostname->Length() == 0) return;
			dialog->SetAddButtonEnabled(false);
			Concurrency::create_task([that, dialog, hostname]() {
				bool status = that->state->AddHost(hostname);
				Platform::WeakReference weakPage(that);
				Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
					Windows::UI::Core::CoreDispatcherPriority::High,
					ref new Windows::UI::Core::DispatchedHandler([weakPage, dialog, status, hostname]() {
						if (!status) {
							dialog->ShowError("Failed to connect to " + hostname);
							dialog->SetAddButtonEnabled(true);
						} else {
							try { dialog->Hide(); }
							catch (...) { MLOG(Utils::LogLevel::Warning, "failed to hide AddHostDialog after successful add"); }
							auto page = weakPage.Resolve<HostSelectorPage>();
							if (page != nullptr) {
								try {
									if (page->HostsGrid != nullptr && page->HostsGrid->SelectedIndex < 0 &&
										page->HostsGrid->Items != nullptr && page->HostsGrid->Items->Size > 0) {
										page->HostsGrid->SelectedIndex = 0;
									}
								} catch (...) {
									MLOG(Utils::LogLevel::Warning, "failed to select newly added host");
								}
								try {
									page->SubscribeMoonPhaseRefreshOnNextLayout(false);
								} catch (...) {
									MLOG(Utils::LogLevel::Warning, "failed to subscribe HostsGrid.LayoutUpdated for new host");
								}
							}
						}
					}));
			});
		}),
		nullptr
	);

	dialog->SetRecentHostnames(state->RecentHostnames, state->RecentHostDisplayNames);
	try { dialog->XamlRoot = this->XamlRoot; } catch (...) {}
	concurrency::create_task(dialog->ShowAsync());
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

	this->ShowHostActions(host);
}

void HostSelectorPage::ShowHostActions(MoonlightHost^ host)
{
    currentHost = host;
    if (currentHost == nullptr) return;

    bool showWake = !(currentHost->Connected || currentHost->WolPolling);
    bool showTest = !showWake;

    auto dialog = ref new HostActionsDialog();
    m_hostActionsDialog = dialog;

    Platform::WeakReference weakThis(this);

    dialog->Configure(
        currentHost->ComputerName,
        showWake,
        showTest,
        ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
            auto that = weakThis.Resolve<HostSelectorPage>();
            if (that == nullptr || that->currentHost == nullptr) return;
            auto t = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
            t->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
            that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(HostSettingsPage::typeid), that->currentHost, t);
        }),
        ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
            auto that = weakThis.Resolve<HostSelectorPage>();
            if (that == nullptr || that->currentHost == nullptr) return;
            that->wakeHostButton_Click(nullptr, nullptr);
        }),
        ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
            auto that = weakThis.Resolve<HostSelectorPage>();
            if (that == nullptr || that->currentHost == nullptr) return;
            that->testConnectionButton_Click(nullptr, nullptr);
        }),
        ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
            auto that = weakThis.Resolve<HostSelectorPage>();
            if (that == nullptr || that->currentHost == nullptr) return;
            auto confirmDialog = ref new ConfirmDialog();
            Platform::String^ hostName = that->currentHost->ComputerName;
            Platform::String^ message = "Remove '" + hostName + "' from your saved hosts?";
            confirmDialog->Configure(
                "Remove Host",
		        message,
		        L"\xE74D",
                "Remove",
                "Cancel",
                ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
                    auto that = weakThis.Resolve<HostSelectorPage>();
                    if (that == nullptr || that->currentHost == nullptr) return;
                    int removedIdx = that->HostsGrid->SelectedIndex;
                    that->State->RemoveHost(that->currentHost);
                    that->currentHost = nullptr;
                    int newSize = (int)that->State->SavedHosts->Size;
                    if (newSize > 0) {
                        that->HostsGrid->SelectedIndex = removedIdx < newSize ? removedIdx : newSize - 1;
                    } else if (that->BackgroundHost != nullptr) {
                        that->BackgroundHost->ResetBackground();
                    }
                })
            );
            try { confirmDialog->XamlRoot = that->XamlRoot; } catch (...) {}
            concurrency::create_task(confirmDialog->ShowAsync());
        }),
        ref new Windows::UI::Xaml::RoutedEventHandler([weakThis](Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^) {
            auto that = weakThis.Resolve<HostSelectorPage>();
            if (that == nullptr) return;
            auto t = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
            t->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
            that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid), nullptr, t);
        })
    );

    try {
        dialog->XamlRoot = this->XamlRoot;
    } catch (...) {}

    concurrency::create_task(dialog->ShowAsync());
}

void HostSelectorPage::HostsGrid_RightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e)
{
	FrameworkElement^ senderElement = dynamic_cast<FrameworkElement^>(e->OriginalSource);
	if (senderElement == nullptr) return;

	auto itemContainer = dynamic_cast<SelectorItem^>(senderElement);
	MoonlightHost^ host = nullptr;
	if (itemContainer != nullptr) {
		host = dynamic_cast<MoonlightHost^>(itemContainer->Content);
	}
	else {
		host = dynamic_cast<MoonlightHost^>(senderElement->DataContext);
	}

	this->ShowHostActions(host);
}

void HostSelectorPage::SettingsButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto slideForward = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
	slideForward->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
	this->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(MoonlightSettings::typeid), nullptr, slideForward);
}

void HostSelectorPage::StartPairing(MoonlightHost^ host) {
	MoonlightClient* client = new MoonlightClient();
	char ipAddressStr[2048];
	wcstombs_s(NULL, ipAddressStr, host->LastHostname->Data(), 2047);
	int status = client->Connect(ipAddressStr);
	if (status != 0)return;
	char* pin = client->GeneratePIN();
	auto dialog = ref new ::moonlight_xbox_dx::PairDialog();
	wchar_t wpin[32];
	mbstowcs_s(NULL, wpin, pin, 31);
	dialog->Configure(ref new Platform::String(wpin));
	try { dialog->XamlRoot = this->XamlRoot; } catch (...) {}
	concurrency::create_task(dialog->ShowAsync());
	Concurrency::create_task([dialog, host, client, pin]() {
			int a = client->Pair();
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([a, dialog, host]()
			{
				if (a == 0) {
					try { dialog->Hide(); }
					catch (...) { MLOG(Utils::LogLevel::Warning, "failed to hide PairDialog after successful pairing"); }
				}
				host->UpdateHostInfo(true);
			}
			));
		}) .then([](concurrency::task<void> t) {
			try {
				t.get();
			}
			catch (const std::exception &e) {
				MLOGF(Utils::LogLevel::Error, "StartPairing task exception: %s", e.what());
			}
			catch (...) {
				MLOG(Utils::LogLevel::Error, "StartPairing task unknown exception");
			}
		});
}

static bool TryQuickTcpConnect(const std::string& hostOnly)
{
	bool reachable = false;
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
		struct addrinfo hints = {}, *res = nullptr;
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		if (getaddrinfo(hostOnly.c_str(), "47989", &hints, &res) == 0) {
			SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
			if (s != INVALID_SOCKET) {
				u_long mode = 1;
				ioctlsocket(s, FIONBIO, &mode);
				connect(s, res->ai_addr, (int)res->ai_addrlen);
				fd_set writeSet; FD_ZERO(&writeSet); FD_SET(s, &writeSet);
				timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
				reachable = (select(0, NULL, &writeSet, NULL, &tv) > 0 && FD_ISSET(s, &writeSet));
				closesocket(s);
			}
			freeaddrinfo(res);
		}
		WSACleanup();
	}
	return reachable;
}

void HostSelectorPage::Connect(MoonlightHost^ host) {
	if (!host->Connected)return;
	if (!host->Paired) {
		StartPairing(host);
		return;
	}
	state->shouldAutoConnect = true;
	continueFetch.store(false);

	Platform::WeakReference weakThis(this);
	std::string hostOnly = Utils::PlatformStringToStdString(host->LastHostname);
	auto colonPos = hostOnly.find(':');
	if (colonPos != std::string::npos) hostOnly = hostOnly.substr(0, colonPos);

	Concurrency::create_task([weakThis, host, hostOnly]() {
		bool reachable = TryQuickTcpConnect(hostOnly);

		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::High,
			ref new Windows::UI::Core::DispatchedHandler([weakThis, host, reachable]() {
				auto that = weakThis.Resolve<HostSelectorPage>();
				if (that == nullptr) return;
				if (!reachable) {
					that->continueFetch.store(true);
					auto dialog = ref new ::moonlight_xbox_dx::AlertDialog();
					dialog->Configure(L"Host Offline", L"The host is not reachable. It may have gone offline.");
					try { dialog->XamlRoot = that->XamlRoot; } catch (...) {}
					concurrency::create_task(dialog->ShowAsync());
					return;
				}
				auto slideForward = ref new Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionInfo();
				slideForward->Effect = Windows::UI::Xaml::Media::Animation::SlideNavigationTransitionEffect::FromRight;
				that->Frame->Navigate(Windows::UI::Xaml::Interop::TypeName(AppPage::typeid), host, slideForward);
			}));
	});
}

void HostSelectorPage::PollTick(Windows::System::Threading::ThreadPoolTimer^ timer)
{
	if (m_isNavigatedAway.load()) {
		try {
			if (timer != nullptr) timer->Cancel();
		} catch (...) {
			MLOG(Utils::LogLevel::Warning, "PollTick failed to cancel timer for destroyed page");
		}
		return;
	}

	m_pollActiveCount.fetch_add(1);
	if (continueFetch.load()) {
		try {
			try {
				mdns_send_query();
			} catch (...) {
			}
			query_mdns();
			std::vector<MoonlightHost^> hostsSnapshot;
			{
				auto savedHosts = GetApplicationState()->SavedHosts;
				for (unsigned int i = 0; i < savedHosts->Size; i++) {
					try {
						hostsSnapshot.push_back(savedHosts->GetAt(i));
					} catch (...) {
						break;
					}
				}
			}
			Concurrency::parallel_for_each(hostsSnapshot.begin(), hostsSnapshot.end(), [](MoonlightHost^ a) {
				try {
					a->UpdateHostInfo(true);
				} catch (...) {
				}
			});
		} catch (const std::exception &ex) {
			MLOGF(Utils::LogLevel::Error, "poll exception: %s", ex.what());
		} catch (...) {
			MLOG(Utils::LogLevel::Error, "poll unknown exception");
		}
	}
	m_pollActiveCount.fetch_sub(1);
}

void moonlight_xbox_dx::HostSelectorPage::wakeHostButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	if (currentHost == nullptr) {
		return;
	}

	try {
		bool success = State->WakeHost(currentHost);
		if (!success) {
			auto fail = ref new ::moonlight_xbox_dx::AlertDialog();
			fail->Configure("Wake Host Failed", "Failed to send Wake-on-LAN packet.\n\nPlease check if Wake-on-LAN is enabled on the host.");
			try { fail->XamlRoot = this->XamlRoot; } catch (...) {}
			concurrency::create_task(fail->ShowAsync());
		}

		if (success) {
			ShowToast(L"Wake on LAN sent");
			auto host = currentHost;
			host->WolPolling = true;
			PollHostAfterWake(host);
		}
	} catch (std::exception ex) {
		auto errDlg = ref new ::moonlight_xbox_dx::AlertDialog();
		errDlg->Configure("Wake Host Error", "An error occurred while trying to wake the host:\n\n" + Utils::StringFromChars((char *)ex.what()));
		try { errDlg->XamlRoot = this->XamlRoot; } catch (...) {}
		concurrency::create_task(errDlg->ShowAsync());
	}
}

void HostSelectorPage::PollHostAfterWake(MoonlightHost^ host)
{
	concurrency::create_task(concurrency::create_async([host]() {
		int consecutiveSuccess = 0;
		for (int i = 0; i < 240; ++i) {
			try {
				host->UpdateHostInfo(false);
				if (host->Connected) {
					consecutiveSuccess++;
					if (consecutiveSuccess >= 2) {
						host->WolPolling = false;
						break;
					}
				} else {
					consecutiveSuccess = 0;
				}
			} catch (...) {
				consecutiveSuccess = 0;
			}
			Sleep(250);
		}
	})).then([host](concurrency::task<void> t) {
		try {
			t.get();
		} catch (...) {
			MLOG(Utils::LogLevel::Error, "PollHostAfterWake task unknown exception");
		}
		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([host]() {
			host->WolPolling = false;
		}));
	});
}

static std::string ProbeHostConnection(const std::string& hostOnly)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return "WSAStartup failed";

	struct addrinfo hints;
	struct addrinfo *res = nullptr;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int gai = getaddrinfo(hostOnly.c_str(), "47989", &hints, &res);
	if (gai != 0) {
		WSACleanup();
		return std::string("DNS lookup failed: ") + std::to_string(gai);
	}

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
	WSACleanup();

	if (!ok) return "Connection failed";
	if (bestRttMs < 0) return "Connection OK";
	char buf[64];
	snprintf(buf, sizeof(buf), "Connection OK (RTT: %.1f ms)", bestRttMs);
	return buf;
}

void HostSelectorPage::testConnectionButton_Click(Platform::Object ^ sender, Windows::UI::Xaml::RoutedEventArgs ^ e) {
	if (currentHost == nullptr) return;

	std::string hostname = Utils::PlatformStringToStdString(currentHost->LastHostname);
	auto pos = hostname.find(':');
	std::string hostOnly = (pos == std::string::npos) ? hostname : hostname.substr(0, pos);

	Platform::WeakReference weakThis(this);
	concurrency::create_task([hostOnly, weakThis]() {
		std::string resultMsg = ProbeHostConnection(hostOnly);

		Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([resultMsg, hostOnly, weakThis]() {
			auto dialog = ref new TestConnectionResultDialog();
			dialog->Configure(
				Utils::StringFromStdString(hostOnly),
				Utils::StringFromStdString(resultMsg)
			);
			auto page = weakThis.Resolve<HostSelectorPage>();
			if (page != nullptr) {
				try { dialog->XamlRoot = page->XamlRoot; } catch (...) {}
			}
			concurrency::create_task(dialog->ShowAsync());
		}));
	});
}
