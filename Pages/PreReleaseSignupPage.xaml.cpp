#include "pch.h"
#include "PreReleaseSignupPage.xaml.h"
#include <Utils.hpp>
#include <functional>

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Core;
using namespace Windows::Services::Store;
using namespace Windows::Foundation;
using namespace std;

static const wchar_t* g_prerelease_flight_group_id = L"your-group-id-here";

namespace {
    bool IsStoreRequestHelperAvailable()
    {
        try {
            return Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.Services.Store.StoreRequestHelper");
        }
        catch (...) {
            return false;
        }
    }

    bool TrySendStoreRequestAsync(Windows::Services::Store::StoreContext^ context, unsigned int requestKind, Platform::String^ paramsJson,
        Windows::UI::Core::CoreDispatcher^ dispatcher, function<void(Platform::String^)> onSuccess,
        function<void(Platform::Exception^)> onError)
    {
#if defined(USE_STORE_REQUEST_HELPER)
        try {
            auto op = Windows::Services::Store::StoreRequestHelper::SendRequestAsync(context, requestKind, paramsJson);
            op->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<Windows::Services::Store::StoreSendRequestResult^>(
                [dispatcher, onSuccess, onError](Windows::Foundation::IAsyncOperation<Windows::Services::Store::StoreSendRequestResult^>^ asyncOp, Windows::Foundation::AsyncStatus status)
            {
                try
                {
                    if (status == Windows::Foundation::AsyncStatus::Completed)
                    {
                        auto result = asyncOp->GetResults();

                        Platform::String^ response = nullptr;
                        try {
                            response = result->Response;
                        }
                        catch (...) {
                            response = nullptr;
                        }

                        if (response == nullptr || response->Length() == 0)
                        {
                            moonlight_xbox_dx::Utils::Log(std::string("StoreRequestHelper response empty or missing\n"));
                            if (onError)
                            {
                                dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([onError]() {
                                    onError(ref new Platform::Exception(E_FAIL, ref new Platform::String(L"Store request returned no response")));
                                }));
                            }
                        }
                        else
                        {
                            moonlight_xbox_dx::Utils::Log(std::string("StoreRequestHelper response: ") + moonlight_xbox_dx::Utils::PlatformStringToStdString(response) + "\n");
                            if (onSuccess)
                            {
                                dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal, ref new Windows::UI::Core::DispatchedHandler([onSuccess, response]() {
                                    onSuccess(response);
                                }));
                            }
                        }
                    }
                }
                catch (Platform::Exception^ ex)
                {
                    if (onError) onError(ex);
                }
            });
            return true;
        }
        catch (Platform::Exception^ ex)
        {
            if (onError) onError(ex);
            return false;
        }
#else
        return false;
#endif
    }
    
    static std::wstring EscapeForJsonString(const std::wstring& s)
    {
        std::wstring r;
        r.reserve(s.size() * 2);
        for (wchar_t c : s)
        {
            if (c == L'"') r += L"\\\"";
            else if (c == L'\\') r += L"\\\\";
            else r += c;
        }
        return r;
    }
}

PreReleaseSignupPage::PreReleaseSignupPage()
{
    InitializeComponent();
    this->Loaded += ref new RoutedEventHandler(this, &PreReleaseSignupPage::OnLoaded);
    this->Unloaded += ref new RoutedEventHandler(this, &PreReleaseSignupPage::OnUnloaded);
}

