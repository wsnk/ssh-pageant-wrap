/* Based on weasel-pageant by Valtteri Vuorikoski
 *
 * Pageant client code.
 * Copyright 2017  Valtteri Vuorikoski
 * Based on ssh-pageant, Copyright 2009, 2011  Josh Stone
 *
 * This file is part of weasel-pageant, and is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This file is derived from part of the PuTTY program, whose original
 * license is available in COPYING.PuTTY.
 */

#include "pageant.h"
#include <windows.h>
#include <cstring>
#include <cinttypes>


#define AGENT_COPYDATA_ID 0x804e50ba   /* random goop */
#define AGENT_MAX_MSGLEN  8192
#define SSH_AGENT_FAILURE 5


namespace {

inline uint32_t msglen(const void *p) {
    return 4 + ntohl(*(const uint32_t *)p);
}

HWND FindPageant() {
    HWND hwnd = FindWindowA("Pageant", "Pageant");
    if (!hwnd)
        THROW_RUNTIME_ERROR("Pageant window not found: " << GetLastError());

    LOG_DEBUG("Pageant window found");
    return hwnd;
}

static PSID get_user_sid(void)
{
    HANDLE proc = NULL, tok = NULL;
    TOKEN_USER *user = NULL;
    DWORD toklen, sidlen;
    PSID sid = NULL, ret = NULL;

    if ((proc = OpenProcess(MAXIMUM_ALLOWED, FALSE, GetCurrentProcessId()))
            && OpenProcessToken(proc, TOKEN_QUERY, &tok)
            && (!GetTokenInformation(tok, TokenUser, NULL, 0, &toklen)
                && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            && (user = (TOKEN_USER *)LocalAlloc(LPTR, toklen))
            && GetTokenInformation(tok, TokenUser, user, toklen, &toklen)) {
        sidlen = GetLengthSid(user->User.Sid);
        sid = (PSID)malloc(sidlen);
        if (sid && CopySid(sidlen, sid, user->User.Sid)) {
            /* Success. Move sid into the return value slot, and null it out
             * to stop the cleanup code freeing it. */
            ret = sid;
            sid = NULL;
        }
    }

    if (proc != NULL)
        CloseHandle(proc);
    if (tok != NULL)
        CloseHandle(tok);
    LocalFree(user);
    free(sid);

    return ret;
}

}  // anonymous namespace


FileMapping::FileMapping(const std::string& mappingName, size_t maxSize, SECURITY_ATTRIBUTES* sa)
    : Name(mappingName)
{
    const DWORD hoMaxSize = maxSize >> sizeof(DWORD) * 8;
    const DWORD loMaxSize = maxSize & ~DWORD(0);

    Handle = CreateFileMappingA(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, hoMaxSize, loMaxSize, Name.c_str());
    if (Handle == nullptr || Handle == INVALID_HANDLE_VALUE)
        THROW_RUNTIME_ERROR("File mapping '" << Name << "' failed: " << GetLastError());

    LOG_DEBUG("Anonymous file mapping '" << Name << "' of " << maxSize << " bytes created");

    View = MapViewOfFile(Handle, FILE_MAP_WRITE, 0, 0, 0);
    if (View == nullptr) {
        CloseHandle(Handle);
        THROW_RUNTIME_ERROR("View if file mapping '" << Name << "' failed: " << GetLastError());
    }

    LOG_DEBUG("View of anonymous file mapping '" << Name << "' created");
}

FileMapping::~FileMapping()
{
    if (Handle && View) {
        UnmapViewOfFile(View);
        CloseHandle(Handle);
        LOG_DEBUG("Anonymous file mapping '" << Name << "' closed");
    }
}

FileMapping::FileMapping(FileMapping&& x)
    : Name(std::move(x.Name))
    , Handle(x.Handle)
    , View(x.View)
{
    x.Handle = nullptr;
    x.View = nullptr;
}

FileMapping& FileMapping::operator =(FileMapping&& x)
{
    std::swap(Name, x.Name);
    std::swap(Handle, x.Handle);
    std::swap(View, x.View);
    return *this;
}


void Pageant::MakeError(Buffer& msg)
{
    static const char err[5] = { 0, 0, 0, 1, SSH_AGENT_FAILURE };
    msg.len = sizeof(err);
    std::memcpy(msg.ptr, err, msg.len);
}

Pageant::Pageant()
    : Hwnd(nullptr)
    , Mapping()
{
    // The mapping name needs to be ANSI or bad stuff happens
    char mapname[] = "PageantRequest12345678";
    sprintf_s(mapname, sizeof(mapname), "PageantRequest%08x", (unsigned)GetCurrentThreadId());

    PSECURITY_DESCRIPTOR psd = NULL;
    SECURITY_ATTRIBUTES sa, *psa = NULL;
    PSID usersid = get_user_sid();
    if (usersid) {
        psd = (PSECURITY_DESCRIPTOR)
            LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
        if (psd) {
            if (InitializeSecurityDescriptor
                    (psd, SECURITY_DESCRIPTOR_REVISION)
                    && SetSecurityDescriptorOwner(psd, usersid, FALSE)) {
                sa.nLength = sizeof(sa);
                sa.bInheritHandle = TRUE;
                sa.lpSecurityDescriptor = psd;
                psa = &sa;
            }
        }
    }

    Mapping = FileMapping(mapname, AGENT_MAX_MSGLEN, psa);

    LocalFree(psd);
    free(usersid);
}

Pageant::~Pageant()
{ }


void Pageant::Query(Buffer& msg) const
{
    if (!Hwnd)
        Hwnd = FindPageant();

    std::memcpy(Mapping.GetView(), msg.ptr, msg.len);

    COPYDATASTRUCT cds = {
        .dwData = AGENT_COPYDATA_ID,
        .cbData = 1 + Mapping.GetName().length(),
        .lpData = const_cast<char*>(Mapping.GetName().c_str()),
    };

    LRESULT id = SendMessage(Hwnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
    if (id == 0) {
        DWORD err = GetLastError();
        if (err == ERROR_INVALID_WINDOW_HANDLE) {
            // TODO: do the same but better
            LOG_DEBUG("Pageant's window handle became invalid, try to find it again");
            Hwnd = nullptr;
            return Query(msg);
        }
        THROW_RUNTIME_ERROR("Pageant failed: " << err);
    }

    const size_t respLen = msglen(Mapping.GetView());
    if (respLen > AGENT_MAX_MSGLEN)
        THROW_RUNTIME_ERROR("Pageant response message is too big: " << respLen);

    msg.len = respLen;
    std::memcpy(msg.ptr, Mapping.GetView(),respLen);
}

void Pageant::TryQuery(Buffer& msg) const noexcept
{
    try {
        Query(msg);
    } catch (const std::exception& exc) {
        LOG_ERROR("Pageant query failed: " << exc.what());
        MakeError(msg);
    }
}

