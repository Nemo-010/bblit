#include <stdio.h>
#include <stdint.h>
extern int bblit_gl_side_for(unsigned dest, void **out);
void bblit_gl_probe(void) {
    static const unsigned slots[] = {0x7c632c,0x7c6368,0x7c62c0,0x7c6614};
    for (int k = 0; k < 4; k++) {
        unsigned img = *(unsigned *)(uintptr_t)slots[k];
        void *side = 0;
        int idx = bblit_gl_side_for(slots[k], &side);
        fprintf(stderr, "[glprobe] slot %#x img=%#x side[%d]=%p\n",
                slots[k], img, idx, side);
    }
    {
        unsigned *glent = (unsigned *)(uintptr_t)0x468750u;
        for (int n = 0; n < 400; n++) {
            unsigned dest = glent[0];
            const char *nm = (const char *)(uintptr_t)glent[1];
            for (int k = 0; k < 4; k++)
                if (dest == slots[k])
                    fprintf(stderr, "[glprobe] dest %#x name=%s\n", dest, nm);
            glent += 2;
            if (dest == 0) break;
        }
    }
}
