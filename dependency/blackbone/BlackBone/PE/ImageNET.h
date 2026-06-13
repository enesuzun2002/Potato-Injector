#pragma once
#include "../Config.h"
#ifdef COMPILER_MSVC

#include "../Include/Winheaders.h"

#include <map>
#include <string>

namespace blackbone
{

/// <summary>
/// .NET metadata parser (Stubbed out)
/// </summary>
class ImageNET
{
public:
    using mapMethodRVA = std::map<std::pair<std::wstring, std::wstring>, uintptr_t>;

public:
    BLACKBONE_API ImageNET(void);
    BLACKBONE_API ~ImageNET(void);

    BLACKBONE_API bool Init( const std::wstring& path );
    BLACKBONE_API bool Parse( mapMethodRVA* methods = nullptr );
    BLACKBONE_API static std::wstring GetImageRuntimeVer( const wchar_t* ImagePath );
};

}

#endif
