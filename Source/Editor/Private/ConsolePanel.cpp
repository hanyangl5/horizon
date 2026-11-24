#include "Editor/ConsolePanel.h"
#include "Core/ICVar.h"
#include "imgui.h"

#include <charconv>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void ConsolePanel::Print(const char* Format, ...)
{
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Output[OutputHead], sizeof(Output[0]), Format, Args);
    va_end(Args);
    OutputHead = (OutputHead + 1) % OutputLimit;
    if (OutputCount < OutputLimit)
    {
        ++OutputCount;
    }
    ScrollToEnd = true;
}

void ConsolePanel::Describe(const hz::CVar& Var)
{
    if (Var.Type == hz::CVar::Kind::Boolean)
    {
        Print("%s = %d   %s", Var.Name, Var.GetBool() ? 1 : 0, Var.Help);
    }
    else
    {
        Print("%s = %.7g   %s", Var.Name, Var.GetFloat(), Var.Help);
    }
}

void ConsolePanel::Execute()
{
    char* Command = Input + strspn(Input, " \t");
    char* End = Command + strlen(Command);
    while (End > Command && (End[-1] == ' ' || End[-1] == '\t'))
    {
        --End;
    }
    *End = 0;
    if (!*Command)
    {
        return;
    }

    if (!HistoryCount || _stricmp(History[(HistoryHead + HistoryLimit - 1) % HistoryLimit], Command))
    {
        snprintf(History[HistoryHead], InputSize, "%s", Command);
        HistoryHead = (HistoryHead + 1) % HistoryLimit;
        if (HistoryCount < HistoryLimit)
        {
            ++HistoryCount;
        }
    }
    HistoryOffset = 0;
    Print("> %s", Command);
    char* Argument = Command + strcspn(Command, " \t");
    if (*Argument)
    {
        *Argument++ = 0;
        Argument += strspn(Argument, " \t");
    }

    if (!_stricmp(Command, "help"))
    {
        Print("Enter a name to read it or add a value to change it. Tab completes names. Up/Down recall commands.");
        const size_t Prefix = strlen(Argument);
        bool         Found = false;
        for (hz::CVar* Var = hz::GetFirstCVar(); Var; Var = Var->GetNext())
        {
            if (!_strnicmp(Var->Name, Argument, Prefix))
            {
                Describe(*Var);
                Found = true;
            }
        }
        if (!Found)
        {
            Print("No variables match '%s'.", Argument);
        }
        return;
    }
    if (hz::CVar* Var = hz::GetCVar(Command))
    {
        if (*Argument && strcmp(Argument, "?"))
        {
            if (Var->Type == hz::CVar::Kind::Boolean)
            {
                const bool Enabled = !strcmp(Argument, "1") || !_stricmp(Argument, "true");
                if (!Enabled && strcmp(Argument, "0") && _stricmp(Argument, "false"))
                {
                    Print("%s expects 0, 1, false or true.", Var->Name);
                    return;
                }
                if (!Var->Set(Enabled))
                {
                    Print("Could not apply %s.", Var->Name);
                    return;
                }
            }
            else
            {
                float                        Number = 0;
                const std::from_chars_result Result = std::from_chars(Argument, End, Number);
                if (Result.ec != std::errc{} || Result.ptr != End || !Var->Set(Number))
                {
                    Print("%s expects a finite number >= %.7g.", Var->Name, Var->Minimum);
                    return;
                }
            }
        }
        Describe(*Var);
        return;
    }
    Print("Unknown command '%s'. Type help to list variables.", Command);
}

int ConsolePanel::EditInput(ImGuiInputTextCallbackData* Data)
{
    ConsolePanel& Console = *static_cast<ConsolePanel*>(Data->UserData);
    if (Data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
    {
        if (Data->EventKey == ImGuiKey_UpArrow && Console.HistoryOffset < Console.HistoryCount)
        {
            if (!Console.HistoryOffset)
            {
                snprintf(Console.Draft, InputSize, "%s", Data->Buf);
            }
            ++Console.HistoryOffset;
        }
        else if (Data->EventKey == ImGuiKey_DownArrow && Console.HistoryOffset > 0)
        {
            --Console.HistoryOffset;
        }
        else
        {
            return 0;
        }
        const int Index = (Console.HistoryHead + HistoryLimit - Console.HistoryOffset) % HistoryLimit;
        Data->DeleteChars(0, Data->BufTextLen);
        Data->InsertChars(0, Console.HistoryOffset ? Console.History[Index] : Console.Draft);
    }
    else if (Data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && Data->BufTextLen && !strpbrk(Data->Buf, " \t"))
    {
        const hz::CVar* Match = nullptr;
        int             Count = 0;
        for (hz::CVar* Var = hz::GetFirstCVar(); Var; Var = Var->GetNext())
        {
            if (!_strnicmp(Var->Name, Data->Buf, Data->BufTextLen))
            {
                if (++Count == 2)
                {
                    Console.Print("%s", Match->Name);
                }
                if (Count > 1)
                {
                    Console.Print("%s", Var->Name);
                }
                Match = Var;
            }
        }
        if (Count == 1)
        {
            Data->DeleteChars(0, Data->BufTextLen);
            Data->InsertChars(0, Match->Name);
            Data->InsertChars(Data->BufTextLen, " ");
        }
    }
    return 0;
}

void ConsolePanel::Draw()
{
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Command", "Command (help)", Input, sizeof(Input),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory |
                                     ImGuiInputTextFlags_CallbackCompletion,
                                 EditInput, this))
    {
        Execute();
        Input[0] = 0;
        ImGui::SetKeyboardFocusHere(-1);
    }
    if (!OutputCount)
    {
        return;
    }
    if (ImGui::BeginChild("Console output", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
    {
        for (int I = 0; I < OutputCount; ++I)
        {
            ImGui::TextWrapped("%s", Output[(OutputHead + OutputLimit - OutputCount + I) % OutputLimit]);
        }
        if (ScrollToEnd)
        {
            ImGui::SetScrollHereY(1);
            ScrollToEnd = false;
        }
    }
    ImGui::EndChild();
}
