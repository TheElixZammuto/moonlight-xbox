#include "pch.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include "AppPage.xaml.h"
#include "UI\Utilities\ImageHelpers.h"
#define MLOG_TAG_OVERRIDE "AppPage"
#include "Utils.hpp"

using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Hosting;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace concurrency;

namespace moonlight_xbox_dx {

namespace {

static Platform::String ^ BlurCachePath(Platform::String ^ imagePath, bool isGlow) {
	if (imagePath == nullptr) return nullptr;
	const wchar_t *s = imagePath->Data();
	int len = (int)wcslen(s);
	int lastSlash = -1, lastDot = -1;
	for (int i = len - 1; i >= 0; i--) {
		if (s[i] == L'\\' && lastSlash < 0) lastSlash = i;
		if (s[i] == L'.' && lastDot < 0) lastDot = i;
	}
	if (lastSlash < 0 || lastDot <= lastSlash) return nullptr;
	auto dir = ref new Platform::String(s, lastSlash + 1);
	auto base = ref new Platform::String(s + lastSlash + 1, lastDot - lastSlash - 1);
	auto blurDir = Platform::String::Concat(dir, ref new Platform::String(L"blur\\"));
	auto suffix = ref new Platform::String(isGlow ? L"_glow.png" : L"_bg.png");
	return Platform::String::Concat(blurDir, Platform::String::Concat(base, suffix));
}

static void SaveBlurStreamSync(IRandomAccessStream ^ stream, Platform::String ^ filePath) {
	if (stream == nullptr || filePath == nullptr) return;
	const wchar_t *s = filePath->Data();
	int len = (int)wcslen(s);
	int lastSlash = -1;
	for (int i = len - 1; i >= 0; i--) {
		if (s[i] == L'\\') {
			lastSlash = i;
			break;
		}
	}
	if (lastSlash < 0) return;
	auto dirPath = ref new Platform::String(s, lastSlash);
	auto fileName = ref new Platform::String(s + lastSlash + 1);
	CreateDirectory(dirPath->Data(), nullptr);
	auto folder = create_task(StorageFolder::GetFolderFromPathAsync(dirPath)).get();
	auto file = create_task(folder->CreateFileAsync(fileName,
	                                                CreationCollisionOption::ReplaceExisting))
	                .get();
	auto fs = create_task(file->OpenAsync(FileAccessMode::ReadWrite)).get();
	create_task(RandomAccessStream::CopyAsync(
	                stream->GetInputStreamAt(0), fs->GetOutputStreamAt(0)))
	    .get();
	create_task(fs->FlushAsync()).get();
}

static concurrency::task<IRandomAccessStream ^> OpenBlurCacheStreamAsync(Platform::String ^ filePath) {
	if (filePath == nullptr || GetFileAttributes(filePath->Data()) == INVALID_FILE_ATTRIBUTES)
		return task_from_result<IRandomAccessStream ^>(nullptr);
	return create_task(StorageFile::GetFileFromPathAsync(filePath))
	    .then([](StorageFile ^ file) -> concurrency::task<IRandomAccessStream ^> {
		    if (file == nullptr) return task_from_result<IRandomAccessStream ^>(nullptr);
		    return create_task(file->OpenReadAsync())
		        .then([](IRandomAccessStream ^ s) -> IRandomAccessStream ^ { return s; });
	    });
}

}

void AppPage::AppsGrid_SelectionChanged(Platform::Object ^ sender, SelectionChangedEventArgs ^ e) {

	if (m_suppressSelectionVisuals) return;

	auto lv = dynamic_cast<ListView ^>(sender);
	if (lv == nullptr || lv->SelectedIndex < 0) return;

	auto app = dynamic_cast<MoonlightApp ^>(lv->SelectedItem);
	if (app == nullptr) return;
	if (app == nullptr) return;

	ApplySelectionVisuals(app, true);
}

void AppPage::ApplySelectionVisuals(MoonlightApp ^ app, bool animate, bool centerImmediate) {

	if (app == nullptr) return;

	MoonlightApp ^ prev = m_selectedApp;
	if (prev != nullptr && prev->Id != app->Id) {
		try {
			prev->IsSelected = false;
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "ApplySelectionVisuals: prev->IsSelected=false failed for app id=%d, stale selected visual may persist\n", prev->Id);
		}
		UpdateContainerSelectionState(prev, false, animate);
	}

