/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef BB_PC_MENU_POLICY_H
#define BB_PC_MENU_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* The title/frontend keeps the donor's WASD navigation. Only keyboard/mouse
 * input in an active world is separated from the menu's D-pad navigation. */
static bool pc_filter_world_input(bool enabled, bool world_started, bool overlay,
                                 bool text_entry, bool rebinding, bool foreground) {
    return enabled && world_started && !overlay && !text_entry && !rebinding && foreground;
}

typedef struct {
    double angle;
    uint64_t at, active_at, pressed_at[4];
    int last_x, last_y;
    bool active;
} PcMovement;

static int pc_resolve_axis(PcMovement* s, int negative, int positive,
                           const bool held[4], uint64_t now) {
    const int indices[2] = {negative, positive};
    for(int i=0;i<2;i++) {
        int k=indices[i];
        if(held[k]) { if(!s->pressed_at[k])s->pressed_at[k]=now; }
        else s->pressed_at[k]=0;
    }
    if(held[negative] && held[positive]) {
        if(s->pressed_at[negative] > s->pressed_at[positive])return -127;
        if(s->pressed_at[positive] > s->pressed_at[negative])return 127;
        return 0;
    }
    return held[negative] ? -127 : held[positive] ? 127 : 0;
}

static void pc_shape_movement(PcMovement* s, const bool held[4], bool walking,
                              uint64_t now, int delay_ms, int rate, bool circular,
                              int* out_x, int* out_y) {
    int x=pc_resolve_axis(s,2,3,held,now),y=pc_resolve_axis(s,0,1,held,now);
    if(x || y) {s->last_x=x;s->last_y=y;s->active_at=now;}
    else if(delay_ms>0 && now-s->active_at<(uint64_t)delay_ms*1000u) {
        x=s->last_x;y=s->last_y;
    } else {s->last_x=s->last_y=0;}
    if(!x && !y) {s->active=false;*out_x=*out_y=0;return;}
    const double pi=3.14159265358979323846;
    double target=atan2((double)y,(double)x);
    if(s->active && rate>0) {
        double diff=fmod(target-s->angle+3*pi,2*pi)-pi;
        double step=fmin(rate*pi/180.0*(double)(now-s->at)/1e6,30*pi/180.0);
        double dir=diff>0 ? 1 : -1;
        if(fabs(diff)>170*pi/180.0) {
            double hy=sin(s->angle+pi/2),hx=cos(s->angle+pi/2);
            dir=hy < -1e-9 ? 1 : hy>1e-9 ? -1 : hx>0 ? 1 : -1;
        }
        s->angle=fabs(diff)<=step ? target : atan2(sin(s->angle+dir*step),cos(s->angle+dir*step));
    } else s->angle=target;
    s->active=true;s->at=now;
    double cx=cos(s->angle),cy=sin(s->angle);
    double scale=walking ? 127*.6 : circular ? 127 : 127/fmax(fabs(cx),fabs(cy));
    *out_x=(int)lround(cx*scale);*out_y=(int)lround(cy*scale);
}
#endif
