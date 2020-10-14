#include "headfiles.h"

void main(void)
{
   disableInterrupts();
   modules_init();
   enableInterrupts();
   system_init();

   while (1)
   {
      task_handle();
   }
}