	m_selectedApp = app;
	try {
		app->IsSelected = true;
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "ApplySelectionVisuals: app->IsSelected=true failed for app id=%d, selection visual desynced\n", app->Id);
	}
	UpdateContainerSelectionState(app, true, animate);

	if (app->BlurredImage == nullptr)
		BlurAppImage(app);
	else
		FadeInBlurIfSelected(app, app->BlurredImage);

	UpdateSelectedAppBox(app, prev, animate);

	if (centerImmediate) {
		CenterSelectedItem(true);
		return;
	}

	CenterSelectedItem(false);

	try {
		if (m_centerDebounceTimer == nullptr) {
			m_centerDebounceTimer = ref new Windows::UI::Xaml::DispatcherTimer();
			m_centerDebounceTimer->Interval = Windows::Foundation::TimeSpan{800000LL};
			Platform::WeakReference weakThis(this);
			m_centerDebounceTimer_token = m_centerDebounceTimer->Tick +=
			    ref new Windows::Foundation::EventHandler<Platform::Object ^>([weakThis](Platform::Object ^, Platform::Object ^) {
				    auto that = weakThis.Resolve<AppPage>();
				    if (that == nullptr) return;
				    try {
					    that->m_centerDebounceTimer->Stop();
				    } catch (...) {
				    }
				    try {
					    that->CenterSelectedItem(false);
				    } catch (...) {
				    }
			    });
		}
		m_centerDebounceTimer->Stop();
		m_centerDebounceTimer->Start();
	} catch (...) {
		MLOG(Utils::LogLevel::Warning, "ApplySelectionVisuals failed to set up center debounce timer, debounced re-centering permanently disabled");
	}
}

void AppPage::UpdateContainerSelectionState(MoonlightApp ^ app, bool isSelected, bool animate) {
	if (app == nullptr) return;
	Windows::UI::Xaml::Controls::ListView ^ grids[] = {this->AppsListView, this->AppsGridView};
	for (auto grid : grids) {
		if (grid == nullptr) continue;
		try {
			auto container = dynamic_cast<Windows::UI::Xaml::Controls::ListViewItem ^>(grid->ContainerFromItem(app));
			if (container == nullptr) continue;
			VisualStateManager::GoToState(container, isSelected ? "Selected" : "Unselected", animate);
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "UpdateContainerSelectionState: GoToState failed for app id=%d, isSelected=%d\n", app->Id, isSelected);
		}
	}
}

void AppPage::UpdateSelectedAppBox(MoonlightApp ^ app, MoonlightApp ^ prev, bool animate) {
	try {
		if (this->SelectedAppText == nullptr || this->SelectedAppBox == nullptr) return;
		Platform::String ^ newText = app->Name != nullptr ? app->Name : ref new Platform::String(L"");
		this->SelectedAppBox->Visibility = Windows::UI::Xaml::Visibility::Visible;
		this->SelectedAppText->Visibility = Windows::UI::Xaml::Visibility::Visible;
		this->SelectedAppText->Foreground = ref new SolidColorBrush(Windows::UI::Colors::White);

		Windows::UI::Xaml::Media::Animation::Storyboard ^ showSb = nullptr;
		Windows::UI::Xaml::Media::Animation::Storyboard ^ hideSb = nullptr;
		try {
			auto res = this->Resources;
			if (res != nullptr) {
				try {
					showSb = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard ^>(res->Lookup(ref new Platform::String(L"ShowSelectedAppStoryboard")));
				} catch (...) {
				}
				try {
					hideSb = dynamic_cast<Windows::UI::Xaml::Media::Animation::Storyboard ^>(res->Lookup(ref new Platform::String(L"HideSelectedAppStoryboard")));
				} catch (...) {
				}
			}
		} catch (...) {
		}

		const unsigned int animVer = ++m_appTextAnimVersion;
		auto weakThis = WeakReference(this);
		auto capturedText = newText;
		AnimateCrossfadeText(
		    this->SelectedAppBox, showSb, hideSb, animate && prev != nullptr,
		    [weakThis, capturedText]() {
			    auto that = weakThis.Resolve<AppPage>();
			    if (that != nullptr && that->SelectedAppText != nullptr)
				    that->SelectedAppText->Text = capturedText;
		    },
		    [weakThis, animVer]() {
			    auto that = weakThis.Resolve<AppPage>();
			    return that != nullptr && that->m_appTextAnimVersion == animVer;
		    });
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "ApplySelectionVisuals: text overlay update failed for app id=%d\n", app->Id);
	}
}

