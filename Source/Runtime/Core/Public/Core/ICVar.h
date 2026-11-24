/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ILog.h"

namespace hz
{
// Registration, lookup and edits run on the main thread. Name and Help must outlive the variable.
class CVar
{
public:
    enum class Kind
    {
        Boolean,
        Float
    };
    using ApplyBool = bool (*)(void*, bool);

    CVar(const char* Name, bool Default, const char* Help);
    CVar(const char* Name, float Default, float Minimum, const char* Help);
    ~CVar();
    CVar(const CVar&) = delete;
    CVar& operator=(const CVar&) = delete;

    bool GetBool() const
    {
        ASSERT(Type == Kind::Boolean);
        return Toggle;
    }
    float GetFloat() const
    {
        ASSERT(Type == Kind::Float);
        return Number;
    }
    bool  Set(bool Value);
    bool  Set(float Value);
    void  SetApplyBool(ApplyBool Callback, void* User);
    void  SetAvailable(bool Enabled) { Available = Enabled; }
    CVar* GetNext() const;

    const char* const Name;
    const char* const Help;
    const Kind        Type;
    const float       Minimum;

private:
    friend CVar* GetFirstCVar();
    void         Register();

    CVar*     Next = nullptr;
    ApplyBool Apply = nullptr;
    void*     ApplyUser = nullptr;
    float     Number = 0;
    bool      Toggle = false;
    bool      Available = true;
};

CVar* GetFirstCVar();
CVar* GetCVar(const char* Name);
} // namespace hz
