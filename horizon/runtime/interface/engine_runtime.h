#pragma once

class EngineRuntime {
  public:
    virtual void Initilize() noexcept = 0;
    virtual void Tick() noexcept = 0;
    virtual void Destroy() noexcept = 0;
};