void AppPage::CenterSelectedItem(bool immediate) {

	auto lv = this->ActiveAppsGrid();
	ListViewItem ^ container = nullptr;
	auto status = ResolveSelectedContainer(lv, m_scrollViewer, container);
	if (status == SelectedContainerStatus::NoSelection) return;
	if (status == SelectedContainerStatus::ScrollViewerMissing) {
		WaitForScrollViewerThenCenter(immediate);
		return;
	}
	if (status == SelectedContainerStatus::ContainerMissing) {
		WaitForContainerThenCenter(immediate);
		return;
	}

	try {
		lv->UpdateLayout();
	} catch (...) {
	}
	if (container->ActualWidth <= 0.0 || container->ActualHeight <= 0.0) return;

	double viewport = m_isGridLayout ? m_scrollViewer->ViewportHeight : m_scrollViewer->ViewportWidth;
	if (viewport <= 0.0) {
		try {
			lv->UpdateLayout();
		} catch (...) {
		}
		viewport = m_isGridLayout ? m_scrollViewer->ViewportHeight : m_scrollViewer->ViewportWidth;
	}
	if (viewport <= 0.0) return;

	try {
		double desiredEdgePadding = m_isGridLayout
		                                ? 0.0
		                                : std::max(0.0, (viewport - container->ActualWidth) * 0.5);
		if (ApplyEdgeCenteringPadding(lv, desiredEdgePadding) == EdgeCenteringPaddingResult::Applied) {
			try {
				lv->UpdateLayout();
			} catch (...) {
			}
		}
	} catch (...) {
	}

	if (!m_initialFocusApplied) {
		m_initialFocusApplied = true;
		try {
			if (container != nullptr)
				container->Focus(Windows::UI::Xaml::FocusState::Programmatic);
			else
				lv->Focus(Windows::UI::Xaml::FocusState::Programmatic);
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "CenterSelectedItem: initial Focus() failed\n");
		}
	}

	CenterContainerInScrollViewer(m_scrollViewer, container, m_isGridLayout, immediate);

	RevealActiveAppsGrid();

	m_entranceAnimationsArmed = false;
}

void AppPage::WaitForContainerThenCenter(bool immediate) {
	auto lv = this->ActiveAppsGrid();
	if (lv == nullptr || m_centerWaitContainer_token.Value != 0) return;
	m_centerWaitContainerTarget = lv;
	Platform::WeakReference weakThis(this);
	m_centerWaitContainer_token = ArmContainerRealizedWait(lv, [weakThis, immediate]() {
		auto that = weakThis.Resolve<AppPage>();
		if (that == nullptr) return;
		that->m_centerWaitContainer_token.Value = 0;
		that->m_centerWaitContainerTarget = nullptr;
		that->CenterSelectedItem(immediate);
	});
	if (m_centerWaitContainer_token.Value == 0) m_centerWaitContainerTarget = nullptr;
}

void AppPage::WaitForScrollViewerThenCenter(bool immediate) {
	auto lv = this->ActiveAppsGrid();
	if (lv == nullptr || m_centerWaitScrollViewer_token.Value != 0) return;
	m_centerWaitScrollViewerTarget = lv;
	Platform::WeakReference weakThis(this);
	m_centerWaitScrollViewer_token = ArmLayoutUpdatedWait(lv, [weakThis, immediate]() {
		auto that = weakThis.Resolve<AppPage>();
		if (that == nullptr) return;
		that->m_centerWaitScrollViewer_token.Value = 0;
		that->m_centerWaitScrollViewerTarget = nullptr;
		that->CenterSelectedItem(immediate);
	});
}