void PreReleaseSignupPage::OnLoaded(Platform::Object^ sender, RoutedEventArgs^ e)
{
    auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    m_back_token = navigation->BackRequested += ref new EventHandler<Windows::UI::Core::BackRequestedEventArgs^>(this, &PreReleaseSignupPage::backButton_Click);

    try
    {
        bool hasStoreHelper = IsStoreRequestHelperAvailable();
        moonlight_xbox_dx::Utils::Log(std::string("PreRelease OnLoaded: ApiInformation::IsTypePresent(StoreRequestHelper) = ") + (hasStoreHelper ? "true\n" : "false\n"));
#if defined(USE_STORE_REQUEST_HELPER)
        moonlight_xbox_dx::Utils::Log("PreRelease compiled with USE_STORE_REQUEST_HELPER\n");
#else
        moonlight_xbox_dx::Utils::Log("PreRelease compiled WITHOUT USE_STORE_REQUEST_HELPER\n");
#endif
        if (hasStoreHelper)
        {
            this->StatusText->Text = "Checking prerelease enrollment status...";
            try
            {
                auto context = Windows::Services::Store::StoreContext::GetDefault();
                std::wstring inner = std::wstring(L"{ \"flightGroupId\": \"") + std::wstring(g_prerelease_flight_group_id) + std::wstring(L"\" }");
                std::wstring esc = EscapeForJsonString(inner);
                std::wstring full = std::wstring(L"{ \"type\": \"GetFlightMembershipStatus\", \"parameters\": \"") + esc + std::wstring(L"\" }");
                auto paramsJson = ref new Platform::String(full.c_str());

                bool started = TrySendStoreRequestAsync(context, 8u, paramsJson, this->Dispatcher,
                    [this](Platform::String^ response) {
                        std::string resp = moonlight_xbox_dx::Utils::PlatformStringToStdString(response);
                        std::string lowered = resp;
                        for (auto &c : lowered) c = (char)tolower(c);
                        bool member = false;
                        if (lowered.find("true") != std::string::npos) member = true;
                        if (lowered.find("member") != std::string::npos) member = true;
                        if (lowered.find("enroll") != std::string::npos) {
                            if (lowered.find("not") != std::string::npos) member = false;
                        }

                        {
                            this->m_isMember = member;
                            if (member)
                            {
                                this->StatusText->Text = "You are enrolled in the prerelease.";
                                this->OptInButton->Content = "Leave Prerelease";
                            }
                            else
                            {
                                this->StatusText->Text = "You are not enrolled. Click to join.";
                                this->OptInButton->Content = "Join Prerelease";
                            }
                            this->OptInButton->IsEnabled = true;
                        }
                    },
                    [this](Platform::Exception^ ex) {
                        auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
                        auto hr = ex->HResult;
                        moonlight_xbox_dx::Utils::Log(std::string("StoreRequestHelper membership query error: HR=0x") + Utils::WideToNarrowString(std::to_wstring((long long)hr)) + " msg=" + msg + "\n");
                        {
                            std::string s = std::string("Error checking prerelease status: ") + msg;
                            this->StatusText->Text = moonlight_xbox_dx::Utils::StringFromStdString(s);
                        }
                        this->OptInButton->IsEnabled = false;
                    });

                if (!started)
                {
                    this->StatusText->Text = "Unable to start Store membership query.";
                    this->OptInButton->IsEnabled = false;
                }
            }
            catch (Platform::Exception^ ex)
            {
                auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
                moonlight_xbox_dx::Utils::Log(std::string("PreRelease membership check exception: ") + msg + "\n");
                this->StatusText->Text = "Failed to verify prerelease status.";
                this->OptInButton->IsEnabled = false;
            }
        }
        else
        {
            this->OptInButton->IsEnabled = false;
            this->StatusText->Text = "Prerelease signing not available on this platform.";
        }
    }
    catch (Platform::Exception ^ex)
    {
        auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
        auto hr = ex->HResult;
        moonlight_xbox_dx::Utils::Log(std::string("PreRelease OnLoaded exception: HR=0x") + Utils::WideToNarrowString(std::to_wstring((long long)hr)) + " msg=" + msg + "\n");
        this->OptInButton->IsEnabled = false;
        this->StatusText->Text = "Error checking prerelease availability.";
    }
}

void PreReleaseSignupPage::OnUnloaded(Platform::Object^ sender, RoutedEventArgs^ e)
{
    auto navigation = Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    navigation->BackRequested -= m_back_token;
}

void PreReleaseSignupPage::backButton_Click(Platform::Object^ sender, Windows::UI::Core::BackRequestedEventArgs^ e)
{
    if (this->Frame->CanGoBack) this->Frame->GoBack();
}

