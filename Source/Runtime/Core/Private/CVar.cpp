/* Copyright (c) 2026 Horizon */
#include "Core/ICVar.h"

#include <math.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#endif

namespace hz
{
static CVar* FirstCVar = nullptr;

static int CompareCVarNames(const char* Left, const char* Right)
{
#if defined(_WIN32)
    return _stricmp(Left, Right);
#else
    return strcasecmp(Left, Right);
#endif
}

CVar::CVar(const char* Name, bool Default, const char* Help): Name(Name), Help(Help), Type(Kind::Boolean), Minimum(0), Toggle(Default)
{
    Register();
}

CVar::CVar(const char* Name, float Default, float Minimum, const char* Help):
    Name(Name), Help(Help), Type(Kind::Float), Minimum(Minimum), Number(Default)
{
    ASSERT(isfinite(Default) && Default >= Minimum);
    Register();
}

void CVar::Register()
{
    ASSERT(Name && Name[0] && Help);
    CVar** Link = &FirstCVar;
    while (*Link)
    {
        const int Order = CompareCVarNames((*Link)->Name, Name);
        if (Order == 0)
        {
            ASSERTMSG(false, "Duplicate CVar '%s'", Name);
            return;
        }
        if (Order > 0)
        {
            break;
        }
        Link = &(*Link)->Next;
    }
    Next = *Link;
    *Link = this;
}

CVar::~CVar()
{
    CVar** Link = &FirstCVar;
    while (*Link && *Link != this)
    {
        Link = &(*Link)->Next;
    }
    if (*Link)
    {
        *Link = Next;
    }
}

CVar* GetFirstCVar() { return FirstCVar && !FirstCVar->Available ? FirstCVar->GetNext() : FirstCVar; }

CVar* CVar::GetNext() const
{
    CVar* Entry = Next;
    while (Entry && !Entry->Available)
    {
        Entry = Entry->Next;
    }
    return Entry;
}

CVar* GetCVar(const char* Name)
{
    for (CVar* Entry = GetFirstCVar(); Entry; Entry = Entry->GetNext())
    {
        if (CompareCVarNames(Entry->Name, Name) == 0)
        {
            return Entry;
        }
    }
    return nullptr;
}

void CVar::SetApplyBool(ApplyBool Callback, void* User)
{
    ASSERT(Type == Kind::Boolean);
    Apply = Callback;
    ApplyUser = User;
}

bool CVar::Set(bool Value)
{
    if (!Available || Type != Kind::Boolean || (Apply && !Apply(ApplyUser, Value)))
    {
        return false;
    }
    Toggle = Value;
    return true;
}

bool CVar::Set(float Value)
{
    if (!Available || Type != Kind::Float || !isfinite(Value) || Value < Minimum)
    {
        return false;
    }
    Number = Value;
    return true;
}
} // namespace hz