void AppPage::UpdateItemHeights() {
	try {
		if (m_isGridLayout) return;
		if (this->AppsListView == nullptr) return;

		for (unsigned int i = 0; i < this->AppsListView->Items->Size; ++i) {
			auto container = dynamic_cast<ListViewItem ^>(this->AppsListView->ContainerFromIndex(i));
			if (container == nullptr) continue;

			std::function<DependencyObject ^ (DependencyObject ^)> find = [&](DependencyObject ^ parent) -> DependencyObject ^ {
				if (parent == nullptr) return nullptr;
				int count = VisualTreeHelper::GetChildrenCount(parent);
				for (int j = 0; j < count; ++j) {
					auto child = VisualTreeHelper::GetChild(parent, j);
					auto fe = dynamic_cast<FrameworkElement ^>(child);
					if (fe != nullptr && fe->GetType()->FullName == "moonlight_xbox_dx.AspectRatioBox") return child;
					auto rec = find(child);
					if (rec != nullptr) return rec;
				}
				return nullptr;
			};

			auto found = find(container);
			if (found == nullptr) continue;
			auto fe = dynamic_cast<FrameworkElement ^>(found);
			if (fe == nullptr) continue;
			if (std::isnan(fe->Height)) continue;

			fe->Height = std::nan("");
			fe->InvalidateMeasure();
			fe->UpdateLayout();
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "UpdateItemHeights: failed, stale Height may cause clipped artwork\n");
	}
}

void AppPage::ArmEntranceAnimations() {
	m_entranceAnimationsArmed = true;
	m_gridEntranceAnimatedIndices.clear();
	m_listEntranceAnimatedIndices.clear();
}

void AppPage::AppsGrid_ContainerContentChanging(
    Windows::UI::Xaml::Controls::ListViewBase ^ sender,
    Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs ^ args) {
	auto container = dynamic_cast<ListViewItem ^>(args->ItemContainer);
	if (container == nullptr) return;

	if (args->InRecycleQueue) {
		try {
			Windows::UI::Xaml::Thickness zero;
			zero.Left = zero.Top = zero.Right = zero.Bottom = 0.0;
			container->Margin = zero;
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "AppsGrid_ContainerContentChanging: Margin reset failed on recycle, stale margin may persist into reuse\n");
		}
		return;
	}

	auto &entranceAnimated = (sender == this->AppsGridView) ? m_gridEntranceAnimatedIndices : m_listEntranceAnimatedIndices;

	if (args->Phase == 0) {
		try {
			if (entranceAnimated.find(args->ItemIndex) == entranceAnimated.end()) {
				entranceAnimated.insert(args->ItemIndex);
				bool isCurrentSelection = sender->SelectedItem != nullptr && args->Item == sender->SelectedItem;
				if (m_entranceAnimationsArmed && !isCurrentSelection)
					PlayEntranceAnimation(container, args->ItemIndex);
			}
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "AppsGrid_ContainerContentChanging: entrance animation failed\n");
		}

		Platform::WeakReference weakThis(this);
		args->RegisterUpdateCallback(
		    ref new TypedEventHandler<ListViewBase ^, ContainerContentChangingEventArgs ^>(
		        [weakThis](ListViewBase ^ s, ContainerContentChangingEventArgs ^ a) {
			        auto that = weakThis.Resolve<AppPage>();
			        if (that == nullptr) return;
			        try {
				        that->AppsGrid_ContainerContentChanging(s, a);
			        } catch (...) {
				        MLOGF(Utils::LogLevel::Warning, "AppsGrid_ContainerContentChanging: Phase1 recursive dispatch failed\n");
			        }
		        }));
		return;
	}

	try {
		auto app = dynamic_cast<MoonlightApp ^>(args->Item);
		if (app != nullptr) {
			VisualStateManager::GoToState(container, app->IsSelected ? "Selected" : "Unselected", false);
			if (app->IsSelected && app->BlurredImage != nullptr)
				FadeInBlurIfSelected(app, app->BlurredImage);
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "AppsGrid_ContainerContentChanging: Phase1 blur recovery failed\n");
	}
}

