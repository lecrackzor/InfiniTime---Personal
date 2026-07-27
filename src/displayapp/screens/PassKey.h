#pragma once

#include "Screen.h"
#include <lvgl/lvgl.h>

namespace Pinetime {
  namespace Applications {
    namespace Screens {

      class PassKey : public Screen {
      public:
        explicit PassKey(uint32_t key);
        ~PassKey() override;
        void UpdateKey(uint32_t key);

      private:
        lv_obj_t* passkeyLabel;
      };
    }
  }
}
