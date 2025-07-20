#include <stdint.h>
#include <stdbool.h>
#include <gem.h>
#include <osbind.h>
#include <unistd.h>

void redraw(int16_t vdi_handle, int16_t x, int16_t y, int16_t w, int16_t h) {
    int16_t pxy[4];

    pxy[0] = x;
    pxy[1] = y;
    pxy[2] = x + w - 1;
    pxy[3] = y + h - 1;

    vswr_mode(vdi_handle, MD_REPLACE);
    vsf_color(vdi_handle, G_WHITE);
    vr_recfl(vdi_handle, pxy);
}

int main(void)
{
    int16_t vdi_handle;
    int16_t work_in[11];
    int16_t work_out[57];
    int16_t msg_buff[8];
    int16_t win_x, win_y, win_w, win_h;
    int16_t desk_x, desk_y, desk_w, desk_h;
    int16_t win_handle;
    int16_t dummy;
    int32_t timer;
    bool mouse_hidden = false;

    if (appl_init() < 0) {
        return 1;
    }

	sleep(1);

    vdi_handle = graf_handle(&work_in[0], &work_out[0], &work_out[0], &work_out[0]);
    work_in[0] = Getrez() + 2;
    work_in[10] = 2;
    v_opnvwk(work_in, &vdi_handle, work_out);
    if (vdi_handle == 0) {
        appl_exit();
        return 1;
    }

    wind_get(0, WF_WORKXYWH, &desk_x, &desk_y, &desk_w, &desk_h);

    win_w = 144;
    win_y = desk_y;
    win_h = 168 - win_y;
    win_x = (desk_w - win_w) / 2 + desk_x;

    win_handle = wind_create(NAME | MOVER, win_x, win_y, win_w, win_h);
    if (win_handle < 0) {
        v_clsvwk(vdi_handle);
        appl_exit();
        return 1;
    }

    wind_set_str(win_handle, WF_NAME, "ATARI ST CLOCK");
    wind_open(win_handle, win_x, win_y, win_w, win_h);

    int16_t work_x, work_y, work_w, work_h;
    wind_get(win_handle, WF_WORKXYWH, &work_x, &work_y, &work_w, &work_h);
    redraw(vdi_handle, work_x, work_y, work_w, work_h);

    graf_mouse(M_ON, 0L);
    timer = 0;

    while (1) {
        int16_t event = evnt_multi(MU_MESAG | MU_TIMER | MU_BUTTON,
                                 1, 1, 1,
                                 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0,
                                 msg_buff, 1000, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy);

        if (event & MU_BUTTON) {
            if (mouse_hidden) {
                graf_mouse(M_ON, 0L);
                mouse_hidden = false;
            }
            timer = 0;
        }

        if (event & MU_TIMER) {
            if (!mouse_hidden && ++timer > 100) {
                graf_mouse(M_OFF, 0L);
                mouse_hidden = true;

                int16_t redraw_msg[8];
                wind_get(win_handle, WF_WORKXYWH, &work_x, &work_y, &work_w, &work_h);
                redraw_msg[0] = WM_REDRAW;
                redraw_msg[1] = gl_apid;
                redraw_msg[2] = 0;
                redraw_msg[3] = win_handle;
                redraw_msg[4] = work_x;
                redraw_msg[5] = work_y;
                redraw_msg[6] = work_w;
                redraw_msg[7] = work_h;
                appl_write(gl_apid, 16, redraw_msg);
            }
        }

        if (event & MU_MESAG) {
            switch (msg_buff[0]) {
                case WM_REDRAW:
                    {
                        int16_t redraw_handle = msg_buff[3];
                        int16_t rx, ry, rw, rh;
                        wind_update(BEG_UPDATE);
                        wind_get(redraw_handle, WF_FIRSTXYWH, &rx, &ry, &rw, &rh);
                        while (rw && rh)
                        {
                            redraw(vdi_handle, rx, ry, rw, rh);
                            wind_get(redraw_handle, WF_NEXTXYWH, &rx, &ry, &rw, &rh);
                        }
                        wind_update(END_UPDATE);
                    }
                    break;
                case WM_CLOSED:
                    if (msg_buff[3] == win_handle) {
                        goto exit_loop;
                    }
                    break;
            }
        }
    }

exit_loop:
    wind_close(win_handle);
    wind_delete(win_handle);

    v_clsvwk(vdi_handle);
    appl_exit();

    return 0;
}
