#include <new>
#include <stddef.h>

#include <vsh/sys_prx_for_user.hpp>

static void* Allocate(size_t size)
{
	return sysPrxForUser::_sys_malloc(size);
}

void* operator new(size_t size)
{
	return Allocate(size);
}

void* operator new[](size_t size)
{
	return Allocate(size);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept
{
	return Allocate(size);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept
{
	return Allocate(size);
}

void* operator new(size_t size, size_t align)
{
	return sysPrxForUser::_sys_memalign(align, size);
}

void* operator new[](size_t size, size_t align)
{
	return sysPrxForUser::_sys_memalign(align, size);
}

void operator delete(void* pointer) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}

void operator delete[](void* pointer) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}

void operator delete(void* pointer, size_t) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}

void operator delete[](void* pointer, size_t) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}

void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
	sysPrxForUser::_sys_free(pointer);
}
