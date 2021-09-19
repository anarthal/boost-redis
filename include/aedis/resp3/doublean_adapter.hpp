/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <aedis/type.hpp>
#include <aedis/response_adapter_base.hpp>

namespace aedis { namespace resp3 {

struct doublean_adapter : public response_adapter_base {
   doublean* result = nullptr;

   doublean_adapter(doublean* p) : result(p) {}

   void on_double(std::string_view s) override
      { *result = s; }
};

} // resp3
} // aedis