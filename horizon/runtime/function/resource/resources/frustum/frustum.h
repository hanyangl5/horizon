/*****************************************************************//**
 * \file   FILE_NAME
 * \brief  BRIEF
 * 
 * \author XXX
 * \date   XXX
 *********************************************************************/

#pragma once

// standard libraries

// third party libraries

// project headers

class CLASS_NAME {
  public:
    CLASS_NAME() noexcept;
    ~CLASS_NAME() noexcept;

    CLASS_NAME(const CLASS_NAME &rhs) noexcept = default;
    CLASS_NAME &operator=(const CLASS_NAME &rhs) noexcept = default;
    CLASS_NAME(CLASS_NAME &&rhs) noexcept = default;
    CLASS_NAME &operator=(CLASS_NAME &&rhs) noexcept = default;
}