void AppPage::PlayEntranceAnimation(Windows::UI::Xaml::Controls::ListViewItem ^ container, int index) {
	using namespace Windows::UI::Xaml::Media;
	using namespace Windows::UI::Xaml::Media::Animation;

	auto prevSb = dynamic_cast<Storyboard ^>(container->Tag);
	if (prevSb != nullptr) {
		try {
			prevSb->Stop();
		} catch (...) {
		}
	}

	auto scale = ref new ScaleTransform();
	scale->ScaleX = kEntranceStartScale;
	scale->ScaleY = kEntranceStartScale;
	container->RenderTransformOrigin = Windows::Foundation::Point(0.5f, 0.5f);
	container->RenderTransform = scale;
	container->Opacity = 0.0;

	int staggerIndex = index < kEntranceMaxStaggerItems ? index : kEntranceMaxStaggerItems;
	Windows::UI::Xaml::Duration animDuration(TimeSpan{(long long)kEntranceDurationMs * 10000LL});
	TimeSpan beginTime{(long long)staggerIndex * kEntranceStaggerMs * 10000LL};

	auto ease = ref new QuadraticEase();
	ease->EasingMode = EasingMode::EaseOut;

	auto sb = ref new Storyboard();

	auto opacityAnim = ref new DoubleAnimation();
	opacityAnim->To = 1.0;
	opacityAnim->Duration = animDuration;
	opacityAnim->BeginTime = beginTime;
	opacityAnim->EasingFunction = ease;
	Storyboard::SetTarget(opacityAnim, container);
	Storyboard::SetTargetProperty(opacityAnim, ref new Platform::String(L"Opacity"));
	sb->Children->Append(opacityAnim);

	auto scaleXAnim = ref new DoubleAnimation();
	scaleXAnim->To = 1.0;
	scaleXAnim->Duration = animDuration;
	scaleXAnim->BeginTime = beginTime;
	scaleXAnim->EasingFunction = ease;
	scaleXAnim->EnableDependentAnimation = true;
	Storyboard::SetTarget(scaleXAnim, container);
	Storyboard::SetTargetProperty(scaleXAnim, ref new Platform::String(L"(UIElement.RenderTransform).(ScaleTransform.ScaleX)"));
	sb->Children->Append(scaleXAnim);

	auto scaleYAnim = ref new DoubleAnimation();
	scaleYAnim->To = 1.0;
	scaleYAnim->Duration = animDuration;
	scaleYAnim->BeginTime = beginTime;
	scaleYAnim->EasingFunction = ease;
	scaleYAnim->EnableDependentAnimation = true;
	Storyboard::SetTarget(scaleYAnim, container);
	Storyboard::SetTargetProperty(scaleYAnim, ref new Platform::String(L"(UIElement.RenderTransform).(ScaleTransform.ScaleY)"));
	sb->Children->Append(scaleYAnim);

	container->Tag = sb;
	sb->Begin();
}

