#pragma once

namespace hz
{
class CVar;
}

struct ImGuiInputTextCallbackData;

class ConsolePanel
{
public:
    void Draw();

private:
    void       Execute();
    void       Print(const char* Format, ...);
    void       Describe(const hz::CVar& Var);
    static int EditInput(ImGuiInputTextCallbackData* Data);

    static constexpr int HistoryLimit = 16, OutputLimit = 64, InputSize = 128;
    char                 Input[InputSize] = {};
    char                 Draft[InputSize] = {};
    char                 History[HistoryLimit][InputSize] = {};
    int                  HistoryHead = 0, HistoryCount = 0, HistoryOffset = 0;
    char                 Output[OutputLimit][256] = {};
    int                  OutputHead = 0, OutputCount = 0;
    bool                 ScrollToEnd = false;
};
