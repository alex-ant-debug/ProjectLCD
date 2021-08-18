#include "sf_audio_dummy.h"

void sf_audio_dummy_entry(void);

/* Audio Framework Dummy Thread entry function */
void sf_audio_dummy_entry(void)
{
    /* TODO: add your own code here */
    while (1)
    {
        tx_thread_sleep (1);
    }
}