void AppPage::BlurAppImage(MoonlightApp ^ selApp) {
	try {
		if (selApp->BlurredImage != nullptr) return;
		if (m_blurInProgressIds.count(selApp->Id)) return;

		static Platform::String ^ placeholderImagePath = ref new Platform::String(L"ms-appx:///Assets/gamepad.svg");
		if (selApp->ImagePath == nullptr || selApp->ImagePath->Equals(placeholderImagePath)) {
			Platform::WeakReference weakThis(this);
			Platform::WeakReference weakApp(selApp);
			auto token = std::make_shared<Windows::Foundation::EventRegistrationToken>();
			*token = selApp->PropertyChanged += ref new Windows::UI::Xaml::Data::PropertyChangedEventHandler(
			    [weakThis, weakApp, token](Platform::Object ^, Windows::UI::Xaml::Data::PropertyChangedEventArgs ^ e) {
				    if (e->PropertyName != "ImagePath") return;
				    auto app = weakApp.Resolve<MoonlightApp>();
				    if (app == nullptr) return;
				    try { app->PropertyChanged -= *token; } catch (...) { }
				    auto that = weakThis.Resolve<AppPage>();
				    if (that == nullptr) return;
				    try { that->BlurAppImage(app); } catch (...) { }
			    });
			return;
		}

		m_blurInProgressIds.insert(selApp->Id);
		bool isGrid = this->m_isGridLayout;
		Platform::WeakReference weakThis(this);

		float glowBlurAmount = 16.0f;
		try {
			auto boxed = Resources->Lookup("BlurAmount");
			auto pv = dynamic_cast<Windows::Foundation::IPropertyValue ^>(boxed);
			if (pv != nullptr) glowBlurAmount = (float)pv->GetDouble();
		} catch (...) {
		}

		auto getOrComputeStream = [this, selApp](float blurDip, float padDip,
		                                         Platform::String ^ cachePath)
		    -> concurrency::task<IRandomAccessStream ^> {
			if (cachePath != nullptr && GetFileAttributes(cachePath->Data()) != INVALID_FILE_ATTRIBUTES)
				return OpenBlurCacheStreamAsync(cachePath);
			return ApplyBlur(selApp, blurDip, padDip)
			    .then([cachePath](IRandomAccessStream ^ stream) -> IRandomAccessStream ^ {
				    if (stream != nullptr && cachePath != nullptr) {
					    try {
						    SaveBlurStreamSync(stream, cachePath);
					    } catch (...) {
						    MLOGF(Utils::LogLevel::Warning, "BlurAppImage: cache write failed, blur will be recomputed every launch\n");
					    }
					    stream->Seek(0);
				    }
				    return stream;
			    },
			          concurrency::task_continuation_context::use_arbitrary());
		};

		auto bgCachePath = BlurCachePath(selApp->ImagePath, false);
		try {
			getOrComputeStream(kBlurAmountBackground, 0.0f, bgCachePath)
			    .then([selApp, weakThis](IRandomAccessStream ^ stream) {
				    try {
					    auto that = weakThis.Resolve<AppPage>();
					    if (that == nullptr) return;
					    that->HandleBlurStreamReady(selApp, weakThis, stream, true);
				    } catch (...) {
					    MLOGF(Utils::LogLevel::Warning, "BlurAppImage: background RunAsync dispatch failed for app id=%d, m_blurInProgressIds entry stuck\n", selApp->Id);
				    }
			    },
			          concurrency::task_continuation_context::use_current());
		} catch (...) {
			MLOGF(Utils::LogLevel::Warning, "BlurAppImage: background getOrComputeStream setup failed for app id=%d, m_blurInProgressIds entry stuck\n", selApp->Id);
		}

		if (!isGrid) {
			auto glowCachePath = BlurCachePath(selApp->ImagePath, true);
			try {
				getOrComputeStream(glowBlurAmount, kBlurGlowPaddingDip, glowCachePath)
				    .then([selApp, weakThis](IRandomAccessStream ^ stream) {
					    try {
						    auto that = weakThis.Resolve<AppPage>();
						    if (that == nullptr) return;
						    that->HandleBlurStreamReady(selApp, weakThis, stream, false);
					    } catch (...) {
						    MLOGF(Utils::LogLevel::Warning, "BlurAppImage: glow RunAsync dispatch failed for app id=%d, GlowImage permanently null\n", selApp->Id);
					    }
				    },
				          concurrency::task_continuation_context::use_current());
			} catch (...) {
				MLOGF(Utils::LogLevel::Warning, "BlurAppImage: glow getOrComputeStream setup failed for app id=%d, GlowImage permanently null\n", selApp->Id);
			}
		}
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "BlurAppImage: unhandled failure for app id=%d, m_blurInProgressIds entry may be stuck\n", selApp->Id);
	}
}