void PreReleaseSignupPage::OptInButton_Click(Platform::Object^ sender, RoutedEventArgs^ e)
{
    moonlight_xbox_dx::Utils::Log("PreRelease OptInButton_Click invoked\n");
    try
    {
        bool hasStoreHelper = IsStoreRequestHelperAvailable();
        moonlight_xbox_dx::Utils::Log(std::string("OptIn: ApiInformation reports StoreRequestHelper = ") + (hasStoreHelper ? "true\n" : "false\n"));
        if (hasStoreHelper)
        {
            auto context = Windows::Services::Store::StoreContext::GetDefault();
                std::wstring inner = std::wstring(L"{ \"flightGroupId\": \"") + std::wstring(g_prerelease_flight_group_id) + std::wstring(L"\" }");
                std::wstring esc = EscapeForJsonString(inner);
                std::wstring full = std::wstring(L"{ \"type\": \"AddToFlightGroup\", \"parameters\": \"") + esc + std::wstring(L"\" }");
            auto paramsJson = ref new Platform::String(full.c_str());

            if (this->m_isMember)
            {
                std::wstring inner = std::wstring(L"{ \"flightGroupId\": \"") + std::wstring(g_prerelease_flight_group_id) + std::wstring(L"\" }");
                std::wstring esc = EscapeForJsonString(inner);
                std::wstring full = std::wstring(L"{ \"type\": \"RemoveFromFlightGroup\", \"parameters\": \"") + esc + std::wstring(L"\" }");
                auto paramsJson = ref new Platform::String(full.c_str());
                bool started = TrySendStoreRequestAsync(context, 8u, paramsJson, this->Dispatcher,
                    [this](Platform::String^ response) {
                        this->StatusText->Text = response;
                        this->m_isMember = false;
                        this->OptInButton->Content = "Join Prerelease";
                    },
                    [this](Platform::Exception^ ex) {
                        auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
                        auto hr = ex->HResult;
                        moonlight_xbox_dx::Utils::Log(std::string("StoreRequestHelper leave callback error: HR=0x") + Utils::WideToNarrowString((std::wstring)L"" + std::to_wstring(hr)) + " msg=" + msg + "\n");
                        {
                            std::string s = std::string("Failed to leave prerelease: ") + msg;
                            this->StatusText->Text = moonlight_xbox_dx::Utils::StringFromStdString(s);
                        }
                    });
                if (!started)
                {
                    this->StatusText->Text = "Failed to start leave request.";
                }
            }
            else
            {
                std::wstring inner = std::wstring(L"{ \"flightGroupId\": \"") + std::wstring(g_prerelease_flight_group_id) + std::wstring(L"\" }");
                std::wstring esc = EscapeForJsonString(inner);
                std::wstring full = std::wstring(L"{ \"type\": \"AddToFlightGroup\", \"parameters\": \"") + esc + std::wstring(L"\" }");
                auto paramsJson = ref new Platform::String(full.c_str());
                bool started = TrySendStoreRequestAsync(context, 8u, paramsJson, this->Dispatcher,
                    [this](Platform::String^ response) {
                        this->StatusText->Text = response;
                        this->m_isMember = true;
                        this->OptInButton->Content = "Leave Prerelease";
                    },
                    [this](Platform::Exception^ ex) {
                        auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
                        auto hr = ex->HResult;
                        moonlight_xbox_dx::Utils::Log(std::string("StoreRequestHelper join callback error: HR=0x") + Utils::WideToNarrowString((std::wstring)L"" + std::to_wstring(hr)) + " msg=" + msg + "\n");
                        {
                            std::string s = std::string("Failed to join prerelease: ") + msg;
                            this->StatusText->Text = moonlight_xbox_dx::Utils::StringFromStdString(s);
                        }
                    });
                if (!started)
                {
                    this->StatusText->Text = "Failed to start join request.";
                }
            }
        }
        else
        {
            this->StatusText->Text = "Prerelease signing not available on this platform.";
            this->OptInButton->IsEnabled = false;
        }
    }
    catch (Platform::Exception ^ex)
    {
        auto msg = moonlight_xbox_dx::Utils::PlatformStringToStdString(ex->Message);
        auto hr = ex->HResult;
        moonlight_xbox_dx::Utils::Log(std::string("OptIn exception: HR=0x") + Utils::WideToNarrowString(std::to_wstring((long long)hr)) + " msg=" + msg + "\n");
        this->StatusText->Text = "Failed to start prerelease signup.";
    }
}
