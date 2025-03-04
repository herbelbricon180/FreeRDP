/**
 * WinPR: Windows Portable Runtime
 * NCrypt library
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/assert.h>
#include <winpr/ncrypt.h>

#ifndef _WIN32

#include <winpr/print.h>
#include "../log.h"

#include "ncrypt.h"

#define TAG WINPR_TAG("ncrypt")

const static char NCRYPT_MAGIC[6] = { 'N', 'C', 'R', 'Y', 'P', 'T' };

SECURITY_STATUS checkNCryptHandle(NCRYPT_HANDLE handle, NCryptHandleType matchType)
{
	NCryptBaseHandle* base;
	if (!handle)
		return ERROR_INVALID_PARAMETER;

	base = (NCryptBaseHandle*)handle;
	if (memcmp(base->magic, NCRYPT_MAGIC, 6) != 0)
		return ERROR_INVALID_PARAMETER;

	switch (base->type)
	{
		case WINPR_NCRYPT_PROVIDER:
		case WINPR_NCRYPT_KEY:
			break;
		default:
			return ERROR_INVALID_PARAMETER;
	}

	if (matchType != WINPR_NCRYPT_INVALID && base->type != matchType)
		return ERROR_INVALID_PARAMETER;
	return ERROR_SUCCESS;
}

void* ncrypt_new_handle(NCryptHandleType kind, size_t len, NCryptGetPropertyFn getProp,
                        NCryptReleaseFn dtor)
{
	NCryptBaseHandle* ret = calloc(1, len);
	if (!ret)
		return NULL;

	memcpy(ret->magic, NCRYPT_MAGIC, sizeof(ret->magic));
	ret->type = kind;
	ret->getPropertyFn = getProp;
	ret->releaseFn = dtor;
	return ret;
}

SECURITY_STATUS winpr_NCryptDefault_dtor(NCryptBaseHandle* h)
{
	memset(h->magic, 0, sizeof(h->magic));
	h->type = WINPR_NCRYPT_INVALID;
	h->releaseFn = NULL;
	free(h);
	return ERROR_SUCCESS;
}

SECURITY_STATUS NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider, LPCWSTR pszProviderName,
                                          DWORD dwFlags)
{

#ifdef WITH_PKCS11
	if (_wcscmp(pszProviderName, MS_SMART_CARD_KEY_STORAGE_PROVIDER) == 0)
	{
		static LPCSTR openscPaths[] = { "/usr/lib/x86_64-linux-gnu/pkcs11/opensc-pkcs11.so", NULL };

		return winpr_NCryptOpenStorageProviderEx(phProvider, pszProviderName, dwFlags, openscPaths);
	}
#endif

	return ERROR_NOT_SUPPORTED;
}

SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                                  LPCWSTR pszProviderName, DWORD dwFlags,
                                                  LPCSTR* modulePaths)
{
#ifdef WITH_PKCS11
	if (_wcscmp(pszProviderName, MS_SMART_CARD_KEY_STORAGE_PROVIDER) == 0 ||
	    _wcscmp(pszProviderName, MS_SCARD_PROV) == 0)
	{
		SECURITY_STATUS NCryptOpenP11StorageProviderEx(NCRYPT_PROV_HANDLE * phProvider,
		                                               LPCWSTR pszProviderName, DWORD dwFlags,
		                                               LPCSTR * modulePaths);
		return NCryptOpenP11StorageProviderEx(phProvider, pszProviderName, dwFlags, modulePaths);
	}
#endif
	return ERROR_NOT_SUPPORTED;
}

SECURITY_STATUS NCryptEnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
                               NCryptKeyName** ppKeyName, PVOID* ppEnumState, DWORD dwFlags)
{
	SECURITY_STATUS ret;
	NCryptBaseProvider* provider = (NCryptBaseProvider*)hProvider;

	ret = checkNCryptHandle((NCRYPT_HANDLE)hProvider, WINPR_NCRYPT_PROVIDER);
	if (ret != ERROR_SUCCESS)
		return ret;

	return provider->enumKeysFn(hProvider, pszScope, ppKeyName, ppEnumState, dwFlags);
}

SECURITY_STATUS NCryptOpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
                              LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags)
{
	SECURITY_STATUS ret;
	NCryptBaseProvider* provider = (NCryptBaseProvider*)hProvider;

	ret = checkNCryptHandle((NCRYPT_HANDLE)hProvider, WINPR_NCRYPT_PROVIDER);
	if (ret != ERROR_SUCCESS)
		return ret;
	if (!phKey || !pszKeyName)
		return ERROR_INVALID_PARAMETER;

	return provider->openKeyFn(hProvider, phKey, pszKeyName, dwLegacyKeySpec, dwFlags);
}

static NCryptKeyGetPropertyEnum propertyStringToEnum(LPCWSTR pszProperty)
{
	if (_wcscmp(pszProperty, NCRYPT_CERTIFICATE_PROPERTY) == 0)
	{
		return NCRYPT_PROPERTY_CERTIFICATE;
	}

	return NCRYPT_PROPERTY_UNKNOWN;
}

SECURITY_STATUS NCryptGetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty, PBYTE pbOutput,
                                  DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags)
{
	NCryptKeyGetPropertyEnum property;
	NCryptBaseHandle* base;

	if (!hObject)
		return ERROR_INVALID_PARAMETER;

	base = (NCryptBaseHandle*)hObject;
	if (memcmp(base->magic, NCRYPT_MAGIC, 6) != 0)
		return ERROR_INVALID_HANDLE;

	property = propertyStringToEnum(pszProperty);
	if (property == NCRYPT_PROPERTY_UNKNOWN)
		return ERROR_NOT_SUPPORTED;

	return base->getPropertyFn(hObject, property, pbOutput, cbOutput, pcbResult, dwFlags);
}

SECURITY_STATUS NCryptFreeObject(NCRYPT_HANDLE hObject)
{
	NCryptBaseHandle* base;
	SECURITY_STATUS ret = checkNCryptHandle((NCRYPT_HANDLE)hObject, WINPR_NCRYPT_INVALID);
	if (ret != ERROR_SUCCESS)
		return ret;

	base = (NCryptBaseHandle*)hObject;
	if (base->releaseFn)
		ret = base->releaseFn(hObject);

	return ret;
}

SECURITY_STATUS NCryptFreeBuffer(PVOID pvInput)
{
	if (!pvInput)
		return ERROR_INVALID_PARAMETER;

	free(pvInput);
	return ERROR_SUCCESS;
}

#else
SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                                  LPCWSTR pszProviderName, DWORD dwFlags,
                                                  LPCSTR* modulePaths)
{
	typedef SECURITY_STATUS (*NCryptOpenStorageProviderFn)(NCRYPT_PROV_HANDLE * phProvider,
	                                                       LPCWSTR pszProviderName, DWORD dwFlags);
	NCryptOpenStorageProviderFn ncryptOpenStorageProviderFn;
	SECURITY_STATUS ret;
	HANDLE lib = LoadLibraryA("ncrypt.dll");
	if (!lib)
		return NTE_PROV_DLL_NOT_FOUND;

	ncryptOpenStorageProviderFn =
	    (NCryptOpenStorageProviderFn)GetProcAddress(lib, "NCryptOpenStorageProvider");
	if (!ncryptOpenStorageProviderFn)
	{
		ret = NTE_PROV_DLL_NOT_FOUND;
		goto out_free_lib;
	}

	ret = ncryptOpenStorageProviderFn(phProvider, pszProviderName, dwFlags);

out_free_lib:
	FreeLibrary(lib);
	return ret;
}
#endif