concurrency::task<IRandomAccessStream ^> AppPage::ApplyBlur(MoonlightApp ^ app, float blurDip, float padDip) {
	if (app == nullptr) return concurrency::task_from_result<IRandomAccessStream ^>(nullptr);

	Platform::String ^ path = app->ImagePath;

	return concurrency::create_task(ImageHelpers::LoadSoftwareBitmapFromUriOrPathAsync(path))
	    .then([this, app, blurDip, padDip](SoftwareBitmap ^ softwareBitmap) -> concurrency::task<IRandomAccessStream ^> {
		    if (softwareBitmap == nullptr) return concurrency::task_from_result<IRandomAccessStream ^>(nullptr);

		    unsigned int ui_targetW = 0, ui_targetH = 0;
		    double ui_dpi = 96.0;
		    try {
			    auto activeGrid = this->ActiveAppsGrid();
			    FrameworkElement ^ fe_for_size = dynamic_cast<FrameworkElement ^>(this->FindName("AppImageRect"));
			    if (fe_for_size == nullptr && activeGrid != nullptr) {
				    auto container = dynamic_cast<ListViewItem ^>(activeGrid->ContainerFromItem(app));
				    if (container != nullptr) {
					    auto found = FindChildByName(container, ref new Platform::String(L"AppImageRect"));
					    if (found == nullptr) found = FindChildByName(container, ref new Platform::String(L"AppImageBlurRect"));
					    fe_for_size = dynamic_cast<FrameworkElement ^>(found);
				    }
			    }

			    if (fe_for_size != nullptr) {
				    double aw = 0.0, ah = 0.0;
				    try {
					    aw = fe_for_size->ActualWidth;
				    } catch (...) {
				    }
				    try {
					    ah = fe_for_size->ActualHeight;
				    } catch (...) {
				    }

				    if ((aw <= 0 || ah <= 0) && activeGrid != nullptr) {
					    try {
						    auto container = dynamic_cast<ListViewItem ^>(activeGrid->ContainerFromItem(app));
						    if (container != nullptr) {
							    auto tryFE = [&](const wchar_t *name) {
								    auto fe = dynamic_cast<FrameworkElement ^>(FindChildByName(container, ref new Platform::String(name)));
								    if (fe != nullptr) {
									    try {
										    if (aw <= 0) aw = fe->ActualWidth;
									    } catch (...) {
									    }
									    try {
										    if (ah <= 0) ah = fe->ActualHeight;
									    } catch (...) {
									    }
								    }
							    };
							    tryFE(L"AppAspectRatioBox");
							    tryFE(L"ItemGrid");
							    tryFE(L"AppImageBlurRect");
							    if (aw <= 0) {
								    try {
									    aw = container->ActualWidth;
								    } catch (...) {
								    }
							    }
						    }
						    if (aw <= 0) {
							    try {
								    aw = activeGrid->ActualWidth;
							    } catch (...) {
							    }
						    }
					    } catch (...) {
					    }
				    }

				    if (aw > 0 && ah > 0) {
					    try {
						    auto di = DisplayInformation::GetForCurrentView();
						    double dpi = di != nullptr ? di->LogicalDpi : 96.0;
						    ui_dpi = dpi;
						    ui_targetW = (unsigned int)std::max(1u, (unsigned int)std::round(aw * dpi / 96.0));
						    ui_targetH = (unsigned int)std::max(1u, (unsigned int)std::round(ah * dpi / 96.0));
					    } catch (...) {
						    ui_targetW = ui_targetH = 0;
						    ui_dpi = 96.0;
					    }
				    }
			    }
		    } catch (...) {
			    ui_targetW = ui_targetH = 0;
		    }

		    unsigned int targetW = ui_targetW != 0 ? ui_targetW : softwareBitmap->PixelWidth;
		    unsigned int targetH = ui_targetH != 0 ? ui_targetH : softwareBitmap->PixelHeight;

		    unsigned int glowTargetW = targetW, glowTargetH = targetH;
		    if (padDip > 0.0f && targetW > 0 && targetH > 0) {
			    unsigned int padPx = (unsigned int)std::round((double)padDip * ui_dpi / 96.0);
			    if (padPx > 0) {
				    glowTargetW = targetW + 2 * padPx;
				    glowTargetH = targetH + 2 * padPx;
			    }
		    }

		    try {
			    ImageHelpers::AdjustSaturation(softwareBitmap, kBackgroundSaturation);
		    } catch (...) {
		    }
		    return ImageHelpers::CreateMaskedBlurredPngStreamAsync(
		        softwareBitmap, glowTargetW, glowTargetH, ui_dpi, blurDip);
	    });
}

