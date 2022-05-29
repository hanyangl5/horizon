#pragma once

namespace Horizon
{
    namespace RHI
    {

        class CommandList
        {
        public:
            CommandList();
            ~CommandList();
            virtual void BeginRecording() = 0;
            virtual void EndRecording() = 0;

            virtual void Draw() = 0;
            virtual void DrawIndirect() = 0;
            virtual void Dispatch() = 0;
            virtual void DispatchIndirect() = 0;
            virtual void UpdateBuffer() = 0;
            virtual void UpdateTexture() = 0;

            virtual void CopyBuffer() = 0;
            virtual void CopyTexture() = 0;

            virtual void InsertBarreir() = 0;
            virtual void Submit() = 0;
        private:
            //CommandListType m_type;

        };
    }
}
