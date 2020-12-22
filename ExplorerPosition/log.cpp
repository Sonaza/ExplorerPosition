#include "log.h"

#include "includewindows.h"

#include <cstdio>
#include <cstdarg>
#include <clocale>

const size_t bufferSize = 1024;

void dprintf(const char *formatParam, ...)
{
	va_list args;
	va_start(args, formatParam);

	char buffer[bufferSize] = { 0 };

	vsprintf_s(buffer, bufferSize, formatParam, args);
	va_end(args);

	printf(buffer);
	OutputDebugStringA(buffer);
}

void dwprintf(const wchar_t *formatParam, ...)
{
	va_list args;
	va_start(args, formatParam);
;
	wchar_t buffer[bufferSize] = { 0 };

	vswprintf_s(buffer, bufferSize, formatParam, args);
	va_end(args);

	wprintf(buffer);
	OutputDebugStringW(buffer);
}

namespace
{

class Nonsense
{
public:
	Nonsense()
	{
		setlocale(LC_ALL, "");
	}
};
Nonsense nonsenseInitializer;

}