void AppPage::HandleBlurStreamReady(MoonlightApp ^ selApp, Platform::WeakReference weakThis,
                                    IRandomAccessStream ^ stream, bool isBackground) {
	if (stream == nullptr) {
		if (!isBackground) return;
		MLOGF(Utils::LogLevel::Warning, "BlurAppImage: background stream null for app id=%d\n", selApp->Id);
		m_blurInProgressIds.erase(selApp->Id);
		return;
	}

	try {
		if (selApp == nullptr) return;
		auto img = ref new BitmapImage();
		create_task(img->SetSourceAsync(stream))
		    .then([weakThis, selApp, img, isBackground]() {
			    try {
				    auto thatCont = weakThis.Resolve<AppPage>();
				    if (thatCont == nullptr) return;
				    if (isBackground) {
					    thatCont->m_blurInProgressIds.erase(selApp->Id);
					    selApp->BlurredImage = img;
					    if (thatCont->m_selectedApp != nullptr &&
					        thatCont->m_selectedApp != selApp &&
					        thatCont->m_selectedApp->Id == selApp->Id)
						    thatCont->m_selectedApp->BlurredImage = img;
					    try {
						    thatCont->FadeInBlurIfSelected(selApp, img);
					    } catch (...) {
					    }
				    } else {
					    selApp->GlowImage = img;
					    if (thatCont->m_selectedApp != nullptr &&
					        thatCont->m_selectedApp != selApp &&
					        thatCont->m_selectedApp->Id == selApp->Id)
						    thatCont->m_selectedApp->GlowImage = img;
				    }
			    } catch (...) {
				    if (isBackground)
					    MLOGF(Utils::LogLevel::Warning, "BlurAppImage: assign-BlurredImage step failed for app id=%d, m_blurInProgressIds entry stuck\n", selApp->Id);
				    else
					    MLOGF(Utils::LogLevel::Warning, "BlurAppImage: assign-GlowImage step failed for app id=%d, GlowImage permanently null (BlurredImage gate blocks retry)\n", selApp->Id);
			    }
		    },
		          concurrency::task_continuation_context::use_current());
	} catch (...) {
		if (isBackground)
			MLOGF(Utils::LogLevel::Warning, "BlurAppImage: SetSourceAsync setup failed for app id=%d, m_blurInProgressIds entry stuck\n", selApp->Id);
		else
			MLOGF(Utils::LogLevel::Warning, "BlurAppImage: glow SetSourceAsync setup failed for app id=%d, GlowImage permanently null\n", selApp->Id);
	}
}

void AppPage::FadeInBlurIfSelected(MoonlightApp ^ app, BitmapImage ^ img) {
	if (app == nullptr || img == nullptr) return;

	bool isSelected = false;
	try {
		auto activeGrid = this->ActiveAppsGrid();
		if (activeGrid != nullptr) {
			auto selItem = dynamic_cast<MoonlightApp ^>(activeGrid->SelectedItem);
			if (selItem != nullptr && selItem->Id == app->Id) isSelected = true;
		}
		if (!isSelected && m_selectedApp != nullptr && m_selectedApp->Id == app->Id)
			isSelected = true;
	} catch (...) {
	}

	if (!isSelected) return;

	try {
		auto vm = this->ViewModel;
		if (vm != nullptr) vm->TransitionToBlurredImage(img);
	} catch (...) {
		MLOGF(Utils::LogLevel::Warning, "FadeInBlurIfSelected: TransitionToBlurredImage failed for app id=%d\n", app->Id);
	}
}

}
