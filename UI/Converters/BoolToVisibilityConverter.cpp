#include "pch.h"
#include "UI\Converters\BoolToVisibilityConverter.h"

using namespace moonlight_xbox_dx;
using namespace Platform;
using namespace Windows::UI::Xaml;

Object^ BoolToVisibilityConverter::Convert(
    Object^ value,
    Windows::UI::Xaml::Interop::TypeName targetType,
    Object^ parameter,
    String^ language)
{

    bool flag = false;
    if (value != nullptr)
    {
        try
        {
            flag = safe_cast<bool>(value);
        }
        catch (...)
        {

            flag = false;
        }
    }

    return flag ? Visibility::Visible : Visibility::Collapsed;
}

Object ^ BoolToVisibilityConverter::ConvertBack(
    Object^ value,
    Windows::UI::Xaml::Interop::TypeName targetType,
    Object^ parameter,
    String^ language)
{
    if (value != nullptr)
    {
        auto vis = safe_cast<Visibility>(value);
        return (vis == Visibility::Visible);
    }
    return false;
}
