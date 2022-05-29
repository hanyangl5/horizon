#pragma once

namespace Horizon
{
    namespace RHI
    {

        class CommandList
        {
        public:
            CommandList() noexcept;
            ~CommandList() noexcept;
            virtual void BeginRecording() noexcept = 0;
            virtual void EndRecording() noexcept = 0;

            virtual void Draw() noexcept = 0;
            virtual void DrawIndirect() noexcept = 0;
            virtual void Dispatch() noexcept = 0;
            virtual void DispatchIndirect() noexcept = 0;
            virtual void UpdateBuffer() noexcept = 0;
            virtual void UpdateTexture() noexcept = 0;

            virtual void CopyBuffer() noexcept = 0;
            virtual void CopyTexture() noexcept = 0;

            virtual void InsertBarreir() noexcept = 0;
            virtual void Submit() noexcept = 0;

        private:
            // CommandListType m_type;
        };
    }
}
