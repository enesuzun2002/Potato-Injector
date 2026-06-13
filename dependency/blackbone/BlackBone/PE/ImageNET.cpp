#include "../Config.h"
#include "ImageNET.h"

#ifdef COMPILER_MSVC

namespace blackbone
{

ImageNET::ImageNET( void )
{
}

ImageNET::~ImageNET(void)
{
}

bool ImageNET::Init( const std::wstring& path )
{
    return false;
}

bool ImageNET::Parse( mapMethodRVA* methods /*= nullptr*/ )
{
    return false;
}

std::wstring ImageNET::GetImageRuntimeVer( const wchar_t* ImagePath )
{
    return L"n/a";
}

}

#endif